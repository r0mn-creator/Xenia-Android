/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_emitter.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include "xenia/base/assert.h"
#include "xenia/base/debugging.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/memory.h"
#include "xenia/cpu/backend/arm64/arm64_backend.h"
#include "xenia/cpu/backend/arm64/arm64_code_cache.h"
#include "xenia/cpu/backend/arm64/arm64_op.h"
#include "xenia/cpu/backend/arm64/arm64_sequences.h"
#include "xenia/cpu/backend/arm64/arm64_stack_layout.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/cpu/function.h"
#include "xenia/cpu/function_debug_info.h"
#include "xenia/cpu/hir/hir_builder.h"
#include "xenia/cpu/hir/value.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

// Dolphin Arm64Gen
using namespace Arm64Gen;

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

// ---------------------------------------------------------------------------
// Register maps
// ---------------------------------------------------------------------------

// Maps HIR GPR index [0..6] → AArch64 register number.
// We use x21–x27: all callee-saved, no special meaning to the OS.
const uint32_t ARM64Emitter::gpr_reg_map_[ARM64Emitter::GPR_COUNT] = {
    21, 22, 23, 24, 25, 26, 27
};

// Maps HIR vector index [0..11] → NEON register number.
// v4–v15: v4–v7 caller-saved, v8–v15 callee-saved.
const uint32_t ARM64Emitter::vec_reg_map_[ARM64Emitter::VEC_COUNT] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

// Dedicated registers (not in the allocatable sets above):
// x19 = PPC context pointer
// x20 = host memory base
static const ARM64Reg kContextReg = ARM64Reg::X19;
static const ARM64Reg kMembaseReg = ARM64Reg::X20;

// Scratch registers (caller-saved, safe to clobber within a sequence):
static const ARM64Reg kScratch0 = ARM64Reg::X9;
static const ARM64Reg kScratch1 = ARM64Reg::X10;
static const ARM64Reg kScratch2 = ARM64Reg::X11;
static const ARM64Reg kScratchV0 = ARM64Reg::Q16;
static const ARM64Reg kScratchV1 = ARM64Reg::Q17;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

// Constructor: allocates the JIT code buffer and detects optional CPU features.
ARM64Emitter::ARM64Emitter(ARM64Backend* backend, size_t block_size)
    : processor_(backend->processor()),
      backend_(backend),
      code_cache_(backend->code_cache()),
      fe_(this) {
  AllocCodeSpace(block_size);
  // Detect optional CPU features.
  // On Android, hwcap detection was done in the backend; here we just
  // read the flags that were set there.
  // For now assume baseline ARMv8.0-A (all modern Android devices).
  feature_flags_ = 0;
}

ARM64Emitter::~ARM64Emitter() = default;

// DIAGNOSTIC: a tiny ring of the most-recently-entered guest function
// addresses, written by each function's prolog. On the stack-overflow crash the
// exception handler dumps this to reveal the recursion cycle. C linkage so
// emulator.cc can read it without name mangling.
extern "C" {
uint32_t g_arm64_last_fn = 0;       // last guest function entered
uint32_t g_arm64_last_fn_prev = 0;  // the one before it (to spot a 2-cycle)
// Ring buffer of the last 16 guest-function prologs entered, to reveal the
// exact recursion cycle (which calls grow the stack without returning).
uint32_t g_arm64_fn_ring[16] = {0};
uint32_t g_arm64_fn_ring_pos = 0;
// Last CALL_POSSIBLE_RETURN compare: the branch target vs the saved
// GUEST_RET_ADDR slot. If these differ when they should match, a guest 'blr'
// return is being mis-resolved as a fresh call (→ infinite recursion).
uint32_t g_arm64_ci_target = 0;
uint32_t g_arm64_ci_saved = 0;
uint32_t g_arm64_ci_matches = 0;    // times the return was detected (took epilog)
uint32_t g_arm64_ci_misses = 0;     // times it fell through to resolve+call
}
// Address of the function currently being compiled (used by the prolog).
static uint32_t s_trace_fn_addr = 0;
// Name of the opcode currently being emitted (for reg()-on-constant diagnostics).
const char* g_arm64_cur_op = nullptr;
// Emitter currently emitting on this thread (translators can run on multiple
// threads concurrently) — used by the const-operand fallback below.
thread_local ARM64Emitter* g_arm64_cur_emitter = nullptr;

// Fallback for sequences that call .reg() on a constant operand they have no
// explicit is_constant handling for. ~150 sequences only handle the register
// case; the HIR constant-propagation pass usually folds constants away, but
// stragglers reach the backend and used to die one opcode per run on the
// assert in arm64_op.h (rotate_left, sub.F64, ...). Instead, materialize the
// constant into a RESERVED register at the current emission point and hand
// that back. x14/x15 + q20/q21 are untouched by the register allocator
// (GPRs x21-x27, vecs v4-v15), all sequences (scratch = x9-x13, q16-q19),
// and the prolog/thunks — and we deliberately avoid LoadConstantF32/F64/V128,
// which build values through x9/x10 (a sequence may already hold live data
// there; that exact clobber was the stvx-crash root cause). X16 (the
// intra-procedure-call temp) is the bit-pattern staging register instead.
// Two regs per class, round-robin, so src1 AND src2 can both be constants.
ARM64Reg Arm64MaterializeConstFallback(const hir::Value* v) {
  ARM64Emitter* e = g_arm64_cur_emitter;
  assert_not_null(e);
  static std::atomic<int> s_logs{0};
  if (s_logs.fetch_add(1, std::memory_order_relaxed) < 50) {
    XELOGW("ARM64 const-fallback op={} type={}",
           g_arm64_cur_op ? g_arm64_cur_op : "?", static_cast<int>(v->type));
  }
  static thread_local unsigned gpr_rr = 0;
  static thread_local unsigned vec_rr = 0;
  switch (v->type) {
    case hir::INT8_TYPE: {
      ARM64Reg r = (gpr_rr++ & 1) ? ARM64Reg::W15 : ARM64Reg::W14;
      e->MOVI2R(r, static_cast<uint64_t>(static_cast<uint8_t>(v->constant.i8)));
      return r;
    }
    case hir::INT16_TYPE: {
      ARM64Reg r = (gpr_rr++ & 1) ? ARM64Reg::W15 : ARM64Reg::W14;
      e->MOVI2R(r,
                static_cast<uint64_t>(static_cast<uint16_t>(v->constant.i16)));
      return r;
    }
    case hir::INT32_TYPE: {
      ARM64Reg r = (gpr_rr++ & 1) ? ARM64Reg::W15 : ARM64Reg::W14;
      e->MOVI2R(r,
                static_cast<uint64_t>(static_cast<uint32_t>(v->constant.i32)));
      return r;
    }
    case hir::INT64_TYPE: {
      ARM64Reg r = (gpr_rr++ & 1) ? ARM64Reg::X15 : ARM64Reg::X14;
      e->MOVI2R(r, static_cast<uint64_t>(v->constant.i64));
      return r;
    }
    case hir::FLOAT32_TYPE: {
      ARM64Reg r = (vec_rr++ & 1) ? ARM64Reg::Q21 : ARM64Reg::Q20;
      uint32_t bits;
      std::memcpy(&bits, &v->constant.f32, 4);
      e->MOVI2R(ARM64Reg::W16, static_cast<uint64_t>(bits));
      ARM64FloatEmitter fe(e);
      fe.FMOV(EncodeRegToSingle(r), ARM64Reg::W16);
      return r;
    }
    case hir::FLOAT64_TYPE: {
      ARM64Reg r = (vec_rr++ & 1) ? ARM64Reg::Q21 : ARM64Reg::Q20;
      uint64_t bits;
      std::memcpy(&bits, &v->constant.f64, 8);
      e->MOVI2R(ARM64Reg::X16, bits);
      ARM64FloatEmitter fe(e);
      fe.FMOV(EncodeRegToDouble(r), ARM64Reg::X16);
      return r;
    }
    case hir::VEC128_TYPE: {
      ARM64Reg r = (vec_rr++ & 1) ? ARM64Reg::Q21 : ARM64Reg::Q20;
      uint64_t lo, hi;
      std::memcpy(&lo, &v->constant.v128.low, 8);
      std::memcpy(&hi, &v->constant.v128.high, 8);
      ARM64FloatEmitter fe(e);
      e->MOVI2R(ARM64Reg::X16, lo);
      fe.INS(8, EncodeRegToQuad(r), 0, ARM64Reg::X16);
      e->MOVI2R(ARM64Reg::X16, hi);
      fe.INS(8, EncodeRegToQuad(r), 1, ARM64Reg::X16);
      return r;
    }
    default:
      assert_always("const fallback: unhandled type");
      return ARM64Reg::X14;
  }
}

// ---------------------------------------------------------------------------
// Store-watch: record every 32-bit guest store whose target guest address falls
// in [g_store_watch_lo, g_store_watch_hi]. Used to find which instruction
// corrupts a specific heap free-list LIST_ENTRY. emulator.cc dumps the ring at
// a fault. Set the range to enable; lo==hi (or lo>hi) disables.
extern "C" {
uint32_t g_store_watch_lo = 0x82B9CE80;
uint32_t g_store_watch_hi = 0x82B9CEA0;
struct XeStoreWatchEntry {
  uint32_t addr;   // guest store target (incl. offset)
  uint32_t value;  // value as written to memory (byte-swapped if the store was)
  uint32_t fn;     // guest function being executed
  uint32_t seq;    // monotonically increasing
};
XeStoreWatchEntry g_store_watch_ring[128] = {};
uint32_t g_store_watch_pos = 0;
}
// DIAGNOSTIC: capture the incoming return-address (caller) and guest r4 the
// first time function 0x9203FA88 is entered (it inserts a NULL block into the
// heap free list — we want to know who passed the bad pointer).
extern "C" {
// Ring of the last 8 entries to 0x9203FA88: caller (x2 = incoming return addr,
// reliable), and guest r3(heap)/r4(block) from context (may be stale).
uint32_t g_fa88_caller_ring[8] = {0};
uint32_t g_fa88_r3_ring[8] = {0};
uint32_t g_fa88_r4_ring[8] = {0};
uint32_t g_fa88_pos = 0;
// Guest reg snapshot (from context) captured at the stwx 0x9203FBC8 store.
uint32_t g_fbc8_r4 = 0;
uint32_t g_fbc8_r25 = 0;
uint32_t g_fbc8_r28 = 0;
uint32_t g_fbc8_r31 = 0;
uint32_t g_fbc8_storeval = 0xDEADBEEF;  // actual value stored by stwx @9203FBC8
uint32_t g_fa88_local_ops = 0;          // # LOAD/STORE_LOCAL emitted in 9203FA88
uint32_t g_fbc8_xregs[7] = {0};         // live host x21..x27 at the FBC8 store
// Address of the guest function currently being emitted (for diagnostics in
// sequence files, which can't see the file-static s_trace_fn_addr).
uint32_t g_emit_fn_addr = 0;
}
// Called from emitted code after each instrumented 32-bit store.
static void XeStoreWatchRecord(uint64_t guest_addr, uint32_t value, uint32_t fn) {
  uint32_t a = static_cast<uint32_t>(guest_addr);
  if (g_store_watch_lo >= g_store_watch_hi) return;
  if (a < g_store_watch_lo || a >= g_store_watch_hi) return;
  uint32_t p = g_store_watch_pos++;
  g_store_watch_ring[p & 127] = {a, value, fn, p};
}

// ---------------------------------------------------------------------------
// Main emit entry point
// ---------------------------------------------------------------------------

// DIAGNOSTIC watchdog: every second, log the last guest function entered
// (g_arm64_last_fn, frozen if a thread is stuck in a tight intra-function spin)
// plus the recent-entry ring. If last_fn stays constant while CPU is pinned, that
// function holds the spin loop.
static void ARM64SpinWatchdog() {
  uint32_t prev = 0xFFFFFFFFu;
  int same = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint32_t cur = g_arm64_last_fn;
    if (cur == prev) {
      ++same;
    } else {
      same = 0;
      prev = cur;
    }
    char ring[160];
    int n = 0;
    uint32_t pos = g_arm64_fn_ring_pos;
    for (int k = 0; k < 16 && n < (int)sizeof(ring) - 10; ++k) {
      uint32_t v = g_arm64_fn_ring[(pos - 1 - k) & 15];
      n += std::snprintf(ring + n, sizeof(ring) - n, "%08X ", v);
    }
    if (same >= 2) XELOGE("SPINWATCH last_fn={:08X} prev={:08X} stuck_s={} ring(newest->old): {}",
           cur, g_arm64_last_fn_prev, same, ring);
    // While the main thread is wedged in the 0x826D16D8 spin-lock acquire, dump
    // the 12-entry guest lock table it spins on (0x82FF2C00..) so we can tell
    // whether the lock word is genuinely nonzero (holder never releases) or
    // zero (our stwcx./CAS lowering is broken). Guest membase is 0x100000000.
    if (same >= 3) {
      const volatile uint32_t* tbl = reinterpret_cast<const volatile uint32_t*>(
          0x100000000ull + 0x82FF2C00ull);
      char locks[200];
      int ln = 0;
      for (int k = 0; k < 12 && ln < (int)sizeof(locks) - 12; ++k) {
        uint32_t be = tbl[k];
        // Values are guest big-endian; swap for display.
        uint32_t v = __builtin_bswap32(be);
        ln += std::snprintf(locks + ln, sizeof(locks) - ln, "%08X ", v);
      }
      XELOGE("SPINWATCH locktbl@82FF2C00: {}", locks);
    }
  }
}

// Compiles one guest function to native ARM64 machine code, places it in the
// code cache, and returns its address and size via the out-parameters.
bool ARM64Emitter::Emit(GuestFunction* function, hir::HIRBuilder* builder,
                         uint32_t debug_info_flags, FunctionDebugInfo* debug_info,
                         void** out_code_address, size_t* out_code_size,
                         std::vector<SourceMapEntry>* out_source_map) {
  static std::once_flag s_watchdog_once;
  std::call_once(s_watchdog_once, [] {
    std::thread(ARM64SpinWatchdog).detach();
  });
  s_trace_fn_addr = function->address();
  g_emit_fn_addr = function->address();
  // Reset the emitter's code buffer.
  ResetCodePtr();

  debug_info_ = debug_info;
  debug_info_flags_ = debug_info_flags;
  trace_data_ = &function->trace_data();
  source_map_arena_.Reset();

  // Emit the function body.
  EmitFunctionInfo func_info = {};
  if (!EmitFunction(builder, func_info)) {
    return false;
  }

  // Place the emitted code into the code cache.
  *out_code_address = Emplace(func_info, function);
  *out_code_size = func_info.code_size.total;

  // DIAGNOSTIC: dump emitted host machine code + the guest->host source map for
  // fn 0x8257F470 to a file so the mis-compiled `lwz r11,0x740(r31)` at guest
  // 0x8257F54C can be decoded offline.
  if (false) {
    XELOGE("HC: memset-region fn compiled at guest {:08X}", function->address());
  }
  if (false && *out_code_address) {
    FILE* fp = fopen(
        "/data/data/jp.xenia.emulator.github.debug/files/hc_82AA59D0.txt", "w");
    if (fp) {
      const uint32_t* w = reinterpret_cast<const uint32_t*>(*out_code_address);
      size_t n = *out_code_size / 4;
      fprintf(fp, "HOSTCODE 8257F470 base=%p words=%zu\n", *out_code_address, n);
      for (size_t k = 0; k < n; ++k)
        fprintf(fp, "%04zx %08X\n", k * 4, w[k]);
      fclose(fp);
    }
  }

  // Return source map.
  source_map_arena_.CloneContents(out_source_map);

  // DIAGNOSTIC: append the guest->host source map to the host-code dump file so
  // the host code for a specific guest pc (e.g. 0x8257F54C) can be located.
  if (false) {
    FILE* fp = fopen(
        "/data/data/jp.xenia.emulator.github.debug/files/sm_82AA59D0.txt", "w");
    if (fp) {
      for (auto& e : *out_source_map)
        fprintf(fp, "guest=%08X host=%04X\n", e.guest_address, e.code_offset);
      fclose(fp);
    }
  }

  return *out_code_address != nullptr;
}

// ---------------------------------------------------------------------------
// Function emission
// ---------------------------------------------------------------------------

// Lays out the HIR local slots, emits prolog, all basic blocks, and epilog;
// populates func_info with per-section sizes and the final stack frame size.
bool ARM64Emitter::EmitFunction(hir::HIRBuilder* builder,
                                 EmitFunctionInfo& func_info) {
  // Clear per-function block tracking state.
  block_labels_.clear();
  pending_block_fixups_.clear();
  epilog_fixups_.clear();

  // Assign each HIR local an ABSOLUTE byte offset from SP, placed ABOVE the
  // fixed frame region (GUEST_STACK_SIZE, which holds fp/lr, the context/return
  // home slots, and the saved callee regs at 0x58..0x150). Mirrors the x64
  // backend. Without this, LOAD_LOCAL/STORE_LOCAL would smash the saved
  // registers and overflow into the host caller's frame (wild jumps → crashes
  // in unrelated threads). The frame is then grown to fit all locals.
  size_t stack_offset = StackLayout::GUEST_STACK_SIZE;
  for (auto slot : builder->locals()) {
    size_t type_size = GetTypeSize(slot->type);
    stack_offset = xe::align(stack_offset, type_size);
    slot->set_constant(static_cast<uint32_t>(stack_offset));
    stack_offset += type_size;
  }
  size_t locals_size = xe::align(stack_offset - StackLayout::GUEST_STACK_SIZE,
                                 static_cast<size_t>(16));
  stack_size_ = StackLayout::GUEST_STACK_SIZE + locals_size;
  assert_true(stack_size_ % 16 == 0);

  // Record prolog start offset.
  size_t prolog_start = GetCodeOffset();
  EmitFunctionProlog(func_info);
  func_info.code_size.prolog = GetCodeOffset() - prolog_start;

  // ---------------------------------------------------------------------------
  // Walk basic blocks and emit each instruction.
  // ---------------------------------------------------------------------------
  size_t body_start = GetCodeOffset();

  auto block = builder->first_block();
  while (block) {
    // Record the start address of this block so backward branches can reach it
    // and forward references can be patched.
    block_labels_[block] = this->GetCodePtr();

    // Resolve any forward-branch fixups targeting this block.
    auto it = pending_block_fixups_.find(block);
    if (it != pending_block_fixups_.end()) {
      for (auto& fb : it->second) {
        SetJumpTarget(fb);
      }
      pending_block_fixups_.erase(it);
    }

    auto instr = block->instr_head;
    while (instr) {
      current_instr_ = instr;
      g_arm64_cur_op = instr->opcode->name;
      g_arm64_cur_emitter = this;  // for the const-operand fallback
      const hir::Instr* new_tail = nullptr;
      if (!SelectSequence(this, instr, &new_tail)) {
        // No handler found — this is a fatal error.
        InstrKey diag_key(instr);
        XELOGE(
            "ARM64Emitter: no sequence for opcode {:d} ({}) key={:08X} "
            "dest={:d} src1={:d} src2={:d} src3={:d} table_size={:d}",
            static_cast<int>(instr->opcode->num), instr->opcode->name,
            diag_key.value, diag_key.dest, diag_key.src1, diag_key.src2,
            diag_key.src3, static_cast<int>(sequence_table().size()));
        UnimplementedInstr(instr);
      }
      instr = new_tail ? new_tail->next : instr->next;
    }

    block = block->next;
  }

  func_info.code_size.body = GetCodeOffset() - body_start;

  // ---------------------------------------------------------------------------
  // Epilog
  // ---------------------------------------------------------------------------
  size_t epilog_start = GetCodeOffset();
  SetEpilogLabel();
  EmitFunctionEpilog();
  func_info.code_size.epilog = GetCodeOffset() - epilog_start;

  func_info.code_size.total = GetCodeOffset();
  func_info.stack_size = stack_size_;

  return true;
}

// ---------------------------------------------------------------------------
// Prolog: save callee-saved registers, set up frame, load context/membase
// ---------------------------------------------------------------------------

// Allocate the current frame (stack_size_ bytes) and save fp/lr at [sp,#0].
// Uses the pre-index STP fast path for small frames and an explicit SUB for
// frames beyond the pre-index immediate range (±504).
void ARM64Emitter::EmitAllocFrame() {
  if (stack_size_ <= 504) {
    STP(IndexType::Pre, ARM64Reg::X29, ARM64Reg::X30,
        ARM64Reg::SP, -(int32_t)stack_size_);
  } else {
    if (stack_size_ <= 4095) {
      SUB(ARM64Reg::SP, ARM64Reg::SP, (u32)stack_size_);
    } else {
      SUBI2R(ARM64Reg::SP, ARM64Reg::SP, stack_size_, ARM64Reg::X17);
    }
    STP(IndexType::Signed, ARM64Reg::X29, ARM64Reg::X30, ARM64Reg::SP, 0);
  }
}

// Restore fp/lr and deallocate the current frame.
void ARM64Emitter::EmitFreeFrame() {
  if (stack_size_ <= 504) {
    LDP(IndexType::Post, ARM64Reg::X29, ARM64Reg::X30,
        ARM64Reg::SP, (int32_t)stack_size_);
  } else {
    LDP(IndexType::Signed, ARM64Reg::X29, ARM64Reg::X30, ARM64Reg::SP, 0);
    if (stack_size_ <= 4095) {
      ADD(ARM64Reg::SP, ARM64Reg::SP, (u32)stack_size_);
    } else {
      ADDI2R(ARM64Reg::SP, ARM64Reg::SP, stack_size_, ARM64Reg::X17);
    }
  }
}

// Emits the full function prolog: frame allocation, callee-save spills,
// context/membase load, return-address setup, and diagnostic entry probes.
void ARM64Emitter::EmitFunctionProlog(const EmitFunctionInfo& func_info) {
  // ARM64 ABI: stack must be 16-byte aligned at all BL instructions.
  // stack_size_ was computed by EmitFunction (fixed frame + locals area).
  assert_true(stack_size_ % 16 == 0);

  // Allocate the frame and save fp/lr. EmitAllocFrame handles frames larger
  // than the pre-index STP immediate range.
  EmitAllocFrame();

  // MOV x29, sp  (set up frame pointer)
  MOV(ARM64Reg::X29, ARM64Reg::SP);

  // Save the allocatable callee-saved registers (x21-x27 and v4-v15). The
  // shared register allocator hands these to guest registers and assumes they
  // survive guest-to-guest calls WITHOUT spilling them around a Call, so each
  // guest function must preserve them like a normal AAPCS64 callee. (Omitting
  // this silently corrupts a register-heavy caller's guest values when it calls
  // another guest function.)
  STP(IndexType::Signed, ARM64Reg::X21, ARM64Reg::X22,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 0));
  STP(IndexType::Signed, ARM64Reg::X23, ARM64Reg::X24,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 16));
  STP(IndexType::Signed, ARM64Reg::X25, ARM64Reg::X26,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 32));
  STR(IndexType::Unsigned, ARM64Reg::X27,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 48));
  for (int v = 4; v <= 15; ++v) {
    int off = (int)StackLayout::GUEST_SAVED_VEC_BASE + (v - 4) * 16;
    STR(IndexType::Unsigned, ARM64Reg::Q0 + v, ARM64Reg::SP, (s32)off);
  }

  // x19 is the guest context register (PPCContext*). It is established by the
  // host-to-guest thunk and is callee-saved (x19-x28), so it is preserved
  // across guest-to-guest calls — every guest function receives the context in
  // x19 already. We must NOT reload it from x1: only the thunk-called entry
  // function happens to also have the context in x1; a guest-to-guest Call()
  // does not set x1, so reloading would corrupt the context for callees.
  //
  // Spill context to its home slot so ReloadContext() can restore it after the
  // register is temporarily reused.
  STR(IndexType::Unsigned, ARM64Reg::X19,
      ARM64Reg::SP, (s32)StackLayout::GUEST_CTX_HOME);

  // Load memory base from context (PPCContext::virtual_membase at +0x8).
  static const int kVirtualMembaseOffset =
      offsetof(ppc::PPCContext, virtual_membase);
  LDR(IndexType::Unsigned, ARM64Reg::X20,
      ARM64Reg::X19, kVirtualMembaseOffset);

  // Return-address bookkeeping (mirrors the x64 backend's rcx handling).
  // The caller — or the host-to-guest thunk for the entry point — passes this
  // function's PPC return address in x2. Save it into GUEST_RET_ADDR so a later
  // guest 'blr' can be recognized as a return by CallIndirect instead of being
  // resolved as a fresh call (which would recurse forever and overflow the host
  // stack). Clear the outgoing call-return slot.
  STR(IndexType::Unsigned, ARM64Reg::X2,
      ARM64Reg::SP, (s32)StackLayout::GUEST_RET_ADDR);
  STR(IndexType::Unsigned, ARM64Reg::ZR,
      ARM64Reg::SP, (s32)StackLayout::GUEST_CALL_RET_ADDR);

  // DIAGNOSTIC: record (prev, last) entered guest function addresses. x9/x10/x11
  // are caller-saved scratch and unused at this point in the prolog.
  MOVI2R(ARM64Reg::X9, reinterpret_cast<uint64_t>(&g_arm64_last_fn));
  LDR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X10), ARM64Reg::X9, 0);
  MOVI2R(ARM64Reg::X11, reinterpret_cast<uint64_t>(&g_arm64_last_fn_prev));
  STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X10), ARM64Reg::X11, 0);
  MOVI2R(ARM64Reg::X10, s_trace_fn_addr);
  STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X10), ARM64Reg::X9, 0);

  // DIAGNOSTIC: append this function's address to the ring buffer:
  //   pos = g_arm64_fn_ring_pos; ring[pos & 15] = addr; pos = pos + 1;
  MOVI2R(ARM64Reg::X9, reinterpret_cast<uint64_t>(&g_arm64_fn_ring_pos));
  LDR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X11), ARM64Reg::X9, 0);
  MOVI2R(ARM64Reg::X10, reinterpret_cast<uint64_t>(&g_arm64_fn_ring[0]));
  AND(EncodeRegTo32(ARM64Reg::X12), EncodeRegTo32(ARM64Reg::X11),
      (uint64_t)0xF);
  ADD(ARM64Reg::X10, ARM64Reg::X10, ARM64Reg::X12,
      ArithOption(ARM64Reg::X12, ShiftType::LSL, 2));
  MOVI2R(ARM64Reg::X13, s_trace_fn_addr);
  STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X13), ARM64Reg::X10, 0);
  ADD(EncodeRegTo32(ARM64Reg::X11), EncodeRegTo32(ARM64Reg::X11), 1);
  STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X11), ARM64Reg::X9, 0);

  // DIAGNOSTIC: on every entry to the gated function, record the caller
  // (x2 = incoming return addr) and guest r3/r4 into an 8-deep ring.
  // RETARGETED to 0x82A4EE78 (vtbl[0] scalar-dtor WRAPPER of the audio
  // singleton at [0x82B9CE90]; the body trace only saw the wrapper as caller
  // — now capture the actual virtual-call site that tears it down early).
  if (s_trace_fn_addr == 0x82A4EE78u) {
    // idx = g_fa88_pos & 7
    MOVI2R(ARM64Reg::X9, reinterpret_cast<uint64_t>(&g_fa88_pos));
    LDR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X11), ARM64Reg::X9, 0);
    AND(EncodeRegTo32(ARM64Reg::X12), EncodeRegTo32(ARM64Reg::X11),
        (uint64_t)0x7);
    // caller_ring[idx] = x2
    MOVI2R(ARM64Reg::X10, reinterpret_cast<uint64_t>(&g_fa88_caller_ring[0]));
    ADD(ARM64Reg::X10, ARM64Reg::X10, ARM64Reg::X12,
        ArithOption(ARM64Reg::X12, ShiftType::LSL, 2));
    STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X2), ARM64Reg::X10, 0);
    // r3_ring[idx] = context->r[3]
    LDR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X13), GetContextReg(),
        (s32)(offsetof(ppc::PPCContext, r) + 3 * sizeof(uint64_t)));
    MOVI2R(ARM64Reg::X10, reinterpret_cast<uint64_t>(&g_fa88_r3_ring[0]));
    ADD(ARM64Reg::X10, ARM64Reg::X10, ARM64Reg::X12,
        ArithOption(ARM64Reg::X12, ShiftType::LSL, 2));
    STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X13), ARM64Reg::X10, 0);
    // r4_ring[idx] = context->r[4]
    LDR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X13), GetContextReg(),
        (s32)(offsetof(ppc::PPCContext, r) + 4 * sizeof(uint64_t)));
    MOVI2R(ARM64Reg::X10, reinterpret_cast<uint64_t>(&g_fa88_r4_ring[0]));
    ADD(ARM64Reg::X10, ARM64Reg::X10, ARM64Reg::X12,
        ArithOption(ARM64Reg::X12, ShiftType::LSL, 2));
    STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X13), ARM64Reg::X10, 0);
    ADD(EncodeRegTo32(ARM64Reg::X11), EncodeRegTo32(ARM64Reg::X11), 1);
    STR(IndexType::Unsigned, EncodeRegTo32(ARM64Reg::X11), ARM64Reg::X9, 0);
  }
}

// ---------------------------------------------------------------------------
// Epilog: restore registers and return to host
// ---------------------------------------------------------------------------

// Restores all allocatable callee-saved GPRs (x21-x27) and VECs (v4-v15)
// from their stack home slots written in the prolog.
void ARM64Emitter::EmitRestoreCalleeSaved() {
  // Restore the allocatable callee-saved registers saved in the prolog.
  LDP(IndexType::Signed, ARM64Reg::X21, ARM64Reg::X22,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 0));
  LDP(IndexType::Signed, ARM64Reg::X23, ARM64Reg::X24,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 16));
  LDP(IndexType::Signed, ARM64Reg::X25, ARM64Reg::X26,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 32));
  LDR(IndexType::Unsigned, ARM64Reg::X27,
      ARM64Reg::SP, (s32)(StackLayout::GUEST_SAVED_GPR_BASE + 48));
  for (int v = 4; v <= 15; ++v) {
    int off = (int)StackLayout::GUEST_SAVED_VEC_BASE + (v - 4) * 16;
    LDR(IndexType::Unsigned, ARM64Reg::Q0 + v, ARM64Reg::SP, (s32)off);
  }
}

// Emits the function epilog: restores context, callee-saved regs, frame, RET.
void ARM64Emitter::EmitFunctionEpilog() {
  // Restore x19 from home slot.
  LDR(IndexType::Unsigned, ARM64Reg::X19,
      ARM64Reg::SP, (s32)StackLayout::GUEST_CTX_HOME);

  EmitRestoreCalleeSaved();

  // Restore fp/lr and deallocate the frame (handles large frames).
  EmitFreeFrame();

  RET();
}

// ---------------------------------------------------------------------------
// Thunk emission
// ---------------------------------------------------------------------------

// Emits the host→guest entry thunk: saves all AAPCS64 callee-saved regs,
// loads context (x1) and return-address (x2), and calls the guest entry point.
bool ARM64Emitter::EmitHostToGuestThunk() {
  host_to_guest_thunk_ptr_ =
      reinterpret_cast<HostToGuestThunk>(this->GetWritableCodePtr());

  size_t thunk_size = StackLayout::THUNK_STACK_SIZE;

  // --- Prolog ---
  STP(IndexType::Pre, ARM64Reg::X29, ARM64Reg::X30,
      ARM64Reg::SP, -(int32_t)thunk_size);
  MOV(ARM64Reg::X29, ARM64Reg::SP);

  // Save x19–x28 (callee-saved GPRs).
  STP(IndexType::Signed, ARM64Reg::X19, ARM64Reg::X20,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x19));
  STP(IndexType::Signed, ARM64Reg::X21, ARM64Reg::X22,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x21));
  STP(IndexType::Signed, ARM64Reg::X23, ARM64Reg::X24,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x23));
  STP(IndexType::Signed, ARM64Reg::X25, ARM64Reg::X26,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x25));
  STP(IndexType::Signed, ARM64Reg::X27, ARM64Reg::X28,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x27));

  // Save lower 64 bits of d8–d15 (callee-saved NEON).
  for (int i = 8; i <= 15; i++) {
    int offset = (int)offsetof(StackLayout::Thunk, d8) + (i - 8) * 8;
    STR(IndexType::Unsigned, ARM64Reg::D0 + i, ARM64Reg::SP, (s32)offset);
  }

  // Save x0 (guest function pointer) to arg_temp[0].
  STR(IndexType::Unsigned, ARM64Reg::X0,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, arg_temp));

  // Set up guest state.
  // x1 = PPCContext* (the guest context register). PPCContext has
  // thread_state at +0x0 and virtual_membase at +0x8.
  MOV(ARM64Reg::X19, ARM64Reg::X1);
  static const int kVirtualMembaseOffset =
      offsetof(ppc::PPCContext, virtual_membase);
  LDR(IndexType::Unsigned, ARM64Reg::X20,
      ARM64Reg::X19, kVirtualMembaseOffset);

  // Branch to the guest function.
  BLR(ARM64Reg::X0);

  // --- Epilog ---
  for (int i = 8; i <= 15; i++) {
    int offset = (int)offsetof(StackLayout::Thunk, d8) + (i - 8) * 8;
    LDR(IndexType::Unsigned, ARM64Reg::D0 + i, ARM64Reg::SP, (s32)offset);
  }

  LDP(IndexType::Signed, ARM64Reg::X19, ARM64Reg::X20,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x19));
  LDP(IndexType::Signed, ARM64Reg::X21, ARM64Reg::X22,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x21));
  LDP(IndexType::Signed, ARM64Reg::X23, ARM64Reg::X24,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x23));
  LDP(IndexType::Signed, ARM64Reg::X25, ARM64Reg::X26,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x25));
  LDP(IndexType::Signed, ARM64Reg::X27, ARM64Reg::X28,
      ARM64Reg::SP, (s32)offsetof(StackLayout::Thunk, x27));

  LDP(IndexType::Post, ARM64Reg::X29, ARM64Reg::X30,
      ARM64Reg::SP, (int32_t)thunk_size);
  RET();
  return true;
}

// Emits the guest→host call thunk: saves x19/x20/lr, shifts arguments,
// calls the C++ handler, then restores registers and returns.
bool ARM64Emitter::EmitGuestToHostThunk() {
  guest_to_host_thunk_ptr_ =
      reinterpret_cast<GuestToHostThunk>(this->GetWritableCodePtr());

  // Called as thunk(target, arg0, arg1) with x0=target, x1=arg0, x2=arg1.
  // The host-side handlers all take (PPCContext* ctx, arg0, arg1), so — exactly
  // like the x64 backend — we move the target out of the way and set the first
  // argument (x0) to the guest context register (x19) before calling. arg0/arg1
  // in x1/x2 are passed through untouched.
  // Save x19/x20 AND the incoming return address (x30). The BLR below clobbers
  // x30; without preserving it here, the final RET would branch back to its own
  // LDP (since x30 == address-after-BLR), turning the thunk into an infinite
  // loop whose post-indexed LDP walks SP up into the stack guard page.
  STP(IndexType::Pre, ARM64Reg::X19, ARM64Reg::X20, ARM64Reg::SP, -16);
  STR(IndexType::Pre, ARM64Reg::X30, ARM64Reg::SP, -16);
  MOV(ARM64Reg::X16, ARM64Reg::X0);   // x16 = target
  MOV(ARM64Reg::X0, ARM64Reg::X19);   // x0 = PPCContext* (context register)
  BLR(ARM64Reg::X16);
  LDR(IndexType::Post, ARM64Reg::X30, ARM64Reg::SP, 16);
  LDP(IndexType::Post, ARM64Reg::X19, ARM64Reg::X20, ARM64Reg::SP, 16);
  RET();
  return true;
}

// Emits a stub thunk that, at runtime, calls Processor::ResolveFunction for a
// guest address and then branches to the resolved host code.
bool ARM64Emitter::EmitResolveFunctionThunk() {
  resolve_function_thunk_ptr_ =
      reinterpret_cast<ResolveFunctionThunk>(this->GetWritableCodePtr());

  STP(IndexType::Pre, ARM64Reg::X19, ARM64Reg::X20, ARM64Reg::SP, -32);
  STR(IndexType::Unsigned, ARM64Reg::X30, ARM64Reg::SP, 16);

  // x0 = guest PC; x19 = context; load Processor* from context.
  static const int kProcessorOffset = 0;  // TODO: verify
  LDR(IndexType::Unsigned, ARM64Reg::X1, ARM64Reg::X19, kProcessorOffset);

  // Placeholder: call address 0 — will trap until properly wired up.
  MOVI2R(ARM64Reg::X2, (u64)0);
  BLR(ARM64Reg::X2);

  LDR(IndexType::Unsigned, ARM64Reg::X30, ARM64Reg::SP, 16);
  LDP(IndexType::Post, ARM64Reg::X19, ARM64Reg::X20, ARM64Reg::SP, 32);
  BR(ARM64Reg::X0);
  return true;
}

// ---------------------------------------------------------------------------
// Code placement
// ---------------------------------------------------------------------------

// Places the emitted machine code into the code cache and returns the
// executable address (distinct from write address on WXor systems).
void* ARM64Emitter::Emplace(const EmitFunctionInfo& func_info,
                             GuestFunction* function) {
  void* code_execute_address = nullptr;
  void* code_write_address = nullptr;

  size_t code_size = GetCodeOffset();

  // region_start_ = start of current function's code (set in ResetCodePtr).
  if (function) {
    code_cache_->PlaceGuestCode(
        function->address(), region_start_,
        func_info, function,
        code_execute_address, code_write_address);
  } else {
    code_cache_->PlaceHostCode(
        0, region_start_, func_info,
        code_execute_address, code_write_address);
  }

  return code_execute_address;
}

// ---------------------------------------------------------------------------
// Register helpers
// ---------------------------------------------------------------------------

// Returns the fixed context register (x19 = PPCContext*).
ARM64Reg ARM64Emitter::GetContextReg() const { return kContextReg; }
// Returns the fixed memory-base register (x20 = virtual_membase).
ARM64Reg ARM64Emitter::GetMembaseReg() const { return kMembaseReg; }

// Reloads x19 (context) from its stack home slot (used after a host call).
void ARM64Emitter::ReloadContext() {
  LDR(IndexType::Unsigned, ARM64Reg::X19,
      ARM64Reg::SP, (s32)StackLayout::GUEST_CTX_HOME);
}

// Reloads x20 (membase) from PPCContext::virtual_membase (used after context reload).
void ARM64Emitter::ReloadMembase() {
  static const int kVirtualMembaseOffset =
      offsetof(ppc::PPCContext, virtual_membase);
  LDR(IndexType::Unsigned, ARM64Reg::X20,
      ARM64Reg::X19, kVirtualMembaseOffset);
}

// Returns the 64-bit host register allocated for HIR value v's GPR slot.
// static
ARM64Reg ARM64Emitter::GetGPR64(const hir::Value* v) {
  return ARM64Reg::X0 + (int)gpr_reg_map_[v->reg.index];
}
// Returns the 32-bit view of the host register for HIR value v's GPR slot.
ARM64Reg ARM64Emitter::GetGPR32(const hir::Value* v) {
  return ARM64Reg::W0 + (int)gpr_reg_map_[v->reg.index];
}
// Returns the 16-bit view (encoded as W) of the host register for v's GPR slot.
ARM64Reg ARM64Emitter::GetGPR16(const hir::Value* v) {
  return ARM64Reg::W0 + (int)gpr_reg_map_[v->reg.index];
}
// Returns the 8-bit view (encoded as W) of the host register for v's GPR slot.
ARM64Reg ARM64Emitter::GetGPR8(const hir::Value* v) {
  return ARM64Reg::W0 + (int)gpr_reg_map_[v->reg.index];
}
// Returns the 128-bit NEON register (Qn) allocated for HIR value v's VEC slot.
ARM64Reg ARM64Emitter::GetVec(const hir::Value* v) {
  return ARM64Reg::Q0 + (int)vec_reg_map_[v->reg.index];
}

// Returns scratch GPR at position index (x9–x13); caller-saved, not guest regs.
// static
ARM64Reg ARM64Emitter::ScratchReg(int index) {
  static const ARM64Reg scratch_gprs[] = {
      ARM64Reg::X9, ARM64Reg::X10, ARM64Reg::X11,
      ARM64Reg::X12, ARM64Reg::X13
  };
  assert_true(index < 5);
  return scratch_gprs[index];
}

// Returns scratch NEON register at position index (q16–q19); caller-saved.
ARM64Reg ARM64Emitter::ScratchVec(int index) {
  static const ARM64Reg scratch_vecs[] = {
      ARM64Reg::Q16, ARM64Reg::Q17, ARM64Reg::Q18, ARM64Reg::Q19
  };
  assert_true(index < 4);
  return scratch_vecs[index];
}

// ---------------------------------------------------------------------------
// Constant loading
// ---------------------------------------------------------------------------

// Loads a 64-bit integer constant into dest using MOVZ/MOVK sequences.
void ARM64Emitter::LoadConstantI64(ARM64Reg dest, uint64_t value) {
  MOVI2R(dest, value);
}

// Loads a 32-bit integer constant into the 32-bit view of dest.
void ARM64Emitter::LoadConstantI32(ARM64Reg dest, uint32_t value) {
  MOVI2R(EncodeRegTo32(dest), (u64)value);
}

// Loads a 32-bit float constant into a NEON scalar register via a GPR scratch.
void ARM64Emitter::LoadConstantF32(ARM64Reg dest, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  MOVI2R(kScratch0, (u64)bits);
  ARM64FloatEmitter fe(this);
  fe.FMOV(EncodeRegToSingle(dest), EncodeRegTo32(kScratch0));
}

// Loads a 64-bit double constant into a NEON scalar register via a GPR scratch.
void ARM64Emitter::LoadConstantF64(ARM64Reg dest, double value) {
  uint64_t bits;
  std::memcpy(&bits, &value, 8);
  MOVI2R(kScratch0, bits);
  ARM64FloatEmitter fe(this);
  fe.FMOV(EncodeRegToDouble(dest), kScratch0);
}

// Loads a 128-bit vector constant into a NEON quad register via two GPR INS.
void ARM64Emitter::LoadConstantV128(ARM64Reg dest, const vec128_t& value) {
  uint64_t lo, hi;
  std::memcpy(&lo, &value.low, 8);
  std::memcpy(&hi, &value.high, 8);
  MOVI2R(kScratch0, lo);
  MOVI2R(kScratch1, hi);
  ARM64FloatEmitter fe(this);
  fe.INS(8, EncodeRegToQuad(dest), 0, kScratch0);
  fe.INS(8, EncodeRegToQuad(dest), 1, kScratch1);
}

// ---------------------------------------------------------------------------
// Native calls
// ---------------------------------------------------------------------------

// Emits an indirect BLR to an arbitrary host function (no arguments beyond ctx).
void ARM64Emitter::CallNative(void* fn) {
  MOVI2R(kScratch0, reinterpret_cast<uint64_t>(fn));
  MOV(ARM64Reg::X0, ARM64Reg::X19);
  BLR(kScratch0);
}

// Emits a call to a typed host function taking only the raw context pointer.
void ARM64Emitter::CallNative(uint64_t (*fn)(void* raw_context)) {
  MOVI2R(kScratch0, reinterpret_cast<uint64_t>(fn));
  MOV(ARM64Reg::X0, ARM64Reg::X19);
  BLR(kScratch0);
}

// Emits a call to a typed host function taking context + one 64-bit argument
// already in x1 (the caller sets x1 before this).
void ARM64Emitter::CallNative(uint64_t (*fn)(void* raw_context, uint64_t arg0)) {
  MOVI2R(kScratch0, reinterpret_cast<uint64_t>(fn));
  MOV(ARM64Reg::X0, ARM64Reg::X19);
  BLR(kScratch0);
}

// Emits a call to a typed host function with context + an explicit arg0 literal.
void ARM64Emitter::CallNative(uint64_t (*fn)(void* raw_context, uint64_t arg0),
                               uint64_t arg0) {
  MOVI2R(kScratch0, reinterpret_cast<uint64_t>(fn));
  MOV(ARM64Reg::X0, ARM64Reg::X19);
  MOVI2R(ARM64Reg::X1, arg0);
  BLR(kScratch0);
}

// ---------------------------------------------------------------------------
// Debug / trap
// ---------------------------------------------------------------------------

// Reads r3 as a guest string pointer and logs its contents; called for PPC
// trap type 20/26 (0x0FE00014 debug-print convention).
static uint64_t TrapDebugPrint(void* raw_context, uint64_t address) {
  auto thread_state = *reinterpret_cast<ThreadState**>(raw_context);
  uint32_t str_ptr = uint32_t(thread_state->context()->r[3]);
  auto str = thread_state->memory()->TranslateVirtual<const char*>(str_ptr);
  XELOGD("(DebugPrint) {}", str);
  return 0;
}

// Logs a forced-trap message and optionally breaks into the debugger; called
// for PPC trap types 0/22 (unconditional assert/break).
static uint64_t TrapDebugBreak(void* raw_context, uint64_t address) {
  XELOGE("tw/td forced trap hit! This should be a crash!");
  if (cvars::break_on_debugbreak) {
    xe::debugging::Break();
  }
  return 0;
}

// Emits a host BRK #0 to trigger an immediate debugger break in host code.
void ARM64Emitter::DebugBreak() {
  BRK(0);
}

// Emits the appropriate host code for a PPC `tw`/`td` trap instruction,
// dispatching on trap_type to debug-print, forced-break, NOP, or a warning.
void ARM64Emitter::Trap(uint16_t trap_type) {
  switch (trap_type) {
    case 20:
    case 26:
      // 0x0FE00014 debug print: r3 = buffer ptr, r4 = length
      CallNative(TrapDebugPrint, 0);
      break;
    case 0:
    case 22:
      // Unconditional trap (assert-style).
      CallNative(TrapDebugBreak, 0);
      break;
    case 25:
      // Benign / ignored on hardware.
      break;
    default:
      XELOGW("Unknown trap type {}", trap_type);
      break;
  }
}

// Logs the unimplemented opcode and emits a forced trap to halt execution.
void ARM64Emitter::UnimplementedInstr(const hir::Instr* i) {
  XELOGE("ARM64Emitter: unimplemented HIR opcode {:d}", (int)i->opcode->num);
  Trap(0xFFFF);
}

// Records a guest-PC → host-code-offset mapping entry for the source map.
void ARM64Emitter::MarkSourceOffset(const hir::Instr* i) {
  auto entry = source_map_arena_.Alloc<SourceMapEntry>();
  entry->guest_address = i->src1.offset;
  entry->code_offset = static_cast<uint32_t>(GetCodeOffset());
  current_guest_address_ = static_cast<uint32_t>(i->src1.offset);
}

// ---------------------------------------------------------------------------
// Epilog label
// ---------------------------------------------------------------------------

// Marks the current code position as the epilog entry and patches all pending
// epilog fixup branches to point here.
void ARM64Emitter::SetEpilogLabel() {
  epilog_offset_ = GetCodeOffset();
  // Patch all outstanding B() instructions that jump to the epilog.
  for (auto& fb : epilog_fixups_) {
    SetJumpTarget(fb);
  }
  epilog_fixups_.clear();
}

// Emits a forward branch placeholder to the epilog and registers it for patching.
FixupBranch ARM64Emitter::GetEpilogFixup() {
  // Emit an unconditional forward branch and register it for later patching.
  FixupBranch fb = B();
  epilog_fixups_.push_back(fb);
  return fb;
}

// ---------------------------------------------------------------------------
// Block label helpers
// ---------------------------------------------------------------------------

// Returns the emitted start address of block, or null if not yet emitted.
const uint8_t* ARM64Emitter::GetBlockAddress(hir::Block* block) const {
  auto it = block_labels_.find(block);
  return it != block_labels_.end() ? it->second : nullptr;
}

// Registers a forward-branch fixup to be patched when block is emitted.
void ARM64Emitter::AddBlockFixup(hir::Block* target, FixupBranch fb) {
  pending_block_fixups_[target].push_back(fb);
}

// Registers a forward-branch fixup to be patched at the epilog label.
void ARM64Emitter::AddEpilogFixup(FixupBranch fb) {
  epilog_fixups_.push_back(fb);
}

// Emits an unconditional branch to label's block (backward) or a forward fixup.
void ARM64Emitter::BranchLabel(hir::Label* label) {
  hir::Block* block = label->block;
  const uint8_t* addr = GetBlockAddress(block);
  if (addr) {
    B(addr);
  } else {
    auto fb = B();
    AddBlockFixup(block, fb);
  }
}

// Emits CBNZ cond→label; uses a fixup branch if the label isn't emitted yet.
void ARM64Emitter::BranchLabelIfNZ(ARM64Reg cond, hir::Label* label) {
  hir::Block* block = label->block;
  const uint8_t* addr = GetBlockAddress(block);
  if (addr) {
    CBNZ(cond, addr);
  } else {
    auto fb = CBNZ(cond);
    AddBlockFixup(block, fb);
  }
}

// Emits CBZ cond→label; uses a fixup branch if the label isn't emitted yet.
void ARM64Emitter::BranchLabelIfZ(ARM64Reg cond, hir::Label* label) {
  hir::Block* block = label->block;
  const uint8_t* addr = GetBlockAddress(block);
  if (addr) {
    CBZ(cond, addr);
  } else {
    auto fb = CBZ(cond);
    AddBlockFixup(block, fb);
  }
}

// ---------------------------------------------------------------------------
// Guest call helpers
// ---------------------------------------------------------------------------

// Resolves a PPC guest address to its compiled host code address.
// Called at runtime when the target isn't already compiled.
// Lock-free direct-mapped cache of resolved indirect-call targets.
// Every guest 'bctrl'/'blr'-not-detected lands in ARM64ResolveFunction, whose
// slow path (Processor::ResolveFunction -> EntryTable::GetOrCreate) takes the
// PROCESS-WIDE global critical-region mutex + a std::map::find on EVERY call.
// Guest init code does millions of indirect calls (e.g. an apply-callback loop
// zeroing a large heap), so that lock/lookup dominates and serializes against
// all other threads -> boot crawls at ~10k calls/s. Compiled guest functions
// never move during a run, so a (guest_address -> host_code) mapping is stable
// once known and can be cached forever without invalidation. This fast path
// turns the hot repeated target into two relaxed atomic loads, no lock.
namespace {
struct ResolveCacheEntry {
  std::atomic<uint32_t> addr;  // guest address (0 = empty; 0 is never a valid
                               // guest code address on the 360)
  std::atomic<uint64_t> code;  // resolved host machine_code pointer
};
constexpr uint32_t kResolveCacheBits = 16;
constexpr uint32_t kResolveCacheSize = 1u << kResolveCacheBits;
constexpr uint32_t kResolveCacheMask = kResolveCacheSize - 1;
ResolveCacheEntry g_resolve_cache[kResolveCacheSize];
inline uint32_t ResolveCacheSlot(uint32_t a) {
  return (a >> 2) & kResolveCacheMask;  // guest code is 4-byte aligned
}
}  // namespace

// Resolves a guest address to its compiled host code, using a lock-free
// direct-mapped cache before falling back to Processor::ResolveFunction.
static uint64_t ARM64ResolveFunction(void* raw_context, uint64_t guest_address) {
  // FAST PATH: lock-free cache hit. Re-check addr AFTER loading code so a
  // concurrent writer replacing a slot-colliding entry (addr invalidated to 0
  // first, then code, then addr) can never make us return a mismatched code.
  {
    uint32_t a = static_cast<uint32_t>(guest_address);
    ResolveCacheEntry& e = g_resolve_cache[ResolveCacheSlot(a)];
    if (e.addr.load(std::memory_order_acquire) == a) {
      uint64_t code = e.code.load(std::memory_order_acquire);
      if (code && e.addr.load(std::memory_order_acquire) == a) {
        return code;
      }
    }
  }
  // DIAGNOSTIC: rate-limited trace of indirect-call resolution. A runaway
  // recursion via mis-detected returns shows up as the same guest_address being
  // resolved over and over. (Only cache MISSES reach here now.)
  {
    // Runaway-recursion detector: if the SAME guest address is resolved many
    // times in a row, a guest 'blr' return is being mis-classified as a call
    // (CallIndirect return-detection failing), so each "call" pushes a host
    // frame → eventual stack overflow. Dump the return-detection probe state
    // (target vs saved return addr, match/miss counts) ONCE so we can see why
    // the compare failed, then keep going (the crash itself still happens, but
    // now with a diagnosis in the log).
    static std::atomic<uint32_t> s_last_addr{0};
    static std::atomic<uint32_t> s_repeat{0};
    static std::atomic<uint64_t> s_total{0};
    static std::atomic<uint32_t> s_logs{0};
    uint32_t addr = static_cast<uint32_t>(guest_address);
    if (s_last_addr.exchange(addr, std::memory_order_relaxed) == addr) {
      s_repeat.fetch_add(1, std::memory_order_relaxed);
    } else {
      s_repeat.store(0, std::memory_order_relaxed);
    }
    // ROLLING SAMPLE: every 300000 indirect-call resolutions, log the address
    // currently being ground on + its caller (guest LR) + the apply-to-array
    // loop regs. A time series of this shows whether the spinning Main XThread is
    // wedged on ONE loop (same addr/lr every sample) or churning (changing).
    uint64_t tot = s_total.fetch_add(1, std::memory_order_relaxed) + 1;
    if ((tot % 300000) == 0 &&
        s_logs.fetch_add(1, std::memory_order_relaxed) < 40) {
      auto* gctx = reinterpret_cast<ppc::PPCContext*>(raw_context);
      XELOGE("ARM64Resolve SAMPLE#{} addr={:08X} consec={} lr={:08X} ctr={:08X} "
             "r3={:08X} r5={:08X} r31={:08X}",
             (uint32_t)(tot / 300000), addr,
             s_repeat.load(std::memory_order_relaxed), (uint32_t)gctx->lr,
             (uint32_t)gctx->ctr, (uint32_t)gctx->r[3], (uint32_t)gctx->r[5],
             (uint32_t)gctx->r[31]);
    }
  }
  auto* ts = *reinterpret_cast<xe::cpu::ThreadState**>(raw_context);
  auto* fn = ts->processor()->ResolveFunction(
      static_cast<uint32_t>(guest_address));
  if (!fn) return 0;
  if (!fn->is_guest()) {
    // BuiltinFunction (0xFFFF00xx) — the static_cast below would read a wrong
    // vtable slot (machine_code() only exists on GuestFunction). No guest code
    // should indirectly call a builtin address; log loudly and fail safe.
    XELOGE("ARM64Resolve: target {:08X} is BUILTIN '{}' — unsupported indirect",
           static_cast<uint32_t>(guest_address), fn->name());
    return 0;
  }
  uint64_t code = reinterpret_cast<uint64_t>(
      static_cast<xe::cpu::GuestFunction*>(fn)->machine_code());
  // Populate the lock-free cache so subsequent calls to this target skip the
  // global-lock slow path. Store code first, then addr (release), so a concurrent
  // reader that sees a matching addr is guaranteed to also see a valid code ptr.
  if (code) {
    uint32_t a = static_cast<uint32_t>(guest_address);
    ResolveCacheEntry& e = g_resolve_cache[ResolveCacheSlot(a)];
    // Invalidate (addr=0) before rewriting a slot-colliding entry, then code,
    // then addr last — pairs with the reader's bracketing addr re-check.
    e.addr.store(0, std::memory_order_release);
    e.code.store(code, std::memory_order_release);
    e.addr.store(a, std::memory_order_release);
  }
  return code;
}

// Logs an error when a call to an extern function has no registered handler.
static uint64_t UndefinedCallExtern(void* raw_context, uint64_t function_ptr) {
  auto* fn = reinterpret_cast<xe::cpu::Function*>(function_ptr);
  XELOGE("undefined extern call to {:08X} {}", fn->address(), fn->name());
  return 0;
}

void ARM64Emitter::EmitStoreWatch(ARM64Reg ea, ARM64Reg value32) {
  // Disabled by default: emitting a host call after every guest store massively
  // slows boot (widening the window for the environmental ART crash). Flip to
  // true to re-enable the store-watch trace.
  static constexpr bool kStoreWatchEnabled = false;
  if (!kStoreWatchEnabled) return;
  // Record EVERY 32-bit store; XeStoreWatchRecord's runtime range filter narrows
  // to [g_store_watch_lo, g_store_watch_hi) (set to the watched address). Must run
  // AFTER the store (ea/value already consumed); clobbers caller-saved x0-x18,x30.
  // x19 (context) and x20 (membase) are callee-saved and survive the host call.
  SUB(ARM64Reg::X0, ea, GetMembaseReg());            // x0 = guest address
  MOV(EncodeRegTo32(ARM64Reg::X1), EncodeRegTo32(value32));  // x1 = value
  MOVI2R(ARM64Reg::X2, current_guest_address_);      // x2 = storing guest PC
  MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&XeStoreWatchRecord));
  BLR(ScratchReg(0));
}

// Camera: record what a 32-bit LOAD actually reads (EA + value), into the same
// store-watch ring, marked with the high bit of the pc so loads are
// distinguishable from stores in the EXC dump. Gated to one function so it
// doesn't slow the whole boot. Must run AFTER the load (value in value32);
// clobbers caller-saved x0-x18,x30 (dest is callee-saved x21-x27 and survives).
void ARM64Emitter::EmitLoadTrace(ARM64Reg ea, ARM64Reg value32) {
  if (g_emit_fn_addr != 0xFFFFFFFFu) return;  // disabled
  SUB(ARM64Reg::X0, ea, GetMembaseReg());            // x0 = guest address
  MOV(EncodeRegTo32(ARM64Reg::X1), EncodeRegTo32(value32));  // x1 = loaded value
  MOVI2R(ARM64Reg::X2, current_guest_address_ | 0x40000000u);  // bit30 = LOAD
  MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&XeStoreWatchRecord));
  BLR(ScratchReg(0));
}

// Emits a direct guest-to-guest call to a known GuestFunction; handles both
// normal calls (BLR with return address in x2) and tail calls (BR after teardown).
void ARM64Emitter::Call(const hir::Instr* instr, GuestFunction* function) {
  ARM64Reg target = ScratchReg(0);
  if (function->machine_code()) {
    MOVI2R(target, reinterpret_cast<uint64_t>(function->machine_code()));
  } else {
    MOV(ARM64Reg::X0, GetContextReg());
    MOVI2R(ARM64Reg::X1, static_cast<uint64_t>(function->address()));
    MOVI2R(target, reinterpret_cast<uint64_t>(ARM64ResolveFunction));
    BLR(target);
    MOV(target, ARM64Reg::X0);
  }
  if (instr->flags & hir::CALL_TAIL) {
    // Tail call: hand the callee OUR return address, tear down our frame, and
    // branch (no host return pushed) so the callee returns straight to our
    // caller. Loads must happen before the frame is deallocated.
    LDR(IndexType::Unsigned, ARM64Reg::X2,
        ARM64Reg::SP, (s32)StackLayout::GUEST_RET_ADDR);
    LDR(IndexType::Unsigned, ARM64Reg::X19,
        ARM64Reg::SP, (s32)StackLayout::GUEST_CTX_HOME);
    EmitRestoreCalleeSaved();
    EmitFreeFrame();
    BR(target);
  } else {
    // Normal call: pass the callee its return address (from the most recent
    // SET_RETURN_ADDRESS) in x2.
    LDR(IndexType::Unsigned, ARM64Reg::X2,
        ARM64Reg::SP, (s32)StackLayout::GUEST_CALL_RET_ADDR);
    BLR(target);
  }
}

// Emits an indirect call through a register: first checks if the target is the
// return address (CALL_POSSIBLE_RETURN detection), then resolves and calls.
void ARM64Emitter::CallIndirect(const hir::Instr* instr, ARM64Reg reg) {
  if (instr->flags & hir::CALL_POSSIBLE_RETURN) {
    ARM64Reg saved_ret = ScratchReg(1);
    LDR(IndexType::Unsigned, EncodeRegTo32(saved_ret),
        ARM64Reg::SP, static_cast<s32>(StackLayout::GUEST_RET_ADDR));
    // PROBE: record the two compare operands so the EXC handler can show why a
    // return wasn't detected.
    MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&g_arm64_ci_target));
    STR(IndexType::Unsigned, EncodeRegTo32(reg), ScratchReg(0), 0);
    MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&g_arm64_ci_saved));
    STR(IndexType::Unsigned, EncodeRegTo32(saved_ret), ScratchReg(0), 0);
    CMP(EncodeRegTo32(reg), EncodeRegTo32(saved_ret));
    auto not_ret = B(CC_NEQ);
    // Return detected: bump match counter, then branch to epilog.
    MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&g_arm64_ci_matches));
    LDR(IndexType::Unsigned, EncodeRegTo32(ScratchReg(2)), ScratchReg(0), 0);
    ADD(EncodeRegTo32(ScratchReg(2)), EncodeRegTo32(ScratchReg(2)), 1);
    STR(IndexType::Unsigned, EncodeRegTo32(ScratchReg(2)), ScratchReg(0), 0);
    AddEpilogFixup(B());
    SetJumpTarget(not_ret);
    // Not a return: bump miss counter and fall through to resolve+call.
    MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(&g_arm64_ci_misses));
    LDR(IndexType::Unsigned, EncodeRegTo32(ScratchReg(2)), ScratchReg(0), 0);
    ADD(EncodeRegTo32(ScratchReg(2)), EncodeRegTo32(ScratchReg(2)), 1);
    STR(IndexType::Unsigned, EncodeRegTo32(ScratchReg(2)), ScratchReg(0), 0);
  }

  // Zero-extend the 32-bit guest address into X1 for the resolver call.
  MOV(ARM64Reg::X1, EncodeRegTo32(reg));
  MOV(ARM64Reg::X0, GetContextReg());
  ARM64Reg fn = ScratchReg(0);
  MOVI2R(fn, reinterpret_cast<uint64_t>(ARM64ResolveFunction));
  BLR(fn);
  // x0 = resolved host code address.

  if (instr->flags & hir::CALL_TAIL) {
    LDR(IndexType::Unsigned, ARM64Reg::X2,
        ARM64Reg::SP, (s32)StackLayout::GUEST_RET_ADDR);
    LDR(IndexType::Unsigned, ARM64Reg::X19,
        ARM64Reg::SP, (s32)StackLayout::GUEST_CTX_HOME);
    EmitRestoreCalleeSaved();
    EmitFreeFrame();
    BR(ARM64Reg::X0);
  } else {
    LDR(IndexType::Unsigned, ARM64Reg::X2,
        ARM64Reg::SP, (s32)StackLayout::GUEST_CALL_RET_ADDR);
    BLR(ARM64Reg::X0);
  }
}

// Emits a call to a builtin or extern function through the guest→host thunk;
// logs an error and calls UndefinedCallExtern if no handler is registered.
void ARM64Emitter::CallExtern(const hir::Instr* instr, const Function* function) {
  bool undefined = true;
  if (function->behavior() == Function::Behavior::kBuiltin) {
    auto* bf = static_cast<const BuiltinFunction*>(function);
    if (bf->handler()) {
      undefined = false;
      auto thunk = backend_->guest_to_host_thunk();
      MOVI2R(ARM64Reg::X0, reinterpret_cast<uint64_t>(bf->handler()));
      MOVI2R(ARM64Reg::X1, reinterpret_cast<uint64_t>(bf->arg0()));
      MOVI2R(ARM64Reg::X2, reinterpret_cast<uint64_t>(bf->arg1()));
      MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(thunk));
      BLR(ScratchReg(0));
    }
  } else if (function->behavior() == Function::Behavior::kExtern) {
    auto* ef = static_cast<const GuestFunction*>(function);
    if (ef->extern_handler()) {
      undefined = false;
      auto thunk = backend_->guest_to_host_thunk();
      MOVI2R(ARM64Reg::X0, reinterpret_cast<uint64_t>(ef->extern_handler()));
      LDR(IndexType::Unsigned, ARM64Reg::X1, GetContextReg(),
          static_cast<s32>(offsetof(ppc::PPCContext, kernel_state)));
      MOVI2R(ARM64Reg::X2, 0);
      MOVI2R(ScratchReg(0), reinterpret_cast<uint64_t>(thunk));
      BLR(ScratchReg(0));
    }
  }
  if (undefined) {
    CallNative(UndefinedCallExtern, reinterpret_cast<uint64_t>(function));
  }
}

// ---------------------------------------------------------------------------
// ValueOp<T>::ResolveReg specializations
//
// Maps a HIR Value's allocated register index to the physical AArch64
// register for each HIR type. GPR set → gpr_reg_map_ (X21–X27),
// vec set → vec_reg_map_ (V4–V15).
// ---------------------------------------------------------------------------
template<>
ARM64Reg ValueOp<I8Op,   KEY_TYPE_V_I8,   int8_t>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::W0 + (int)ARM64Emitter::gpr_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<I16Op,  KEY_TYPE_V_I16,  int16_t>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::W0 + (int)ARM64Emitter::gpr_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<I32Op,  KEY_TYPE_V_I32,  int32_t>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::W0 + (int)ARM64Emitter::gpr_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<I64Op,  KEY_TYPE_V_I64,  int64_t>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::X0 + (int)ARM64Emitter::gpr_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<F32Op,  KEY_TYPE_V_F32,  float>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::S0 + (int)ARM64Emitter::vec_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<F64Op,  KEY_TYPE_V_F64,  double>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::D0 + (int)ARM64Emitter::vec_reg_map_[v->reg.index];
}
template<>
ARM64Reg ValueOp<V128Op, KEY_TYPE_V_V128, vec128_t>::ResolveReg(const hir::Value* v) {
  return ARM64Reg::Q0 + (int)ARM64Emitter::vec_reg_map_[v->reg.index];
}

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
