/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * ARM64 HIR Opcode Sequences — Part 1 of 3 (opcodes 1–36)
 *
 * Covers: control flow, type conversion, local/context/offset/mmio memory,
 *         vector shift-loads, clock, scalar min/max, select, comparisons,
 *         and scalar add/sub/mul/div/fma/neg/abs/sqrt/rsqrt/recip/pow2/log2.
 *
 * Endianness: Xbox 360 is big-endian; ARM64 is little-endian.
 *   - All guest memory loads byte-swap after load (REV/REV16/REV32/REV64).
 *   - All guest memory stores byte-swap before store.
 *   - Context struct accesses (via x19) are NOT swapped — the context is a
 *     host-layout struct, same as on x64.
 *   - Vector loads byte-swap the entire 128-bit register (REV64 + EXT lane
 *     swap, which is the NEON equivalent of the x64 vpshufd+vpshufb chain).
 *
 * Register conventions (see arm64_emitter.h for full table):
 *   x19 = PPC context ptr  (GetContextReg())
 *   x20 = host membase ptr (GetMembaseReg())
 *   x21–x27 = allocatable GPRs (callee-saved)
 *   v4–v15   = allocatable NEON regs
 *   x9–x13   = scratch GPRs  (ScratchReg(0..4))
 *   v16–v19  = scratch NEON  (ScratchVec(0..3))
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_sequences.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <atomic>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/backend/arm64/arm64_compat.h"
#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/backend/arm64/arm64_op.h"
#include "xenia/cpu/backend/arm64/arm64_stack_layout.h"
#include "xenia/cpu/hir/hir_builder.h"
#include "xenia/cpu/processor.h"

using namespace Arm64Gen;

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

using namespace xe::cpu;
using namespace xe::cpu::hir;

// DIAGNOSTIC: address of the guest function currently being emitted (defined in
// arm64_emitter.cc). Used to gate per-function spill-slot logging.
extern "C" uint32_t g_emit_fn_addr;
extern "C" uint32_t g_fa88_local_ops;

// Global dispatch table — populated by all EMITTER_OPCODE_TABLE macros
// across this file and its two companion part files. Function-local static so
// it is constructed before the first registration regardless of static
// initialization order across translation units.
std::unordered_map<uint32_t, SequenceSelectFn>& sequence_table() {
  static std::unordered_map<uint32_t, SequenceSelectFn> table;
  return table;
}

bool SelectSequence(ARM64Emitter* e, const Instr* i,
                    const Instr** new_tail) {
  auto key = InstrKey(i).value;
  auto& table = sequence_table();
  auto it = table.find(key);
  if (it != table.end()) {
    if (!it->second(*e, i)) {
      return false;
    }
    *new_tail = i;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Helper: compute effective guest address = membase (x20) + addr_reg/const.
// Leaves result in dest_reg. Uses ScratchReg(1) internally if addr is const.
// ---------------------------------------------------------------------------
static ARM64Reg ComputeEA(ARM64Emitter& e, const I64Op& addr,
                           ARM64Reg dest_reg) {
  // Guest addresses are 32-bit; the high 32 bits of the address register are
  // garbage (a PPC `addis rX,r0,SIMM` sign-extends, so any 0x8.../0x9... global
  // address arrives as 0xFFFFFFFF........). Adding membase to that full 64-bit
  // value wraps to a wrong host address. Zero-extend to 32 bits first — mirrors
  // the x64 backend (reg.cvt32() / masking the constant).
  if (addr.is_constant) {
    e.MOVI2R(dest_reg,
             static_cast<uint64_t>(static_cast<uint32_t>(addr.constant())));
    e.ADD(dest_reg, e.GetMembaseReg(), dest_reg);
  } else {
    e.MOV(EncodeRegTo32(dest_reg), EncodeRegTo32(addr.reg()));  // zero-extend
    e.ADD(dest_reg, e.GetMembaseReg(), dest_reg);
  }
  return dest_reg;
}

// Honor the LOAD_STORE_BYTE_SWAP flag: only byte-swap guest memory accesses
// when the frontend set it. Flag-0 (raw) accesses are paired with an explicit
// OPCODE_BYTE_SWAP, so an unconditional swap would double-swap. See part2.
static inline bool LSByteSwap(const hir::Instr* instr) {
  return (instr->flags & LOAD_STORE_BYTE_SWAP) != 0;
}

// ============================================================================
// OPCODE_COMMENT  (#1)
// Debug annotation — no code emitted in the JIT stream.
// ============================================================================
struct COMMENT : Sequence<COMMENT, I<OPCODE_COMMENT, VoidOp, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // No-op: comments are for debug display only.
  }
};
EMITTER_OPCODE_TABLE(OPCODE_COMMENT, COMMENT);

// ============================================================================
// OPCODE_NOP  (#2)
// ============================================================================
struct NOP : Sequence<NOP, I<OPCODE_NOP, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.NOP();
  }
};
EMITTER_OPCODE_TABLE(OPCODE_NOP, NOP);

// ============================================================================
// OPCODE_SOURCE_OFFSET  (#3)
// Records a guest PC → host PC mapping entry for the source map.
// ============================================================================
struct SOURCE_OFFSET
    : Sequence<SOURCE_OFFSET, I<OPCODE_SOURCE_OFFSET, VoidOp, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.MarkSourceOffset(i.instr);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SOURCE_OFFSET, SOURCE_OFFSET);

// ============================================================================
// OPCODE_DEBUG_BREAK / OPCODE_DEBUG_BREAK_TRUE  (#4–5)
// BRK #0 halts execution for a debugger. The TRUE variant is conditional.
// ============================================================================
struct DEBUG_BREAK : Sequence<DEBUG_BREAK, I<OPCODE_DEBUG_BREAK, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.DebugBreak();
  }
};
struct DEBUG_BREAK_TRUE
    : Sequence<DEBUG_BREAK_TRUE,
               I<OPCODE_DEBUG_BREAK_TRUE, VoidOp, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.DebugBreak();
      return;
    }
    // Break only if the condition (src1) is non-zero. CBZ skips over the BRK
    // when the condition register is zero.
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.DebugBreak();
    e.SetJumpTarget(skip);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_DEBUG_BREAK, DEBUG_BREAK);
EMITTER_OPCODE_TABLE(OPCODE_DEBUG_BREAK_TRUE, DEBUG_BREAK_TRUE);

// ============================================================================
// OPCODE_TRAP / OPCODE_TRAP_TRUE  (#6–7)
// BRK #trap_type — the exception handler reads the immediate to identify the
// trap reason (same role as UD2 on x64).
// ============================================================================
struct TRAP : Sequence<TRAP, I<OPCODE_TRAP, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.Trap(static_cast<uint16_t>(i.instr->flags));
  }
};
// TRAP_TRUE: the trap CODE is in i.instr->flags (NOT an operand — the prior
// signature wrongly added an OffsetOp src2, so it matched no real trap and the
// backend emitted an unconditional BRK placeholder → spurious SIGTRAP). The
// condition src1 can be any integer width (the PPC tw/td result), so register
// all four — mirrors the x64 backend (TRAP_TRUE_I8/I16/I32/I64).
struct TRAP_TRUE_I8
    : Sequence<TRAP_TRUE_I8, I<OPCODE_TRAP_TRUE, VoidOp, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.Trap(static_cast<uint16_t>(i.instr->flags));
      return;
    }
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.Trap(static_cast<uint16_t>(i.instr->flags));
    e.SetJumpTarget(skip);
  }
};
struct TRAP_TRUE_I16
    : Sequence<TRAP_TRUE_I16, I<OPCODE_TRAP_TRUE, VoidOp, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.Trap(static_cast<uint16_t>(i.instr->flags));
      return;
    }
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.Trap(static_cast<uint16_t>(i.instr->flags));
    e.SetJumpTarget(skip);
  }
};
struct TRAP_TRUE_I32
    : Sequence<TRAP_TRUE_I32, I<OPCODE_TRAP_TRUE, VoidOp, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.Trap(static_cast<uint16_t>(i.instr->flags));
      return;
    }
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.Trap(static_cast<uint16_t>(i.instr->flags));
    e.SetJumpTarget(skip);
  }
};
struct TRAP_TRUE_I64
    : Sequence<TRAP_TRUE_I64, I<OPCODE_TRAP_TRUE, VoidOp, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.Trap(static_cast<uint16_t>(i.instr->flags));
      return;
    }
    auto skip = e.CBZ(i.src1.reg());  // full 64-bit condition
    e.Trap(static_cast<uint16_t>(i.instr->flags));
    e.SetJumpTarget(skip);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_TRAP, TRAP);
EMITTER_OPCODE_TABLE(OPCODE_TRAP_TRUE, TRAP_TRUE_I8, TRAP_TRUE_I16,
                     TRAP_TRUE_I32, TRAP_TRUE_I64);

// ============================================================================
// OPCODE_CALL / OPCODE_CALL_TRUE  (#8–9)
// Direct calls to a known GuestFunction. The address is resolved at compile
// time; we emit a BL to the target (or to the resolve thunk on first call).
// ============================================================================
struct CALL : Sequence<CALL, I<OPCODE_CALL, VoidOp, SymbolOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.Call(i.instr, static_cast<GuestFunction*>(i.src1.value));
  }
};
struct CALL_TRUE
    : Sequence<CALL_TRUE, I<OPCODE_CALL_TRUE, VoidOp, I8Op, SymbolOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) {
        e.Call(i.instr, static_cast<GuestFunction*>(i.src2.value));
      }
      return;
    }
    // Conditional call: skip over the call when the condition byte is zero.
    // (This was a placeholder that ALWAYS called — which turned every
    // conditional-call/branch lowered through here into an unconditional one.)
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.Call(i.instr, static_cast<GuestFunction*>(i.src2.value));
    e.SetJumpTarget(skip);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CALL, CALL);
EMITTER_OPCODE_TABLE(OPCODE_CALL_TRUE, CALL_TRUE);

// ============================================================================
// OPCODE_CALL_INDIRECT / OPCODE_CALL_INDIRECT_TRUE  (#10–11)
// Indirect call: target guest address is in a register. We look it up in the
// indirection table and branch to the compiled host function.
// ============================================================================
struct CALL_INDIRECT
    : Sequence<CALL_INDIRECT, I<OPCODE_CALL_INDIRECT, VoidOp, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg target = i.src1.is_constant ? ARM64Emitter::ScratchReg(0)
                                         : i.src1.reg();
    if (i.src1.is_constant) {
      e.MOVI2R(target, static_cast<uint64_t>(i.src1.constant()));
    }
    e.CallIndirect(i.instr, target);
  }
};
struct CALL_INDIRECT_TRUE
    : Sequence<CALL_INDIRECT_TRUE,
               I<OPCODE_CALL_INDIRECT_TRUE, VoidOp, I8Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) {
        ARM64Reg target = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                             : i.src2.reg();
        if (i.src2.is_constant)
          e.MOVI2R(target, static_cast<uint64_t>(i.src2.constant()));
        e.CallIndirect(i.instr, target);
      }
      return;
    }
    // Conditional indirect call (e.g. conditional bclr/bcctr): skip the call
    // when the condition byte is zero. The old placeholder ALWAYS called,
    // which made every conditional return (bclr cr.X) return unconditionally —
    // e.g. the game's strchr returned non-NULL at the terminator, sending the
    // path tokenizer into a table-overflow runaway.
    ARM64Reg target = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                         : i.src2.reg();
    if (i.src2.is_constant)
      e.MOVI2R(target, static_cast<uint64_t>(i.src2.constant()));
    auto skip = e.CBZ(EncodeRegTo32(i.src1.reg()));
    e.CallIndirect(i.instr, target);
    e.SetJumpTarget(skip);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CALL_INDIRECT, CALL_INDIRECT);
EMITTER_OPCODE_TABLE(OPCODE_CALL_INDIRECT_TRUE, CALL_INDIRECT_TRUE);

// ============================================================================
// OPCODE_CALL_EXTERN  (#12)
// Call to a host-side HLE export handler. Same as CallIndirect but through
// the GuestToHost thunk.
// ============================================================================
struct CALL_EXTERN
    : Sequence<CALL_EXTERN, I<OPCODE_CALL_EXTERN, VoidOp, SymbolOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.CallExtern(i.instr, i.src1.value);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CALL_EXTERN, CALL_EXTERN);

// ============================================================================
// OPCODE_RETURN / OPCODE_RETURN_TRUE  (#13–14)
// Jump to the function epilog. RETURN_TRUE does so only if src1 != 0.
// ============================================================================
struct RETURN : Sequence<RETURN, I<OPCODE_RETURN, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Unconditional branch to epilog.
    // The epilog label is registered at function-end; we emit a fixup branch.
    auto f = e.GetEpilogFixup();
    // Dolphin's ARM64CodeBlock::SetJumpTarget(f) will be called at epilog.
    (void)f;
  }
};
struct RETURN_TRUE
    : Sequence<RETURN_TRUE, I<OPCODE_RETURN_TRUE, VoidOp, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) {
        auto f = e.GetEpilogFixup();
        (void)f;
      }
      return;
    }
    // CBNZ: branch to epilog if condition register is non-zero.
    e.AddEpilogFixup(e.CBNZ(EncodeRegTo32(i.src1.reg())));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_RETURN, RETURN);
EMITTER_OPCODE_TABLE(OPCODE_RETURN_TRUE, RETURN_TRUE);

// ============================================================================
// OPCODE_SET_RETURN_ADDRESS  (#15)
// Stores the PPC return address into the GUEST_RET_ADDR stack slot.
// ============================================================================
struct SET_RETURN_ADDRESS
    : Sequence<SET_RETURN_ADDRESS,
               I<OPCODE_SET_RETURN_ADDRESS, VoidOp, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src1.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : i.src1.reg();
    if (i.src1.is_constant)
      e.MOVI2R(src, static_cast<uint64_t>(i.src1.constant()));
    // Store into the OUTGOING slot — this is the return address that the NEXT
    // Call() will hand to its callee (loaded into x2 before the BLR). The
    // function's own incoming return address lives in GUEST_RET_ADDR (saved by
    // the prolog) and must not be clobbered here.
    e.STR(INDEX_UNSIGNED, src, SP, StackLayout::GUEST_CALL_RET_ADDR);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SET_RETURN_ADDRESS, SET_RETURN_ADDRESS);

// ============================================================================
// OPCODE_BRANCH / OPCODE_BRANCH_TRUE / OPCODE_BRANCH_FALSE  (#16–18)
// Intra-function branches to HIR labels (which map to basic block entries).
// ============================================================================
struct BRANCH : Sequence<BRANCH, I<OPCODE_BRANCH, VoidOp, LabelOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.BranchLabel(i.src1.value);
  }
};
struct BRANCH_TRUE
    : Sequence<BRANCH_TRUE, I<OPCODE_BRANCH_TRUE, VoidOp, I8Op, LabelOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (i.src1.constant()) e.BranchLabel(i.src2.value);
      return;
    }
    e.BranchLabelIfNZ(EncodeRegTo32(i.src1.reg()), i.src2.value);
  }
};
struct BRANCH_FALSE
    : Sequence<BRANCH_FALSE, I<OPCODE_BRANCH_FALSE, VoidOp, I8Op, LabelOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      if (!i.src1.constant()) e.BranchLabel(i.src2.value);
      return;
    }
    e.BranchLabelIfZ(EncodeRegTo32(i.src1.reg()), i.src2.value);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_BRANCH, BRANCH);
EMITTER_OPCODE_TABLE(OPCODE_BRANCH_TRUE, BRANCH_TRUE);
EMITTER_OPCODE_TABLE(OPCODE_BRANCH_FALSE, BRANCH_FALSE);

// ============================================================================
// OPCODE_ASSIGN  (#19 — between BRANCH_FALSE and CAST in canonical order)
// Move a value from src to dest, same type. For registers this is a MOV;
// for constants it's an immediate load.
// ============================================================================
struct ASSIGN_I8 : Sequence<ASSIGN_I8, I<OPCODE_ASSIGN, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.MOVI2R(EncodeRegTo32(i.dest.reg()),
               static_cast<uint32_t>(i.src1.constant()) & 0xFF);
    else
      e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ASSIGN_I16 : Sequence<ASSIGN_I16, I<OPCODE_ASSIGN, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.MOVI2R(EncodeRegTo32(i.dest.reg()),
               static_cast<uint32_t>(i.src1.constant()) & 0xFFFF);
    else
      e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ASSIGN_I32 : Sequence<ASSIGN_I32, I<OPCODE_ASSIGN, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.MOVI2R(EncodeRegTo32(i.dest.reg()),
               static_cast<uint32_t>(i.src1.constant()));
    else
      e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ASSIGN_I64 : Sequence<ASSIGN_I64, I<OPCODE_ASSIGN, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.MOVI2R(i.dest.reg(), static_cast<uint64_t>(i.src1.constant()));
    else
      e.MOV(i.dest.reg(), i.src1.reg());
  }
};
struct ASSIGN_F32 : Sequence<ASSIGN_F32, I<OPCODE_ASSIGN, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.LoadConstantF32(i.dest.reg(), i.src1.constant());
    else
      e.FMOV(EncodeRegToSingle(i.dest.reg()),
             EncodeRegToSingle(i.src1.reg()));
  }
};
struct ASSIGN_F64 : Sequence<ASSIGN_F64, I<OPCODE_ASSIGN, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.LoadConstantF64(i.dest.reg(), i.src1.constant());
    else
      e.FMOV(EncodeRegToDouble(i.dest.reg()),
             EncodeRegToDouble(i.src1.reg()));
  }
};
struct ASSIGN_V128 : Sequence<ASSIGN_V128, I<OPCODE_ASSIGN, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.LoadConstantV128(i.dest.reg(), i.src1.constant());
    else
      // ORR Vd.16b, Vn.16b, Vn.16b is the canonical 128-bit NEON move.
      e.ORR(i.dest.reg(), i.src1.reg(), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ASSIGN, ASSIGN_I8, ASSIGN_I16, ASSIGN_I32,
                     ASSIGN_I64, ASSIGN_F32, ASSIGN_F64, ASSIGN_V128);

// ============================================================================
// OPCODE_CAST  (#20)
// Bit-reinterpret between int and float of the same width.
// No arithmetic conversion — just FMOV (moves between int and float register
// files while preserving the bit pattern).
// ============================================================================
struct CAST_I32_F32 : Sequence<CAST_I32_F32, I<OPCODE_CAST, I32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FMOV Wd, Sn: float register → int register, bits unchanged.
    e.FMOV(EncodeRegTo32(i.dest.reg()), EncodeRegToSingle(i.src1.reg()));
  }
};
struct CAST_I64_F64 : Sequence<CAST_I64_F64, I<OPCODE_CAST, I64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(i.dest.reg(), EncodeRegToDouble(i.src1.reg()));
  }
};
struct CAST_F32_I32 : Sequence<CAST_F32_I32, I<OPCODE_CAST, F32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FMOV Sd, Wn: int register → float register, bits unchanged.
    e.FMOV(EncodeRegToSingle(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct CAST_F64_I64 : Sequence<CAST_F64_I64, I<OPCODE_CAST, F64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(EncodeRegToDouble(i.dest.reg()), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CAST, CAST_I32_F32, CAST_I64_F64, CAST_F32_I32,
                     CAST_F64_I64);

// ============================================================================
// OPCODE_ZERO_EXTEND  (#21 — SIGN_EXTEND is after this in HIR order)
// Unsigned widening. On ARM64, writing to a 32-bit register implicitly
// zero-extends to 64 bits, so I64←I32 is just a MOV Wd, Wm.
// ============================================================================
struct ZERO_EXTEND_I16_I8
    : Sequence<ZERO_EXTEND_I16_I8, I<OPCODE_ZERO_EXTEND, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // UXTB zero-extends byte to 32 bits (upper 32 bits of Xd are zeroed).
    e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ZERO_EXTEND_I32_I8
    : Sequence<ZERO_EXTEND_I32_I8, I<OPCODE_ZERO_EXTEND, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ZERO_EXTEND_I64_I8
    : Sequence<ZERO_EXTEND_I64_I8, I<OPCODE_ZERO_EXTEND, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // UXTB into 32-bit dest → upper 32 bits auto-zeroed → 64-bit zero-extend.
    e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ZERO_EXTEND_I32_I16
    : Sequence<ZERO_EXTEND_I32_I16, I<OPCODE_ZERO_EXTEND, I32Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ZERO_EXTEND_I64_I16
    : Sequence<ZERO_EXTEND_I64_I16, I<OPCODE_ZERO_EXTEND, I64Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct ZERO_EXTEND_I64_I32
    : Sequence<ZERO_EXTEND_I64_I32, I<OPCODE_ZERO_EXTEND, I64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // MOV Wd, Wm zero-extends to 64 bits implicitly on AArch64.
    e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ZERO_EXTEND, ZERO_EXTEND_I16_I8,
                     ZERO_EXTEND_I32_I8, ZERO_EXTEND_I64_I8,
                     ZERO_EXTEND_I32_I16, ZERO_EXTEND_I64_I16,
                     ZERO_EXTEND_I64_I32);

// ============================================================================
// OPCODE_SIGN_EXTEND  (#22 — same slot as ZERO_EXTEND in canonical ordering)
// Signed widening.
// ============================================================================
struct SIGN_EXTEND_I16_I8
    : Sequence<SIGN_EXTEND_I16_I8, I<OPCODE_SIGN_EXTEND, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct SIGN_EXTEND_I32_I8
    : Sequence<SIGN_EXTEND_I32_I8, I<OPCODE_SIGN_EXTEND, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct SIGN_EXTEND_I64_I8
    : Sequence<SIGN_EXTEND_I64_I8, I<OPCODE_SIGN_EXTEND, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // SXTB into 64-bit dest.
    e.SXTB(i.dest.reg(), EncodeRegTo32(i.src1.reg()));
  }
};
struct SIGN_EXTEND_I32_I16
    : Sequence<SIGN_EXTEND_I32_I16, I<OPCODE_SIGN_EXTEND, I32Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct SIGN_EXTEND_I64_I16
    : Sequence<SIGN_EXTEND_I64_I16, I<OPCODE_SIGN_EXTEND, I64Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTH(i.dest.reg(), EncodeRegTo32(i.src1.reg()));
  }
};
struct SIGN_EXTEND_I64_I32
    : Sequence<SIGN_EXTEND_I64_I32, I<OPCODE_SIGN_EXTEND, I64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTW(i.dest.reg(), EncodeRegTo32(i.src1.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SIGN_EXTEND, SIGN_EXTEND_I16_I8,
                     SIGN_EXTEND_I32_I8, SIGN_EXTEND_I64_I8,
                     SIGN_EXTEND_I32_I16, SIGN_EXTEND_I64_I16,
                     SIGN_EXTEND_I64_I32);

// ============================================================================
// OPCODE_TRUNCATE  (#23)
// Narrowing: mask off upper bits. ARM64 doesn't have a TRUNCATE instruction
// so we AND down to the target width.
// ============================================================================
struct TRUNCATE_I8_I16
    : Sequence<TRUNCATE_I8_I16, I<OPCODE_TRUNCATE, I8Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), 0xFF);
  }
};
struct TRUNCATE_I8_I32
    : Sequence<TRUNCATE_I8_I32, I<OPCODE_TRUNCATE, I8Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), 0xFF);
  }
};
struct TRUNCATE_I8_I64
    : Sequence<TRUNCATE_I8_I64, I<OPCODE_TRUNCATE, I8Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), 0xFF);
  }
};
struct TRUNCATE_I16_I32
    : Sequence<TRUNCATE_I16_I32, I<OPCODE_TRUNCATE, I16Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), 0xFFFF);
  }
};
struct TRUNCATE_I16_I64
    : Sequence<TRUNCATE_I16_I64, I<OPCODE_TRUNCATE, I16Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), 0xFFFF);
  }
};
struct TRUNCATE_I32_I64
    : Sequence<TRUNCATE_I32_I64, I<OPCODE_TRUNCATE, I32Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // MOV Wd, Wm keeps only the low 32 bits.
    e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_TRUNCATE, TRUNCATE_I8_I16, TRUNCATE_I8_I32,
                     TRUNCATE_I8_I64, TRUNCATE_I16_I32, TRUNCATE_I16_I64,
                     TRUNCATE_I32_I64);

// ============================================================================
// OPCODE_CONVERT  (#24)
// Integer ↔ float arithmetic conversion (with rounding).
// ============================================================================
struct CONVERT_I32_F32
    : Sequence<CONVERT_I32_F32, I<OPCODE_CONVERT, I32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FCVTZS: float → signed int, rounding toward zero.
    e.FCVTZS(EncodeRegTo32(i.dest.reg()),
             EncodeRegToSingle(i.src1.reg()));
  }
};
struct CONVERT_I32_F64
    : Sequence<CONVERT_I32_F64, I<OPCODE_CONVERT, I32Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FCVTZS(EncodeRegTo32(i.dest.reg()),
             EncodeRegToDouble(i.src1.reg()));
  }
};
struct CONVERT_I64_F64
    : Sequence<CONVERT_I64_F64, I<OPCODE_CONVERT, I64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FCVTZS(i.dest.reg(), EncodeRegToDouble(i.src1.reg()));
  }
};
struct CONVERT_F32_I32
    : Sequence<CONVERT_F32_I32, I<OPCODE_CONVERT, F32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // SCVTF: signed int → float.
    e.SCVTF(EncodeRegToSingle(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct CONVERT_F32_F64
    : Sequence<CONVERT_F32_F64, I<OPCODE_CONVERT, F32Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FCVT(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()));
  }
};
struct CONVERT_F64_I64
    : Sequence<CONVERT_F64_I64, I<OPCODE_CONVERT, F64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SCVTF(EncodeRegToDouble(i.dest.reg()), i.src1.reg());
  }
};
struct CONVERT_F64_F32
    : Sequence<CONVERT_F64_F32, I<OPCODE_CONVERT, F64Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FCVT(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CONVERT, CONVERT_I32_F32, CONVERT_I32_F64,
                     CONVERT_I64_F64, CONVERT_F32_I32, CONVERT_F32_F64,
                     CONVERT_F64_I64, CONVERT_F64_F32);

// ============================================================================
// OPCODE_ROUND  (#25)
// Rounding mode in src2 offset (ROUND_TO_NEAREST / ZERO / FLOOR / CEIL).
// ARM64 has dedicated instructions for each mode — exactly 1 cycle each.
// ============================================================================
struct ROUND_F32
    : Sequence<ROUND_F32, I<OPCODE_ROUND, F32Op, F32Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    auto d = EncodeRegToSingle(i.dest.reg());
    auto s = EncodeRegToSingle(i.src1.reg());
    switch (i.src2.value) {
      case ROUND_TO_NEAREST:
        e.FRINTN(d, s);  // round to nearest, ties to even (IEEE default)
        break;
      case ROUND_TO_ZERO:
        e.FRINTZ(d, s);  // truncate toward zero
        break;
      case ROUND_TO_MINUS_INFINITY:
        e.FRINTM(d, s);  // floor
        break;
      case ROUND_TO_POSITIVE_INFINITY:
        e.FRINTP(d, s);  // ceil
        break;
      default:
        e.FRINTI(d, s);  // round using current FPCR rounding mode
        break;
    }
  }
};
struct ROUND_F64
    : Sequence<ROUND_F64, I<OPCODE_ROUND, F64Op, F64Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    auto d = EncodeRegToDouble(i.dest.reg());
    auto s = EncodeRegToDouble(i.src1.reg());
    switch (i.src2.value) {
      case ROUND_TO_NEAREST:        e.FRINTN(d, s); break;
      case ROUND_TO_ZERO:           e.FRINTZ(d, s); break;
      case ROUND_TO_MINUS_INFINITY: e.FRINTM(d, s); break;
      case ROUND_TO_POSITIVE_INFINITY: e.FRINTP(d, s); break;
      default:                      e.FRINTI(d, s); break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ROUND, ROUND_F32, ROUND_F64);

// ============================================================================
// OPCODE_LOAD_VECTOR_SHL  (#26)
// Loads a 128-bit shift-left mask vector based on a byte count.
// Used for lvsl (load vector for shift-left) — a VMX-128 specific operation.
// The result is a permute control vector where byte N = N + shift_amount.
// We build it in a helper and load from a precomputed table.
// ============================================================================
struct LOAD_VECTOR_SHL_I8
    : Sequence<LOAD_VECTOR_SHL_I8,
               I<OPCODE_LOAD_VECTOR_SHL, V128Op, I8Op>> {
  // Precomputed lvsl table: 16 rows of 16 bytes.
  // Row N contains [N, N+1, N+2, ..., N+15] (clamped to 31 for out-of-range).
  static const uint8_t lvsl_table[16][16];

  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // shift_amount = src1 & 0xF  (only low 4 bits matter)
    ARM64Reg shift = i.src1.is_constant
                         ? ARM64Emitter::ScratchReg(0)
                         : EncodeRegTo32(i.src1.reg());
    if (i.src1.is_constant) {
      e.MOVI2R(shift, static_cast<uint32_t>(i.src1.constant()) & 0xF);
    } else {
      e.AND(shift, shift, 0xF);
    }

    // Load the precomputed table address into a scratch register.
    ARM64Reg table_ptr = ARM64Emitter::ScratchReg(1);
    e.MOVI2R(table_ptr, reinterpret_cast<uint64_t>(lvsl_table));

    // Multiply shift by 16 to get the row offset: shift_bytes = shift << 4.
    ARM64Reg row_off = ARM64Emitter::ScratchReg(2);
    e.LSL(row_off, shift, 4);

    // Load the 16-byte row: LDR Qd, [table_ptr + row_off]
    e.ADD(table_ptr, table_ptr, row_off);
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), table_ptr, 0);
  }
};

// lvsl_table[N][j] = N + j, values are byte indices for the permute vector.
const uint8_t LOAD_VECTOR_SHL_I8::lvsl_table[16][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    { 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16},
    { 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17},
    { 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18},
    { 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19},
    { 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20},
    { 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21},
    { 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22},
    { 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23},
    { 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24},
    {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25},
    {11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26},
    {12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27},
    {13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28},
    {14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29},
    {15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30},
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_VECTOR_SHL, LOAD_VECTOR_SHL_I8);

// ============================================================================
// OPCODE_LOAD_VECTOR_SHR  (#27)
// Loads a 128-bit shift-right mask vector (lvsr).
// lvsr_table[N][j] = 16 - N + j.
// ============================================================================
struct LOAD_VECTOR_SHR_I8
    : Sequence<LOAD_VECTOR_SHR_I8,
               I<OPCODE_LOAD_VECTOR_SHR, V128Op, I8Op>> {
  static const uint8_t lvsr_table[16][16];

  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg shift = i.src1.is_constant
                         ? ARM64Emitter::ScratchReg(0)
                         : EncodeRegTo32(i.src1.reg());
    if (i.src1.is_constant) {
      e.MOVI2R(shift, static_cast<uint32_t>(i.src1.constant()) & 0xF);
    } else {
      e.AND(shift, shift, 0xF);
    }
    ARM64Reg table_ptr = ARM64Emitter::ScratchReg(1);
    e.MOVI2R(table_ptr, reinterpret_cast<uint64_t>(lvsr_table));
    ARM64Reg row_off = ARM64Emitter::ScratchReg(2);
    e.LSL(row_off, shift, 4);
    e.ADD(table_ptr, table_ptr, row_off);
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), table_ptr, 0);
  }
};
const uint8_t LOAD_VECTOR_SHR_I8::lvsr_table[16][16] = {
    {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31},
    {15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30},
    {14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29},
    {13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28},
    {12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27},
    {11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26},
    {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25},
    { 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24},
    { 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23},
    { 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22},
    { 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21},
    { 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20},
    { 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19},
    { 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18},
    { 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17},
    { 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16},
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_VECTOR_SHR, LOAD_VECTOR_SHR_I8);

// ============================================================================
// OPCODE_LOAD_CLOCK  (#28)
// Reads the hardware performance counter (equivalent to RDTSC on x86).
// ARM64: MRS Xd, CNTVCT_EL0 reads the virtual counter at ~100ns resolution.
// ============================================================================
struct LOAD_CLOCK : Sequence<LOAD_CLOCK, I<OPCODE_LOAD_CLOCK, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // CNTVCT_EL0 is the virtual count register, accessible from EL0 on
    // Android (CNTKCTL_EL1.EL0VCTEN must be set, which Android does).
    e.CNTVCT(i.dest.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_CLOCK, LOAD_CLOCK);

// ============================================================================
// OPCODE_LOAD_LOCAL / OPCODE_STORE_LOCAL  (#29–30)
// Access to HIR-local spill slots on the host stack. src1 (OffsetOp) holds the
// ABSOLUTE byte offset from SP, assigned in EmitFunction()'s local-allocation
// pass (locals live above the fixed frame region, at >= GUEST_STACK_SIZE, so
// they never overlap the saved callee registers or the host caller's frame).
// The offset is naturally aligned to the access size by that pass.
// ============================================================================
struct LOAD_LOCAL_I8
    : Sequence<LOAD_LOCAL_I8, I<OPCODE_LOAD_LOCAL, I8Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDRB(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_I16
    : Sequence<LOAD_LOCAL_I16, I<OPCODE_LOAD_LOCAL, I16Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDRH(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_I32
    : Sequence<LOAD_LOCAL_I32, I<OPCODE_LOAD_LOCAL, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_I64
    : Sequence<LOAD_LOCAL_I64, I<OPCODE_LOAD_LOCAL, I64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_F32
    : Sequence<LOAD_LOCAL_F32, I<OPCODE_LOAD_LOCAL, F32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegToSingle(i.dest.reg()), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_F64
    : Sequence<LOAD_LOCAL_F64, I<OPCODE_LOAD_LOCAL, F64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegToDouble(i.dest.reg()), SP, i.src1.constant());
  }
};
struct LOAD_LOCAL_V128
    : Sequence<LOAD_LOCAL_V128, I<OPCODE_LOAD_LOCAL, V128Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), SP, i.src1.constant());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_LOCAL, LOAD_LOCAL_I8, LOAD_LOCAL_I16,
                     LOAD_LOCAL_I32, LOAD_LOCAL_I64, LOAD_LOCAL_F32,
                     LOAD_LOCAL_F64, LOAD_LOCAL_V128);

struct STORE_LOCAL_I8
    : Sequence<STORE_LOCAL_I8, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()) & 0xFF);
    e.STRB(INDEX_UNSIGNED, src, SP, i.src1.constant());
  }
};
struct STORE_LOCAL_I16
    : Sequence<STORE_LOCAL_I16, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()) & 0xFFFF);
    e.STRH(INDEX_UNSIGNED, src, SP, i.src1.constant());
  }
};
struct STORE_LOCAL_I32
    : Sequence<STORE_LOCAL_I32, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()));
    e.STR(INDEX_UNSIGNED, src, SP, i.src1.constant());
  }
};
struct STORE_LOCAL_I64
    : Sequence<STORE_LOCAL_I64, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : i.src2.reg();
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint64_t>(i.src2.constant()));
    e.STR(INDEX_UNSIGNED, src, SP, i.src1.constant());
  }
};
struct STORE_LOCAL_F32
    : Sequence<STORE_LOCAL_F32, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, EncodeRegToSingle(src), SP, i.src1.constant());
  }
};
struct STORE_LOCAL_F64
    : Sequence<STORE_LOCAL_F64, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, EncodeRegToDouble(src), SP, i.src1.constant());
  }
};
struct STORE_LOCAL_V128
    : Sequence<STORE_LOCAL_V128, I<OPCODE_STORE_LOCAL, VoidOp, I32Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, src, SP, i.src1.constant());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_STORE_LOCAL, STORE_LOCAL_I8, STORE_LOCAL_I16,
                     STORE_LOCAL_I32, STORE_LOCAL_I64, STORE_LOCAL_F32,
                     STORE_LOCAL_F64, STORE_LOCAL_V128);

// ============================================================================
// OPCODE_LOAD_CONTEXT / OPCODE_STORE_CONTEXT  (#31–32)
// Read/write the PPC register file struct (pointed to by x19).
// No endian swap — the context struct is host-layout (same as x64 backend).
// ============================================================================
struct LOAD_CONTEXT_I8
    : Sequence<LOAD_CONTEXT_I8, I<OPCODE_LOAD_CONTEXT, I8Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDRB(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()),
           e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_I16
    : Sequence<LOAD_CONTEXT_I16, I<OPCODE_LOAD_CONTEXT, I16Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDRH(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()),
           e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_I32
    : Sequence<LOAD_CONTEXT_I32, I<OPCODE_LOAD_CONTEXT, I32Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()),
          e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_I64
    : Sequence<LOAD_CONTEXT_I64, I<OPCODE_LOAD_CONTEXT, I64Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_F32
    : Sequence<LOAD_CONTEXT_F32, I<OPCODE_LOAD_CONTEXT, F32Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegToSingle(i.dest.reg()),
          e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_F64
    : Sequence<LOAD_CONTEXT_F64, I<OPCODE_LOAD_CONTEXT, F64Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, EncodeRegToDouble(i.dest.reg()),
          e.GetContextReg(), i.src1.value);
  }
};
struct LOAD_CONTEXT_V128
    : Sequence<LOAD_CONTEXT_V128, I<OPCODE_LOAD_CONTEXT, V128Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), e.GetContextReg(), i.src1.value);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_CONTEXT, LOAD_CONTEXT_I8, LOAD_CONTEXT_I16,
                     LOAD_CONTEXT_I32, LOAD_CONTEXT_I64, LOAD_CONTEXT_F32,
                     LOAD_CONTEXT_F64, LOAD_CONTEXT_V128);

struct STORE_CONTEXT_I8
    : Sequence<STORE_CONTEXT_I8,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant
                       ? ARM64Emitter::ScratchReg(0)
                       : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()) & 0xFF);
    e.STRB(INDEX_UNSIGNED, src, e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_I16
    : Sequence<STORE_CONTEXT_I16,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant
                       ? ARM64Emitter::ScratchReg(0)
                       : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()) & 0xFFFF);
    e.STRH(INDEX_UNSIGNED, src, e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_I32
    : Sequence<STORE_CONTEXT_I32,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant
                       ? ARM64Emitter::ScratchReg(0)
                       : EncodeRegTo32(i.src2.reg());
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src2.constant()));
    e.STR(INDEX_UNSIGNED, src, e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_I64
    : Sequence<STORE_CONTEXT_I64,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : i.src2.reg();
    if (i.src2.is_constant)
      e.MOVI2R(src, static_cast<uint64_t>(i.src2.constant()));
    e.STR(INDEX_UNSIGNED, src, e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_F32
    : Sequence<STORE_CONTEXT_F32,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, EncodeRegToSingle(src),
          e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_F64
    : Sequence<STORE_CONTEXT_F64,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, EncodeRegToDouble(src),
          e.GetContextReg(), i.src1.value);
  }
};
struct STORE_CONTEXT_V128
    : Sequence<STORE_CONTEXT_V128,
               I<OPCODE_STORE_CONTEXT, VoidOp, OffsetOp, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(src, i.src2.constant());
    e.STR(INDEX_UNSIGNED, src, e.GetContextReg(), i.src1.value);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_STORE_CONTEXT, STORE_CONTEXT_I8, STORE_CONTEXT_I16,
                     STORE_CONTEXT_I32, STORE_CONTEXT_I64, STORE_CONTEXT_F32,
                     STORE_CONTEXT_F64, STORE_CONTEXT_V128);

// ============================================================================
// OPCODE_CONTEXT_BARRIER  (#33)
// Ensures context writes are visible. On ARM64, normal stores to the context
// struct (ordinary memory) are ordered by the ISA, so this is a no-op.
// ============================================================================
struct CONTEXT_BARRIER
    : Sequence<CONTEXT_BARRIER, I<OPCODE_CONTEXT_BARRIER, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) { e.NOP(); }
};
EMITTER_OPCODE_TABLE(OPCODE_CONTEXT_BARRIER, CONTEXT_BARRIER);

// ============================================================================
// OPCODE_LOAD_MMIO / OPCODE_STORE_MMIO  (#34–35)
// MMIO accesses are dispatched through C++ handler callbacks. The address and
// handler pointer are compile-time constants embedded in the instruction.
// ============================================================================

// C-linkage thunks called by the JIT for MMIO load/store.
// src1.offset holds an MMIORange* (set by HIRBuilder::LoadMmio/StoreMmio and
// the constant-propagation pass) — NOT an MMIOHandler*. The old code cast it
// to MMIOHandler and called CheckLoad, which read the range's fields as if
// they were the handler's mapped_ranges_ vector and ended up branching to XEX
// instruction bytes (SIGBUS pc=3D80...3D80...).
// Byte order matches x64: the HIR's following/preceding BYTE_SWAP survives the
// LOAD→LOAD_MMIO replacement, and MMIO callbacks speak host-endian, so the
// load result is pre-swapped (to be un-swapped by the HIR) and the store value
// is swapped back to host order before the callback.
static uint32_t MMIOLoad32(void* context, void* range_ptr,
                           uint32_t guest_addr) {
  auto* range = reinterpret_cast<xe::cpu::MMIORange*>(range_ptr);
  uint32_t result = static_cast<uint32_t>(
      range->read(nullptr, range->callback_context, guest_addr));
  // DIAGNOSTIC (rate-limited): which MMIO regs does the guest poll, and what
  // do we hand back? result logged in host order (pre-swap).
  static std::atomic<int> logs{0};
  if (logs.fetch_add(1, std::memory_order_relaxed) < 64) {
    XELOGW("MMIO load32 [{:08X}] -> {:08X}", guest_addr, result);
  }
  return xe::byte_swap(result);
}
static void MMIOStore32(void* context, void* range_ptr, uint32_t guest_addr,
                        uint32_t value) {
  auto* range = reinterpret_cast<xe::cpu::MMIORange*>(range_ptr);
  uint32_t v = xe::byte_swap(value);
  static std::atomic<int> logs{0};
  if (logs.fetch_add(1, std::memory_order_relaxed) < 64) {
    XELOGW("MMIO store32 [{:08X}] <- {:08X}", guest_addr, v);
  }
  range->write(nullptr, range->callback_context, guest_addr, v);
}

struct LOAD_MMIO_I32
    : Sequence<LOAD_MMIO_I32,
               I<OPCODE_LOAD_MMIO, I32Op, OffsetOp, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // src1 = MMIORange* (OffsetOp stores pointer as uint64),
    // src2 = guest_address (OffsetOp).
    // We call MMIOLoad32(context, range, guest_addr).
    // x0 = context (x19), x1 = range_ptr, x2 = guest_addr
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(X1, i.src1.value);
    e.MOVI2R(EncodeRegTo32(X2), static_cast<uint32_t>(i.src2.value));
    e.MOVI2R(ARM64Emitter::ScratchReg(3),
             reinterpret_cast<uint64_t>(MMIOLoad32));
    e.BLR(ARM64Emitter::ScratchReg(3));
    // Result is in W0; move to dest.
    e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(X0));
  }
};
struct STORE_MMIO_I32
    : Sequence<STORE_MMIO_I32,
               I<OPCODE_STORE_MMIO, VoidOp, OffsetOp, OffsetOp, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // HIR operand order (hir_builder.cc StoreMmio / constant-propagation):
    // src1 = MMIORange* (OffsetOp), src2 = guest_address (OffsetOp),
    // src3 = value (I32, reg or constant). Matches x64_seq_memory.cc.
    // Capture the value FIRST: src3 may live in any allocated register, and
    // it must be staged into W3 before we overwrite X0-X2 (none of which can
    // hold an allocated value, but keep the order defensive anyway).
    if (i.src3.is_constant) {
      e.MOVI2R(EncodeRegTo32(X3), static_cast<uint32_t>(i.src3.constant()));
    } else {
      e.MOV(EncodeRegTo32(X3), EncodeRegTo32(i.src3.reg()));
    }
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(X1, i.src1.value);
    e.MOVI2R(EncodeRegTo32(X2), static_cast<uint32_t>(i.src2.value));
    e.MOVI2R(ARM64Emitter::ScratchReg(3),
             reinterpret_cast<uint64_t>(MMIOStore32));
    e.BLR(ARM64Emitter::ScratchReg(3));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_MMIO, LOAD_MMIO_I32);
EMITTER_OPCODE_TABLE(OPCODE_STORE_MMIO, STORE_MMIO_I32);

// ============================================================================
// OPCODE_LOAD_OFFSET / OPCODE_STORE_OFFSET  (#36)
// Like LOAD/STORE but with a compile-time constant byte offset added to
// the address. Used for struct member accesses.
// Byte-swap rules are identical to LOAD/STORE.
// ============================================================================
struct LOAD_OFFSET_I8
    : Sequence<LOAD_OFFSET_I8,
               I<OPCODE_LOAD_OFFSET, I8Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    e.LDRB(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
  }
};
struct LOAD_OFFSET_I16
    : Sequence<LOAD_OFFSET_I16,
               I<OPCODE_LOAD_OFFSET, I16Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    e.LDRH(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
    if (LSByteSwap(i.instr))
      e.REV16(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
  }
};
struct LOAD_OFFSET_I32
    : Sequence<LOAD_OFFSET_I32,
               I<OPCODE_LOAD_OFFSET, I32Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
    if (LSByteSwap(i.instr))
      e.REV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
    e.EmitLoadTrace(ea, EncodeRegTo32(i.dest.reg()));
  }
};
struct LOAD_OFFSET_I64
    : Sequence<LOAD_OFFSET_I64,
               I<OPCODE_LOAD_OFFSET, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), ea, 0);
    if (LSByteSwap(i.instr)) e.REV(i.dest.reg(), i.dest.reg());
  }
};
struct LOAD_OFFSET_F32
    : Sequence<LOAD_OFFSET_F32,
               I<OPCODE_LOAD_OFFSET, F32Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(tmp), ea, 0);
    if (LSByteSwap(i.instr)) e.REV(EncodeRegTo32(tmp), EncodeRegTo32(tmp));
    e.FMOV(EncodeRegToSingle(i.dest.reg()), EncodeRegTo32(tmp));
  }
};
struct LOAD_OFFSET_F64
    : Sequence<LOAD_OFFSET_F64,
               I<OPCODE_LOAD_OFFSET, F64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.LDR(INDEX_UNSIGNED, tmp, ea, 0);
    if (LSByteSwap(i.instr)) e.REV(tmp, tmp);
    e.FMOV(EncodeRegToDouble(i.dest.reg()), tmp);
  }
};
struct LOAD_OFFSET_V128
    : Sequence<LOAD_OFFSET_V128,
               I<OPCODE_LOAD_OFFSET, V128Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    // Load then full 128-bit byte-swap (big→little endian lane reversal).
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), ea, 0);
    if (LSByteSwap(i.instr)) {
      e.REV64(i.dest.reg(), i.dest.reg());  // reverse bytes in each 64-bit lane
      e.EXT(i.dest.reg(), i.dest.reg(), i.dest.reg(), 8);  // swap two 64b lanes
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD_OFFSET, LOAD_OFFSET_I8, LOAD_OFFSET_I16,
                     LOAD_OFFSET_I32, LOAD_OFFSET_I64, LOAD_OFFSET_F32,
                     LOAD_OFFSET_F64, LOAD_OFFSET_V128);

struct STORE_OFFSET_I8
    : Sequence<STORE_OFFSET_I8,
               I<OPCODE_STORE_OFFSET, VoidOp, I64Op, I64Op,I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    ARM64Reg val = i.src3.is_constant ? ARM64Emitter::ScratchReg(1)
                                      : EncodeRegTo32(i.src3.reg());
    if (i.src3.is_constant)
      e.MOVI2R(val, static_cast<uint32_t>(i.src3.constant()) & 0xFF);
    e.STRB(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_OFFSET_I16
    : Sequence<STORE_OFFSET_I16,
               I<OPCODE_STORE_OFFSET, VoidOp, I64Op, I64Op,I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    ARM64Reg val = i.src3.is_constant ? ARM64Emitter::ScratchReg(1)
                                      : EncodeRegTo32(i.src3.reg());
    if (i.src3.is_constant)
      e.MOVI2R(val, static_cast<uint32_t>(i.src3.constant()) & 0xFFFF);
    if (LSByteSwap(i.instr)) e.REV16(val, val);
    e.STRH(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_OFFSET_I32
    : Sequence<STORE_OFFSET_I32,
               I<OPCODE_STORE_OFFSET, VoidOp, I64Op, I64Op,I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    // Use the 32-bit (W) scratch form for a constant source — otherwise REV/STR
    // run on the 64-bit X register and emit a 64-bit byte-swap + 64-bit store
    // that writes 8 bytes, clobbering the adjacent word.
    ARM64Reg val = i.src3.is_constant ? EncodeRegTo32(ARM64Emitter::ScratchReg(1))
                                      : EncodeRegTo32(i.src3.reg());
    if (i.src3.is_constant)
      e.MOVI2R(val, static_cast<uint32_t>(i.src3.constant()));
    if (LSByteSwap(i.instr)) e.REV(val, val);
    e.STR(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_OFFSET_I64
    : Sequence<STORE_OFFSET_I64,
               I<OPCODE_STORE_OFFSET, VoidOp, I64Op, I64Op,I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    ARM64Reg val = i.src3.is_constant ? ARM64Emitter::ScratchReg(1)
                                      : i.src3.reg();
    if (i.src3.is_constant)
      e.MOVI2R(val, static_cast<uint64_t>(i.src3.constant()));
    if (LSByteSwap(i.instr)) e.REV(val, val);
    e.STR(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_OFFSET_V128
    : Sequence<STORE_OFFSET_V128,
               I<OPCODE_STORE_OFFSET, VoidOp, I64Op, I64Op,V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Materialize a constant value FIRST: LoadConstantV128 uses the GPR scratch
    // regs to build the constant, which would clobber the EA (also in the GPR
    // scratch) if computed first.
    ARM64Reg val = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                      : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantV128(val, i.src3.constant());
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    ComputeEA(e, i.src1, ea);
    // Fold the (possibly large) guest offset into the address register. The
    // AArch64 load/store scaled-immediate only covers a small range, so a big
    // offset like 0x5FAC must not be passed as the instruction immediate.
    if (i.src2.constant() != 0)
      e.ADDI2R(ea, ea, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(2));
    if (LSByteSwap(i.instr)) {
      // Byte-swap before store: swap 64-bit lanes then REV64 each lane.
      ARM64Reg tmp = ARM64Emitter::ScratchVec(1);
      e.EXT(tmp, val, val, 8);
      e.REV64(tmp, tmp);
      e.STR(INDEX_UNSIGNED, tmp, ea, 0);
    } else {
      e.STR(INDEX_UNSIGNED, val, ea, 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_STORE_OFFSET, STORE_OFFSET_I8, STORE_OFFSET_I16,
                     STORE_OFFSET_I32, STORE_OFFSET_I64, STORE_OFFSET_V128);

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
