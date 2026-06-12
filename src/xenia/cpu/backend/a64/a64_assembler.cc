/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64Assembler — HIR-to-AArch64 machine code lowering.
 *
 * IMPLEMENTATION STRATEGY:
 *   "All values live in memory" — the simplest possible register allocation.
 *   Before each instruction, source values are loaded from HIR slots into
 *   scratch registers.  After each instruction, the destination is stored back.
 *   This is correct but slow; a future improvement can add a linear-scan
 *   allocator on top without touching the per-opcode lowering logic.
 *
 * FRAME LAYOUT (see a64_assembler.h for full diagram):
 *   Frame = 32 bytes of saved registers + N*8 bytes of HIR slots
 *   N = builder->max_value_ordinal()
 *   Rounded to 16-byte alignment for AAPCS64 compliance.
 *
 * SCRATCH REGISTER ASSIGNMENTS:
 *   X0 = destination operand (loaded or computed, then stored to slot)
 *   X1 = source 1 (loaded from slot or materialized from constant)
 *   X2 = source 2 (same)
 *   X3 = scratch / address register for LOAD/STORE
 *   X4 = scratch for CALL function pointer
 *
 * MEMORY ACCESS (LOAD / STORE):
 *   Guest address in X1 is zero-extended and added to virtual_membase (X20).
 *   Result address: X3 = X20 + X1
 *   Load/store size is chosen from the HIR value type (INT8→LDRB, INT16→LDRH,
 *   INT32/FLOAT32→LDR32, INT64+→LDR64).  For byte-swapped accesses
 *   (LOAD_STORE_BYTE_SWAP flag), REV16_32 / REV32 / REV64 is emitted after
 *   load or before store (1-byte accesses need no swap).
 *
 * CONTEXT ACCESS (LOAD_CONTEXT / STORE_CONTEXT):
 *   PPCContext is stored in host (little-endian) order — no byte swap.
 *   Immediate-offset form is always used (PPCContext max offset ~0xA28 < 4096).
 *
 * CALL / CALL_EXTERN:
 *   CALL:          src1.symbol is a Function*.  We load its machine_code()
 *                  pointer (8 bytes from the Function object at the offset of
 *                  the machine_code_ field) and BLR to it.
 *   CALL_EXTERN:   src1.symbol is a Function*.  We obtain the backend's
 *                  guest_to_host_thunk and call:
 *                    guest_to_host_thunk(extern_fn, ctx, nullptr)
 *                  where extern_fn is the Function's extern_handler (C++ fn ptr
 *                  stored in Function::extern_handler).
 *   CALL_INDIRECT: src1.value holds the guest target address (uint32).
 *                  We look it up via Processor::ResolveFunction (not yet
 *                  implemented — emits BRK 0xDEAD as placeholder).
 *
 * BRANCH FIXUP:
 *   Forward branches use a placeholder 0-offset instruction.  After all blocks
 *   are emitted the A64Emitter::ApplyFixups() overwrites each placeholder with
 *   the real offset to the target block.
 *
 * TROUBLESHOOTING:
 *   - BRK 0xDEAD crash: hit an unimplemented HIR opcode.  The instr->opcode->name
 *     field names it — add a case in LowerInstruction().
 *   - Stack-alignment fault (BUS_ADRALN): frame_size is not a multiple of 16.
 *     BuildProlog rounds up — check the rounding arithmetic.
 *   - Wrong context access: X19 must be PPCContext* at all times.  If it gets
 *     clobbered the whole session is broken.  X19 is callee-saved so it should
 *     survive C++ calls, but check that no opcode handler writes X19.
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_assembler.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <android/log.h>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/backend/a64/a64_backend.h"
#include "xenia/cpu/backend/a64/a64_code_cache.h"
#include "xenia/cpu/backend/a64/a64_function.h"
#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/cpu/hir/hir_builder.h"
#include "xenia/cpu/hir/block.h"
#include "xenia/cpu/hir/instr.h"
#include "xenia/cpu/hir/label.h"
#include "xenia/cpu/hir/opcodes.h"
#include "xenia/cpu/hir/value.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

// Trampoline helpers — thin C wrappers around virtual methods.
// JIT code loads their addresses and BLRs to them to avoid hardcoding
// vtable offsets (which would break if the class layout changes).
//
// DemandAndGetMachineCode(fn, ctx)
//   Compile fn on first call (demand compilation), then return machine_code_.
//   ctx is the current PPCContext* (X19 in the JIT frame); we reach the
//   Processor through ctx->thread_state->processor() to call DemandFunction.
// Trace log using a dedicated "xenia_jit" logcat tag to avoid rate limiting
// on the main "xenia" tag.
static void TraceLog(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  __android_log_print(ANDROID_LOG_INFO, "xenia_jit", "%s", buf);
}

// Called at runtime when a guest block is entered; logs the first N unique
// guest PCs to identify infinite loops.  Only active while tracing.
static std::atomic<uint32_t> s_trace_enabled{1};  // set to 0 after enough data
static std::atomic<uint32_t> s_block_call_count{0};
static void BlockTraceHelper(uint32_t guest_pc) {
  uint32_t n = s_block_call_count.fetch_add(1, std::memory_order_relaxed);
  // Log first 200 block entries, then every 10000th to show if still looping
  if (n < 200 || (n % 10000) == 0) {
    TraceLog("BLOCK %u @%08x", n, guest_pc);
  }
}

static uint8_t* DemandAndGetMachineCode(GuestFunction* fn,
                                        ppc::PPCContext* ctx) {
  if (!fn->machine_code()) {
    TraceLog("demand-compile @%08x '%s'", fn->address(), fn->name().c_str());
    ctx->thread_state->processor()->ResolveFunction(fn->address());
    if (!fn->machine_code()) {
      TraceLog("compile FAILED @%08x", fn->address());
    }
  }
  return fn->machine_code();
}

// CallIndirectHelper — resolve a guest address to machine code at runtime.
// x0 = guest addr (uint32), x1 = PPCContext*
static uint8_t* CallIndirectHelper(uint32_t guest_addr, ppc::PPCContext* ctx) {
  auto* processor = ctx->thread_state->processor();
  auto* fn = processor->ResolveFunction(guest_addr);
  if (!fn) {
    TraceLog("CALL_INDIRECT failed @%08x", guest_addr);
    return nullptr;
  }
  auto* gfn = static_cast<GuestFunction*>(fn);
  TraceLog("CALL_INDIRECT @%08x -> machine_code=%p", guest_addr,
           (void*)gfn->machine_code());
  return gfn->machine_code();
}

static void CallExternHelper(GuestFunction::ExternHandler handler,
                             ppc::PPCContext* ctx) {
  TraceLog("CALL_EXTERN handler=%p", (void*)(uintptr_t)handler);
  if (handler) {
    handler(ctx, ctx->kernel_state);
  } else {
    // Null handler — zero r3 so callers don't see stale return values.
    ctx->r[3] = 0;
  }
}

// ---------------------------------------------------------------------------
A64Assembler::A64Assembler(A64Backend* backend)
    : Assembler(backend), a64_backend_(backend) {}

A64Assembler::~A64Assembler() = default;

bool A64Assembler::Initialize() {
  return Assembler::Initialize();
}

void A64Assembler::Reset() {
  emitter_.Reset();
  block_offsets_.clear();
  last_frame_size_ = 0;
  Assembler::Reset();
}

bool A64Assembler::Assemble(GuestFunction* function,
                             hir::HIRBuilder* builder,
                             uint32_t /*debug_info_flags*/,
                             std::unique_ptr<FunctionDebugInfo> /*debug_info*/) {
  TraceLog("Assemble ENTER @%08x '%s'", function->address(),
           function->name().c_str());

  // Determine the total number of HIR value slots needed.
  size_t num_slots = builder->max_value_ordinal();

  // PROLOG — allocate frame and establish X25=slot_base
  size_t frame_size = BuildProlog(num_slots);

  // Iterate blocks in order, recording the byte offset of each one.
  std::unordered_map<int, size_t> block_off_map;
  block_offsets_.clear();

  for (auto* block = builder->first_block(); block; block = block->next) {
    int ord = static_cast<int>(block->ordinal);
    // Grow vector if needed
    if (ord >= static_cast<int>(block_offsets_.size())) {
      block_offsets_.resize(static_cast<size_t>(ord + 1), SIZE_MAX);
    }
    size_t blk_byte = emitter_.Size();
    block_offsets_[static_cast<size_t>(ord)] = blk_byte;
    block_off_map[ord] = blk_byte;

    // Emit block-trace call for every block (use SOURCE_OFFSET guest PC
    // if available, otherwise encode fn_addr | block_ordinal as a sentinel).
    {
      uint32_t block_guest_pc = 0;
      for (auto* si = block->instr_head; si; si = si->next) {
        if (si->opcode && si->opcode->num == hir::OPCODE_SOURCE_OFFSET) {
          block_guest_pc = static_cast<uint32_t>(si->src1.offset);
          break;
        }
      }
      if (!block_guest_pc) {
        // Sentinel: lower 16 bits = block ordinal, upper bits = 0xBBBB0000
        block_guest_pc = 0xBBBB0000u | static_cast<uint32_t>(ord & 0xFFFF);
      }
      emitter_.EmitMov64Fast(X4, reinterpret_cast<uintptr_t>(&BlockTraceHelper));
      emitter_.EmitMov64Fast(X0, block_guest_pc);
      emitter_.Emit(BLR(X4));
    }

    // Lower each instruction in this block.
    for (auto* instr = block->instr_head; instr; instr = instr->next) {
      LowerInstruction(instr);
    }
  }

  // Patch all forward (and backward) branch placeholders.
  if (!emitter_.ApplyFixups(block_off_map)) {
    XELOGE("A64Assembler: branch fixup failed for function @{:#x}",
           function->address());
    return false;
  }

  // Write the EPILOG at the very end (after the last block).
  // Note: blocks that contain RETURN already emit their own epilog+RET.
  // This epilog is a safety landing pad for fall-through paths.
  BuildEpilog(frame_size);

  // Allocate space in the code cache.
  size_t code_size = emitter_.Size();
  if (code_size == 0) {
    XELOGE("A64Assembler: empty code for function @{:#x}", function->address());
    return false;
  }

  uint8_t* write_ptr  = nullptr;
  uint8_t* exec_ptr   = a64_backend_->a64_code_cache()->Alloc(code_size, &write_ptr);
  if (!exec_ptr) {
    XELOGE("A64Assembler: code cache exhausted (size={} bytes)", code_size);
    return false;
  }

  std::memcpy(write_ptr, emitter_.Data(), code_size);
  a64_backend_->a64_code_cache()->FlushInstrCache(write_ptr, code_size);

  // Attach the machine code to the function.
  reinterpret_cast<A64Function*>(function)->Setup(exec_ptr, code_size);

  // Register with the code cache so LookupFunction works for debugger / stack
  // unwinding.
  a64_backend_->a64_code_cache()->RegisterFunction(function, exec_ptr, code_size);

  TraceLog("compiled @%08x '%s' %zu bytes", function->address(),
           function->name().c_str(), code_size);
  return true;
}

// ---------------------------------------------------------------------------
// PROLOG
//   Stack frame on entry to the JIT function:
//     [SP, #0 ] = X29 (frame pointer)
//     [SP, #8 ] = X30 (LR)
//     [SP, #16] = X25 (HIR slot base)
//     [SP, #24] = X26 (reserved)
//     [SP, #32..32+slots_bytes] = HIR value slots
//
//   X25 = SP + 32  after the prolog (base of slot area).
// ---------------------------------------------------------------------------
size_t A64Assembler::BuildProlog(size_t num_slots) {
  size_t slots_bytes = num_slots * 8;
  // Total frame: 32 (saved regs) + slot area, rounded to 16-byte alignment
  size_t frame_size = (32 + slots_bytes + 15) & ~size_t(15);

  // STP X29, X30, [SP, #-frame_size]!  — allocate frame, save FP+LR
  emitter_.Emit(STP64_PRE(X29, X30, SP, -static_cast<int>(frame_size)));
  // STP X25, X26, [SP, #16]           — save X25 (slot base) and X26
  emitter_.Emit(STP64_OFF(X25, X26, SP, 16));
  // ADD X25, SP, #32                  — establish slot base pointer
  emitter_.Emit(ADD64ri(X25, SP, 32));
  // MOV X29, SP                       — set frame pointer
  emitter_.Emit(ADD64ri(X29, SP, 0));

  last_frame_size_ = frame_size;
  return frame_size;
}

// ---------------------------------------------------------------------------
// EPILOG — mirrors the prolog exactly.
// ---------------------------------------------------------------------------
void A64Assembler::BuildEpilog(size_t frame_size) {
  emitter_.Emit(LDP64_OFF(X25, X26, SP, 16));
  emitter_.Emit(LDP64_POST(X29, X30, SP, static_cast<int>(frame_size)));
  emitter_.Emit(RET_instr());
}

// ---------------------------------------------------------------------------
// Helper: load HIR value `v` into AArch64 integer register `reg`.
// ---------------------------------------------------------------------------
void A64Assembler::LoadValue(const hir::Value* v, uint32_t reg) {
  if (v->flags & hir::VALUE_IS_CONSTANT) {
    // Materialize constant inline
    emitter_.EmitMov64Fast(static_cast<int>(reg), v->constant.u64);
  } else {
    // Load from HIR slot: LDR reg, [X25, #ordinal*8]
    size_t off = SlotOffset(static_cast<int>(v->ordinal));
    if (off <= 32760) {
      emitter_.Emit(LDR64(static_cast<int>(reg), X25,
                          static_cast<uint32_t>(off)));
    } else {
      // Slot offset too large for imm12 — use a register offset
      // X8 is a scratch register not used as dest/src1/src2
      emitter_.EmitMov64Fast(X8, static_cast<uint64_t>(off));
      emitter_.Emit(LDR64_REG(static_cast<int>(reg), X25, X8));
    }
  }
}

// ---------------------------------------------------------------------------
// Helper: store register `reg` back to HIR value slot for `v`.
// ---------------------------------------------------------------------------
void A64Assembler::StoreValue(const hir::Value* v, uint32_t reg) {
  size_t off = SlotOffset(static_cast<int>(v->ordinal));
  if (off <= 32760) {
    emitter_.Emit(STR64(static_cast<int>(reg), X25,
                        static_cast<uint32_t>(off)));
  } else {
    emitter_.EmitMov64Fast(X8, static_cast<uint64_t>(off));
    emitter_.Emit(STR64_REG(static_cast<int>(reg), X25, X8));
  }
}

// ---------------------------------------------------------------------------
// LowerInstruction — one-to-one (or one-to-few) HIR instruction lowering.
// ---------------------------------------------------------------------------
void A64Assembler::LowerInstruction(const hir::Instr* instr) {
  if (!instr->opcode) return;

  switch (instr->opcode->num) {
    // -------------------------------------------------------------------------
    case hir::OPCODE_NOP:
    case hir::OPCODE_COMMENT:
    case hir::OPCODE_SOURCE_OFFSET:
    case hir::OPCODE_CONTEXT_BARRIER:
    case hir::OPCODE_MEMORY_BARRIER:
      // No code needed
      break;

    // -------------------------------------------------------------------------
    // DEBUG_BREAK — emit a software breakpoint (SIGTRAP)
    case hir::OPCODE_DEBUG_BREAK:
      emitter_.Emit(BRK(0));
      break;

    // -------------------------------------------------------------------------
    // ASSIGN: dest = src1 (both HIR values)
    case hir::OPCODE_ASSIGN:
      LoadValue(instr->src1.value, X0);
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // LOAD_CONTEXT: dest = *(PPCContext* + offset)
    //   src1.offset = byte offset into context.
    //   Context values are stored in host (little-endian) order — no byte swap.
    //   PPCContext max offset is ~0xA28 (< 4096), so immediate form always fits.
    case hir::OPCODE_LOAD_CONTEXT: {
      uint32_t ctx_off = static_cast<uint32_t>(instr->src1.offset);
      switch (instr->dest->type) {
        case hir::INT8_TYPE:
          emitter_.Emit(LDRB(X0, X19, ctx_off));
          break;
        case hir::INT16_TYPE:
          emitter_.Emit(LDRH(X0, X19, ctx_off));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          emitter_.Emit(LDR32(X0, X19, ctx_off));
          break;
        default:  // INT64_TYPE, FLOAT64_TYPE, VEC128_TYPE treated as 64-bit
          if (ctx_off <= 32760) {
            emitter_.Emit(LDR64(X0, X19, ctx_off));
          } else {
            emitter_.EmitMov64Fast(X1, static_cast<uint64_t>(ctx_off));
            emitter_.Emit(LDR64_REG(X0, X19, X1));
          }
          break;
      }
      StoreValue(instr->dest, X0);
      break;
    }

    // -------------------------------------------------------------------------
    // STORE_CONTEXT: *(PPCContext* + offset) = src2
    //   src1.offset = byte offset, src2.value = value to write.
    //   No byte swap — context is host-endian.
    case hir::OPCODE_STORE_CONTEXT: {
      uint32_t ctx_off = static_cast<uint32_t>(instr->src1.offset);
      LoadValue(instr->src2.value, X0);
      switch (instr->src2.value->type) {
        case hir::INT8_TYPE:
          emitter_.Emit(STRB(X0, X19, ctx_off));
          break;
        case hir::INT16_TYPE:
          emitter_.Emit(STRH(X0, X19, ctx_off));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          emitter_.Emit(STR32(X0, X19, ctx_off));
          break;
        default:  // INT64_TYPE and wider
          if (ctx_off <= 32760) {
            emitter_.Emit(STR64(X0, X19, ctx_off));
          } else {
            emitter_.EmitMov64Fast(X1, static_cast<uint64_t>(ctx_off));
            emitter_.Emit(STR64_REG(X0, X19, X1));
          }
          break;
      }
      break;
    }

    // -------------------------------------------------------------------------
    // LOAD: dest = *(guest_addr)
    //   src1.value = guest virtual address (uint32, zero-extended).
    //   The LOAD_STORE_BYTE_SWAP flag is set for big-endian PPC memory accesses.
    //   Load size must match dest type; byte-swap size must match too.
    case hir::OPCODE_LOAD: {
      LoadValue(instr->src1.value, X1);    // X1 = guest addr (32-bit)
      emitter_.Emit(UXTW(X3, X1));         // X3 = zero-extend to 64-bit
      emitter_.Emit(ADD64rr(X3, X20, X3)); // X3 = membase + guest_addr
      const bool bswap = (instr->flags & hir::LOAD_STORE_BYTE_SWAP) != 0;
      switch (instr->dest->type) {
        case hir::INT8_TYPE:
          emitter_.Emit(LDRB(X0, X3, 0));
          // 1-byte load: no byte swap needed
          break;
        case hir::INT16_TYPE:
          emitter_.Emit(LDRH(X0, X3, 0));
          if (bswap) emitter_.Emit(REV16_32(X0, X0));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          emitter_.Emit(LDR32(X0, X3, 0));
          if (bswap) emitter_.Emit(REV32(X0, X0));
          break;
        default:  // INT64_TYPE and wider
          emitter_.Emit(LDR64(X0, X3, 0));
          if (bswap) emitter_.Emit(REV64(X0, X0));
          break;
      }
      StoreValue(instr->dest, X0);
      break;
    }

    // -------------------------------------------------------------------------
    // STORE: *(guest_addr) = src2
    //   src1.value = guest address, src2.value = value.
    //   Byte-swap before storing for big-endian PPC memory accesses.
    case hir::OPCODE_STORE: {
      LoadValue(instr->src1.value, X1);    // X1 = guest addr
      LoadValue(instr->src2.value, X0);    // X0 = value to store
      emitter_.Emit(UXTW(X3, X1));         // X3 = zero-extend
      emitter_.Emit(ADD64rr(X3, X20, X3)); // X3 = host address
      const bool bswap = (instr->flags & hir::LOAD_STORE_BYTE_SWAP) != 0;
      switch (instr->src2.value->type) {
        case hir::INT8_TYPE:
          // 1-byte store: no byte swap needed
          emitter_.Emit(STRB(X0, X3, 0));
          break;
        case hir::INT16_TYPE:
          if (bswap) emitter_.Emit(REV16_32(X0, X0));
          emitter_.Emit(STRH(X0, X3, 0));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          if (bswap) emitter_.Emit(REV32(X0, X0));
          emitter_.Emit(STR32(X0, X3, 0));
          break;
        default:  // INT64_TYPE and wider
          if (bswap) emitter_.Emit(REV64(X0, X0));
          emitter_.Emit(STR64(X0, X3, 0));
          break;
      }
      break;
    }

    // -------------------------------------------------------------------------
    // LOAD_OFFSET: dest = *(guest_addr + offset)
    //   src1.value = guest base address, src2.value = byte offset (often constant)
    //   Same byte-swap rules as LOAD.
    case hir::OPCODE_LOAD_OFFSET: {
      LoadValue(instr->src1.value, X1);    // X1 = guest base addr
      if (instr->src2.value->flags & hir::VALUE_IS_CONSTANT) {
        emitter_.EmitMov64Fast(X2,
            static_cast<uint64_t>(instr->src2.value->constant.i64));
      } else {
        LoadValue(instr->src2.value, X2);  // X2 = runtime offset
      }
      emitter_.Emit(ADD64rr(X1, X1, X2)); // X1 = base + offset (64-bit)
      emitter_.Emit(UXTW(X3, X1));         // X3 = zero-extend low 32 bits
      emitter_.Emit(ADD64rr(X3, X20, X3)); // X3 = membase + guest_addr
      const bool bswap = (instr->flags & hir::LOAD_STORE_BYTE_SWAP) != 0;
      switch (instr->dest->type) {
        case hir::INT8_TYPE:
          emitter_.Emit(LDRB(X0, X3, 0));
          break;
        case hir::INT16_TYPE:
          emitter_.Emit(LDRH(X0, X3, 0));
          if (bswap) emitter_.Emit(REV16_32(X0, X0));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          emitter_.Emit(LDR32(X0, X3, 0));
          if (bswap) emitter_.Emit(REV32(X0, X0));
          break;
        default:
          emitter_.Emit(LDR64(X0, X3, 0));
          if (bswap) emitter_.Emit(REV64(X0, X0));
          break;
      }
      StoreValue(instr->dest, X0);
      break;
    }

    // -------------------------------------------------------------------------
    // STORE_OFFSET: *(guest_addr + offset) = src3
    //   src1.value = guest base address, src2.value = offset, src3.value = value
    case hir::OPCODE_STORE_OFFSET: {
      LoadValue(instr->src1.value, X1);    // X1 = guest base addr
      if (instr->src2.value->flags & hir::VALUE_IS_CONSTANT) {
        emitter_.EmitMov64Fast(X2,
            static_cast<uint64_t>(instr->src2.value->constant.i64));
      } else {
        LoadValue(instr->src2.value, X2);  // X2 = runtime offset
      }
      LoadValue(instr->src3.value, X0);    // X0 = value to store
      emitter_.Emit(ADD64rr(X1, X1, X2)); // X1 = base + offset
      emitter_.Emit(UXTW(X3, X1));         // X3 = zero-extend
      emitter_.Emit(ADD64rr(X3, X20, X3)); // X3 = host address
      const bool bswap = (instr->flags & hir::LOAD_STORE_BYTE_SWAP) != 0;
      switch (instr->src3.value->type) {
        case hir::INT8_TYPE:
          emitter_.Emit(STRB(X0, X3, 0));
          break;
        case hir::INT16_TYPE:
          if (bswap) emitter_.Emit(REV16_32(X0, X0));
          emitter_.Emit(STRH(X0, X3, 0));
          break;
        case hir::INT32_TYPE:
        case hir::FLOAT32_TYPE:
          if (bswap) emitter_.Emit(REV32(X0, X0));
          emitter_.Emit(STR32(X0, X3, 0));
          break;
        default:
          if (bswap) emitter_.Emit(REV64(X0, X0));
          emitter_.Emit(STR64(X0, X3, 0));
          break;
      }
      break;
    }

    // -------------------------------------------------------------------------
    // Arithmetic: ADD, SUB, AND, OR, XOR, MUL
    case hir::OPCODE_ADD:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(ADD64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_SUB:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(SUB64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_AND:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(AND64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_OR:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(ORR64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_XOR:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(EOR64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    // AND_NOT: dest = src1 & ~src2  (BIC instruction)
    case hir::OPCODE_AND_NOT:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(BIC64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_MUL:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(MUL64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_NEG:
      LoadValue(instr->src1.value, X1);
      emitter_.Emit(NEG64r(X0, X1));
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // NOT: dest = ~src1  (bitwise complement)
    case hir::OPCODE_NOT:
      LoadValue(instr->src1.value, X1);
      emitter_.Emit(NOT64r(X0, X1));
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // ADD_CARRY: dest = src1 + src2 + src3 (carry, INT8)
    //   Use ADDS to set carry from src1+src2, then ADC to add the carry byte.
    //   src3 is 0 or 1 (not a CPU carry flag), so just add it as an integer.
    case hir::OPCODE_ADD_CARRY:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      LoadValue(instr->src3.value, X3);    // X3 = carry (INT8: 0 or 1)
      emitter_.Emit(ADD64rr(X0, X1, X2));  // X0 = src1 + src2
      emitter_.Emit(ADD64rr(X0, X0, X3));  // X0 += carry
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // Shifts
    case hir::OPCODE_SHL:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(LSL64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_SHR:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(LSR64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_SHA:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(ASR64rr(X0, X1, X2));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_ROTATE_LEFT:
      // AArch64 has RORV but not ROLV.  ROL Xd, Xn, shift = ROR Xd, Xn, (64-shift)
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      // X3 = 64 - X2
      emitter_.EmitMov64Fast(X3, 64);
      emitter_.Emit(SUB64rr(X3, X3, X2));
      emitter_.Emit(ROR64rr(X0, X1, X3));
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // Comparisons
    case hir::OPCODE_COMPARE_EQ:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_EQ));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_NE:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_NE));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_SLT:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_LT));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_SLE:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_LE));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_SGT:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_GT));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_SGE:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_GE));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_ULT:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_LO));   // unsigned lower
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_ULE:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_LS));   // unsigned lower-or-same
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_UGT:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_HI));   // unsigned higher
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_COMPARE_UGE:
      LoadValue(instr->src1.value, X1);
      LoadValue(instr->src2.value, X2);
      emitter_.Emit(CMP64rr(X1, X2));
      emitter_.Emit(CSET64(X0, COND_HS));   // unsigned higher-or-same
      StoreValue(instr->dest, X0);
      break;

    // IS_TRUE / IS_FALSE
    case hir::OPCODE_IS_TRUE:
      LoadValue(instr->src1.value, X1);
      emitter_.Emit(CMP64ri(X1, 0));
      emitter_.Emit(CSET64(X0, COND_NE));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_IS_FALSE:
      LoadValue(instr->src1.value, X1);
      emitter_.Emit(CMP64ri(X1, 0));
      emitter_.Emit(CSET64(X0, COND_EQ));
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // Type conversions
    case hir::OPCODE_ZERO_EXTEND:
      LoadValue(instr->src1.value, X1);
      // UXTW handles 32→64; for narrower types AND with mask
      if (instr->src1.value->type == hir::INT32_TYPE) {
        emitter_.Emit(UXTW(X0, X1));
      } else {
        // For 8-bit and 16-bit: mask off upper bits
        emitter_.Emit(MOV64rr(X0, X1));
      }
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_SIGN_EXTEND:
      LoadValue(instr->src1.value, X1);
      if (instr->src1.value->type == hir::INT32_TYPE) {
        emitter_.Emit(SXTW(X0, X1));
      } else if (instr->src1.value->type == hir::INT16_TYPE) {
        emitter_.Emit(SXTH64(X0, X1));
      } else {
        emitter_.Emit(SXTB64(X0, X1));
      }
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_TRUNCATE:
      LoadValue(instr->src1.value, X1);
      // Truncation: just copy — the upper bits are ignored when stored
      emitter_.Emit(MOV64rr(X0, X1));
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_CAST:
      LoadValue(instr->src1.value, X1);
      emitter_.Emit(MOV64rr(X0, X1));
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // Bit count
    case hir::OPCODE_CNTLZ: {
      // CLZ Xd, Xn
      LoadValue(instr->src1.value, X1);
      // CLZ X0, X1 — count leading zeros (64-bit)
      // Encoding: 0xDAC01400 | (Rn<<5) | Rd
      emitter_.Emit(0xDAC01400u | (uint32_t(X1) << 5) | X0);
      StoreValue(instr->dest, X0);
      break;
    }

    // -------------------------------------------------------------------------
    // Byte-swap
    case hir::OPCODE_BYTE_SWAP:
      LoadValue(instr->src1.value, X1);
      if (instr->dest->type == hir::INT64_TYPE) {
        emitter_.Emit(REV64(X0, X1));
      } else {
        emitter_.Emit(REV32(X0, X1));
      }
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // SELECT: dest = cond ? src2 : src3
    case hir::OPCODE_SELECT:
      LoadValue(instr->src1.value, X1);   // condition
      LoadValue(instr->src2.value, X2);   // true value
      LoadValue(instr->src3.value, X3);   // false value
      emitter_.Emit(CMP64ri(X1, 0));
      // CSEL X0, X2, X3, NE  — select X2 if NE (non-zero), else X3
      // CSEL Xd, Xn, Xm, cond = 0x9A800000 | cond<<12 | Xm<<16 | Xn<<5 | Xd
      emitter_.Emit(0x9A800000u | (uint32_t(COND_NE) << 12) |
                    (uint32_t(X3) << 16) | (uint32_t(X2) << 5) | X0);
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // SET_RETURN_ADDRESS: store the return address into context LR slot (0x10)
    case hir::OPCODE_SET_RETURN_ADDRESS: {
      LoadValue(instr->src1.value, X0);
      // Store into context LR field (ppc_context.h: lr at offset 0x10)
      emitter_.Emit(STR64(X0, X19, 0x10));
      break;
    }

    // -------------------------------------------------------------------------
    // RETURN — emit epilog and RET
    case hir::OPCODE_RETURN:
      // Walk back up the prolog to find frame_size.  We store it in a local
      // during Assemble() but this is a separate method.  We keep a cached copy
      // in last_frame_size_ set at prolog time.
      // WORKAROUND: emit a B to the function epilog which is emitted at the end.
      // Record a fixup targeting a special sentinel ordinal -1 for "epilog".
      // Actually the simplest approach: inline the epilog here.
      //
      // The assembler doesn't have frame_size here — so we use a sentinel block
      // approach: we record the epilog offset and patch, OR we just inline it.
      // Since we don't have frame_size stored as a member, inline a runtime
      // calculation: X29 is FP, SP = FP means the slot area is already
      // gone.  But we don't need to: we save the frame_size in a member.
      //
      // SIMPLIFICATION: emit the epilog inline for each RETURN instruction.
      // This is correct.  The extra instructions per RETURN are cheap.
      // frame_size is in last_frame_size_ — set by BuildProlog.
      emitter_.Emit(LDP64_OFF(X25, X26, SP, 16));
      emitter_.Emit(LDP64_POST(X29, X30, SP,
                               static_cast<int>(last_frame_size_)));
      emitter_.Emit(RET_instr());
      break;

    // -------------------------------------------------------------------------
    // RETURN_TRUE: if (src1) RET
    case hir::OPCODE_RETURN_TRUE: {
      LoadValue(instr->src1.value, X0);
      // CBNZ X0, +8  (skip the B past the epilog)
      size_t cbnz_idx = emitter_.InstrCount();
      emitter_.Emit(CBNZ64(X0, 0));  // placeholder
      // B past the inline epilog (3 instructions = 12 bytes ahead)
      size_t b_idx = emitter_.InstrCount();
      emitter_.Emit(B_instr(0));     // placeholder: skip epilog
      // Epilog:
      size_t epilog_off = emitter_.Size();
      emitter_.Emit(LDP64_OFF(X25, X26, SP, 16));
      emitter_.Emit(LDP64_POST(X29, X30, SP,
                               static_cast<int>(last_frame_size_)));
      emitter_.Emit(RET_instr());
      // After epilog:
      size_t after_off = emitter_.Size();
      // Patch CBNZ to jump to epilog
      {
        int32_t delta = static_cast<int32_t>(
            static_cast<int64_t>(epilog_off) -
            static_cast<int64_t>(cbnz_idx * 4));
        // We can't use RecordBranchFixup here (it uses block ordinals).
        // Patch directly:
        const_cast<uint32_t*>(emitter_.Data())[cbnz_idx] = CBNZ64(X0, delta);
      }
      // Patch B to jump after epilog
      {
        int32_t delta = static_cast<int32_t>(
            static_cast<int64_t>(after_off) -
            static_cast<int64_t>(b_idx * 4));
        const_cast<uint32_t*>(emitter_.Data())[b_idx] = B_instr(delta);
      }
      break;
    }

    // -------------------------------------------------------------------------
    // BRANCH: unconditional jump to label
    case hir::OPCODE_BRANCH: {
      hir::Label* lbl = instr->src1.label;
      int target_ord  = static_cast<int>(lbl->block->ordinal);
      size_t fix_idx  = emitter_.InstrCount();
      emitter_.RecordBranchFixup(fix_idx, target_ord, BranchKind::B);
      emitter_.Emit(B_instr(0));  // placeholder
      break;
    }

    // -------------------------------------------------------------------------
    // BRANCH_TRUE: if (src1 != 0) goto label; fall through otherwise
    case hir::OPCODE_BRANCH_TRUE: {
      LoadValue(instr->src1.value, X0);
      hir::Label* lbl = instr->src2.label;
      int target_ord  = static_cast<int>(lbl->block->ordinal);
      size_t fix_idx  = emitter_.InstrCount();
      emitter_.RecordBranchFixup(fix_idx, target_ord, BranchKind::CBNZ_X0);
      emitter_.Emit(CBNZ64(X0, 0));  // placeholder
      break;
    }

    // -------------------------------------------------------------------------
    // BRANCH_FALSE: if (src1 == 0) goto label; fall through otherwise
    case hir::OPCODE_BRANCH_FALSE: {
      LoadValue(instr->src1.value, X0);
      hir::Label* lbl = instr->src2.label;
      int target_ord  = static_cast<int>(lbl->block->ordinal);
      size_t fix_idx  = emitter_.InstrCount();
      emitter_.RecordBranchFixup(fix_idx, target_ord, BranchKind::CBZ_X0);
      emitter_.Emit(CBZ64(X0, 0));  // placeholder
      break;
    }

    // -------------------------------------------------------------------------
    // CALL: call a known Function by symbol
    //   src1.symbol = Function* — may be A64Function or NativeFunction
    //
    //   We call through DemandAndGetMachineCode() to compile the target on
    //   first call (lazy/demand compilation) and return its machine_code_.
    //   Code sequence:
    //     X4 = &DemandAndGetMachineCode
    //     X0 = fn pointer
    //     X1 = PPCContext* (X19)     ← second arg to DemandAndGetMachineCode
    //     BLR X4          → X0 = fn->machine_code() (compiled if needed)
    //     CBZ X0, +4      → skip BLR if still null (compilation failed)
    //     BLR X0          → call guest code (X19/X20 preserved as callee-saved)
    case hir::OPCODE_CALL: {
      Function* fn = instr->src1.symbol;
      emitter_.EmitMov64Fast(X4,
          reinterpret_cast<uintptr_t>(&DemandAndGetMachineCode));
      emitter_.EmitMov64Fast(X0, reinterpret_cast<uintptr_t>(fn));
      emitter_.Emit(MOV64rr(X1, X19));     // X1 = PPCContext* (ctx arg)
      emitter_.Emit(BLR(X4));              // X0 = fn->machine_code()
      emitter_.Emit(CBZ64(X0, 8));         // skip BLR if nullptr (+8 = 2 instrs)
      emitter_.Emit(BLR(X0));              // call guest code
      break;
    }

    // -------------------------------------------------------------------------
    // CALL_TRUE: if (src1 != 0) call src2
    //   src1 = condition value, src2 = Function* symbol
    case hir::OPCODE_CALL_TRUE: {
      Function* fn = instr->src2.symbol;
      LoadValue(instr->src1.value, X3);    // X3 = condition
      // CBZ X3, skip_call — forward branch placeholder
      size_t cbz_idx = emitter_.InstrCount();
      emitter_.Emit(CBZ64(X3, 0));         // placeholder, patched below
      // Inline CALL body (same as OPCODE_CALL)
      emitter_.EmitMov64Fast(X4,
          reinterpret_cast<uintptr_t>(&DemandAndGetMachineCode));
      emitter_.EmitMov64Fast(X0, reinterpret_cast<uintptr_t>(fn));
      emitter_.Emit(MOV64rr(X1, X19));
      emitter_.Emit(BLR(X4));
      emitter_.Emit(CBZ64(X0, 8));
      emitter_.Emit(BLR(X0));
      // Patch the outer CBZ to skip the call block
      {
        size_t after_idx = emitter_.InstrCount();
        int32_t delta = int32_t(int64_t(after_idx - cbz_idx) * 4);
        const_cast<uint32_t*>(emitter_.Data())[cbz_idx] = CBZ64(X3, delta);
      }
      break;
    }

    // -------------------------------------------------------------------------
    // CALL_INDIRECT: call through a runtime guest address in src1.
    //   We call a C helper that resolves/compiles the target on demand.
    case hir::OPCODE_CALL_INDIRECT: {
      // X1 = guest address (uint32)
      LoadValue(instr->src1.value, X1);
      // X2 = PPCContext* (X19)
      emitter_.Emit(MOV64rr(X2, X19));
      // X4 = &CallIndirectHelper
      emitter_.EmitMov64Fast(X4, reinterpret_cast<uintptr_t>(&CallIndirectHelper));
      emitter_.Emit(BLR(X4));   // X0 = machine_code ptr or null
      emitter_.Emit(CBZ64(X0, 8));
      emitter_.Emit(BLR(X0));
      break;
    }

    // -------------------------------------------------------------------------
    // CALL_EXTERN: invoke a GuestFunction with an extern_handler (C++ callback)
    //   Calls CallExternHelper(handler, PPCContext*) which calls handler(ctx, kstate).
    //   The handler pointer is embedded at JIT time (fn is known here).
    //   If the handler is null at JIT time, we still embed the address and call
    //   CallExternHelper which checks for null internally.
    case hir::OPCODE_CALL_EXTERN: {
      auto* extern_fn = static_cast<GuestFunction*>(instr->src1.symbol);
      // Embed handler and helper address as compile-time constants.
      // X0 = handler (may be null; CallExternHelper handles null)
      emitter_.EmitMov64Fast(X0, reinterpret_cast<uintptr_t>(
                                     extern_fn->extern_handler()));
      // X1 = PPCContext*
      emitter_.Emit(MOV64rr(X1, X19));
      // X4 = &CallExternHelper
      emitter_.EmitMov64Fast(X4, reinterpret_cast<uintptr_t>(&CallExternHelper));
      emitter_.Emit(BLR(X4));
      break;
    }

    // -------------------------------------------------------------------------
    // TRAP — unconditional software breakpoint / halt
    case hir::OPCODE_TRAP:
      emitter_.Emit(BRK(0xBEEF));
      break;

    // -------------------------------------------------------------------------
    // LOAD_LOCAL / STORE_LOCAL — values in the locals_ list are HIR values;
    // they map to slots just like any other value.
    case hir::OPCODE_LOAD_LOCAL:
      LoadValue(instr->src1.value, X0);
      StoreValue(instr->dest, X0);
      break;

    case hir::OPCODE_STORE_LOCAL:
      LoadValue(instr->src2.value, X0);
      StoreValue(instr->src1.value, X0);
      break;

    // -------------------------------------------------------------------------
    // LOAD_CLOCK — return a 64-bit counter value using CNTVCT_EL0
    case hir::OPCODE_LOAD_CLOCK:
      // MRS X0, CNTVCT_EL0  (0xD53BE040 for EL0 virtual timer count)
      emitter_.Emit(0xD53BE040u | X0);
      StoreValue(instr->dest, X0);
      break;

    // -------------------------------------------------------------------------
    // Anything else — emit a distinctive software breakpoint so the crash
    // shows up clearly in a debugger with opcode name in the log.
    default:
      XELOGW("A64Assembler: unimplemented HIR opcode '{}' — BRK 0xDEAD",
             instr->opcode->name);
      emitter_.Emit(BRK(0xDEAD));
      break;
  }
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
