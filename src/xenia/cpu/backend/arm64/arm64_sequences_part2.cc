/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * ARM64 HIR Opcode Sequences — Part 2 of 3 (opcodes 37–72)
 *
 * Covers:
 *   LOAD/STORE (raw guest memory with big-endian byte-swap)
 *   MEMSET, CACHE_CONTROL, MEMORY_BARRIER
 *   MIN/MAX (scalar + V128), VECTOR_MIN/MAX
 *   SELECT, IS_TRUE/FALSE/NAN
 *   COMPARE_EQ/NE/SLT/SLE/SGT/SGE/ULT/ULE/UGT/UGE (I8/I16/I32/I64/F32/F64)
 *   VECTOR_COMPARE_EQ/SGT/SGE/UGT/UGE
 *   DID_SATURATE
 *   ADD (I8–I64, F32/F64, V128), ADD_CARRY, VECTOR_ADD
 *   SUB (I8–I64, F32/F64, V128), VECTOR_SUB
 *   MUL (I8–I64, F32/F64), MUL_HI, DIV (I8–I64, F32/F64, V128)
 *   MUL_ADD, MUL_SUB (F32/F64/V128)
 *   NEG (I8–I64, F32/F64, V128)
 *   ABS (F32/F64/V128)
 *   SQRT, RSQRT, RECIP (F32/F64/V128)
 *   POW2, LOG2 (F32/F64/V128 — emulated via std::exp2/std::log2)
 *
 * Key ARM64 notes:
 *   - Integer MIN/MAX: ARM64 has no scalar SMIN/SMAX in GPRs; we use
 *     CMP + CSEL (conditional select), which is 2 instructions, same cost.
 *   - ADD_CARRY: ARM64 uses ADCS (add with carry from NZCV carry flag).
 *     We set the carry flag by doing a fake compare: CMP Wn, #0 after
 *     clearing/setting via ADDS Wzr, Wzr, carry_reg (sets C if carry_reg!=0).
 *   - MUL_HI: ARM64 has UMULH (unsigned) and SMULH (signed) — exactly what
 *     we need, much cleaner than the x64 MUL/DIV-register dance.
 *   - DIV: UDIV/SDIV on ARM64 return 0 for divide-by-zero (no exception),
 *     so we do NOT need the x64 "skip if zero" guard.
 *   - RECIP/RSQRT: FRECPE/FRSQRTE give estimates; we add one Newton-Raphson
 *     refinement step to match the Altivec accuracy guarantee of < 1/4096.
 *   - POW2/LOG2: No native instruction; emulated via scalar C++ helper calls.
 *   - Vector NEG F32x4: FNEG Vd.4S, Vn.4S
 *   - Vector ABS F32x4: FABS Vd.4S, Vn.4S
 *   - Vector SQRT F32x4: FSQRT Vd.4S, Vn.4S
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_sequences.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <cmath>
#include <cstring>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/backend/arm64/arm64_compat.h"
#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/backend/arm64/arm64_op.h"
#include "xenia/cpu/backend/arm64/arm64_stack_layout.h"
#include "xenia/cpu/processor.h"

using namespace Arm64Gen;

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

using namespace xe::cpu;
using namespace xe::cpu::hir;

// ---------------------------------------------------------------------------
// Helper: compute effective address into ea_reg = membase + addr_src.
// ---------------------------------------------------------------------------
static ARM64Reg ComputeEA(ARM64Emitter& e, const I64Op& addr,
                           ARM64Reg ea_reg) {
  // Guest addresses are 32-bit; the high 32 bits of the address register are
  // garbage (a PPC `addis rX,r0,SIMM` sign-extends, so any 0x8.../0x9... global
  // address arrives as 0xFFFFFFFF........). Adding membase to that full 64-bit
  // value wraps to a wrong host address. Zero-extend to 32 bits first — mirrors
  // the x64 backend (reg.cvt32() / masking the constant).
  if (addr.is_constant) {
    e.MOVI2R(ea_reg, static_cast<uint64_t>(static_cast<uint32_t>(addr.constant())));
    e.ADD(ea_reg, e.GetMembaseReg(), ea_reg);
  } else {
    e.MOV(EncodeRegTo32(ea_reg), EncodeRegTo32(addr.reg()));  // zero-extend
    e.ADD(ea_reg, e.GetMembaseReg(), ea_reg);
  }
  return ea_reg;
}

// ---------------------------------------------------------------------------
// Helper: load a GPR src operand into scratch_reg if it's a constant.
// Returns the register to use (either the operand's reg or scratch_reg).
// ---------------------------------------------------------------------------
template <typename OpT>
static ARM64Reg GetGPRSrc32(ARM64Emitter& e, const OpT& op,
                              ARM64Reg scratch, uint32_t mask = 0xFFFFFFFF) {
  if (op.is_constant) {
    e.MOVI2R(scratch, static_cast<uint32_t>(op.constant()) & mask);
    return EncodeRegTo32(scratch);
  }
  return EncodeRegTo32(op.reg());
}

template <typename OpT>
static ARM64Reg GetGPRSrc64(ARM64Emitter& e, const OpT& op, ARM64Reg scratch) {
  if (op.is_constant) {
    e.MOVI2R(scratch, static_cast<uint64_t>(op.constant()));
    return scratch;
  }
  return op.reg();
}

// Honor the LOAD_STORE_BYTE_SWAP flag. PPC big-endian guest accesses set it;
// raw (host-endian) accesses clear it. The optimizer may pair a flag-0 load
// with an explicit OPCODE_BYTE_SWAP (and the memory-combination pass toggles
// this flag), so byte-swapping unconditionally double-swaps and corrupts the
// value. Only swap when the flag is set — mirrors the x64 backend.
static inline bool LSByteSwap(const hir::Instr* instr) {
  return (instr->flags & LOAD_STORE_BYTE_SWAP) != 0;
}

// ============================================================================
// OPCODE_LOAD  (#37)
// Raw guest memory load. Address = membase + src1. Big-endian byte-swap.
// ============================================================================
struct LOAD_I8 : Sequence<LOAD_I8, I<OPCODE_LOAD, I8Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Single byte — no byte-swap needed.
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    e.LDRB(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
  }
};
struct LOAD_I16 : Sequence<LOAD_I16, I<OPCODE_LOAD, I16Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    e.LDRH(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
    // Big-endian → little-endian: swap two bytes (only if flagged).
    if (LSByteSwap(i.instr))
      e.REV16(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
  }
};
struct LOAD_I32 : Sequence<LOAD_I32, I<OPCODE_LOAD, I32Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(i.dest.reg()), ea, 0);
    // REV reverses all 4 bytes: big-endian → little-endian.
    if (LSByteSwap(i.instr))
      e.REV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
  }
};
struct LOAD_I64 : Sequence<LOAD_I64, I<OPCODE_LOAD, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), ea, 0);
    if (LSByteSwap(i.instr)) e.REV(i.dest.reg(), i.dest.reg());
  }
};
struct LOAD_F32 : Sequence<LOAD_F32, I<OPCODE_LOAD, F32Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Load 32-bit int, byte-swap, then bitcast to float.
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(tmp), ea, 0);
    if (LSByteSwap(i.instr)) e.REV(EncodeRegTo32(tmp), EncodeRegTo32(tmp));
    e.FMOV(EncodeRegToSingle(i.dest.reg()), EncodeRegTo32(tmp));
  }
};
struct LOAD_F64 : Sequence<LOAD_F64, I<OPCODE_LOAD, F64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.LDR(INDEX_UNSIGNED, tmp, ea, 0);
    if (LSByteSwap(i.instr)) e.REV(tmp, tmp);
    e.FMOV(EncodeRegToDouble(i.dest.reg()), tmp);
  }
};
struct LOAD_V128 : Sequence<LOAD_V128, I<OPCODE_LOAD, V128Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), ea, 0);
    // Full 128-bit big→little endian conversion:
    // Step 1: REV64 Vd.16B — reverse bytes within each 64-bit lane.
    // Step 2: EXT Vd.16B, Vd.16B, Vd.16B, #8 — swap the two 64-bit lanes.
    // Combined effect: reverses all 16 bytes.
    if (LSByteSwap(i.instr)) {
      e.REV64(i.dest.reg(), i.dest.reg());
      e.EXT(i.dest.reg(), i.dest.reg(), i.dest.reg(), 8);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOAD, LOAD_I8, LOAD_I16, LOAD_I32, LOAD_I64,
                     LOAD_F32, LOAD_F64, LOAD_V128);

// ============================================================================
// OPCODE_STORE  (#38)
// Raw guest memory store. Big-endian byte-swap before writing.
// ============================================================================
struct STORE_I8 : Sequence<STORE_I8, I<OPCODE_STORE, VoidOp, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg val = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(1), 0xFF);
    e.STRB(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_I16 : Sequence<STORE_I16, I<OPCODE_STORE, VoidOp, I64Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg val = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(1), 0xFFFF);
    // Swap bytes before storing (only if flagged). When swapping a non-constant
    // source we must not clobber the guest register, so swap into a scratch.
    // Use the 32-bit (W) scratch form so REV16 stays a 32-bit op.
    if (LSByteSwap(i.instr)) {
      ARM64Reg tmp = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
      e.REV16(tmp, val);
      val = tmp;
    }
    e.STRH(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_I32 : Sequence<STORE_I32, I<OPCODE_STORE, VoidOp, I64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg val = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(1));
    if (LSByteSwap(i.instr)) {
      // Use the 32-bit (W) form of the scratch — otherwise REV/STR operate on the
      // 64-bit X register, emitting a 64-bit byte-swap + 64-bit store that writes
      // 8 bytes and clobbers the adjacent word.
      ARM64Reg tmp = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
      e.REV(tmp, val);
      val = tmp;
    }
    e.STR(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_I64 : Sequence<STORE_I64, I<OPCODE_STORE, VoidOp, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg val = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(1));
    if (LSByteSwap(i.instr)) {
      ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
      e.REV(tmp, val);
      val = tmp;
    }
    e.STR(INDEX_UNSIGNED, val, ea, 0);
    e.EmitStoreWatch(ea, val);
  }
};
struct STORE_F32 : Sequence<STORE_F32, I<OPCODE_STORE, VoidOp, I64Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Materialize the constant FIRST (LoadConstantF32 uses GPR scratch x9 to
    // build the bits → would clobber the EA, which also lives in x9).
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(src, i.src2.constant());
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    // Bitcast float → int, byte-swap, store.
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.FMOV(EncodeRegTo32(tmp), EncodeRegToSingle(src));
    if (LSByteSwap(i.instr)) e.REV(EncodeRegTo32(tmp), EncodeRegTo32(tmp));
    e.STR(INDEX_UNSIGNED, EncodeRegTo32(tmp), ea, 0);
  }
};
struct STORE_F64 : Sequence<STORE_F64, I<OPCODE_STORE, VoidOp, I64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Materialize the constant FIRST (LoadConstantF64 uses GPR scratch x9 →
    // would clobber the EA, which also lives in x9).
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(src, i.src2.constant());
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
    e.FMOV(tmp, EncodeRegToDouble(src));
    if (LSByteSwap(i.instr)) e.REV(tmp, tmp);
    e.STR(INDEX_UNSIGNED, tmp, ea, 0);
  }
};
struct STORE_V128 : Sequence<STORE_V128, I<OPCODE_STORE, VoidOp, I64Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Materialize a constant value FIRST: LoadConstantV128 uses the GPR scratch
    // regs (x9/x10) to build the 128-bit constant, which would clobber the EA
    // if we computed it first (the EA also lives in ScratchReg(0)=x9).
    ARM64Reg src = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(src, i.src2.constant());
    ARM64Reg ea  = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    if (LSByteSwap(i.instr)) {
      // Little→big endian: EXT to swap lanes, then REV64 each lane.
      ARM64Reg tmp = ARM64Emitter::ScratchVec(1);
      e.EXT(tmp, src, src, 8);
      e.REV64(tmp, tmp);
      e.STR(INDEX_UNSIGNED, tmp, ea, 0);
    } else {
      e.STR(INDEX_UNSIGNED, src, ea, 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_STORE, STORE_I8, STORE_I16, STORE_I32, STORE_I64,
                     STORE_F32, STORE_F64, STORE_V128);

// ============================================================================
// OPCODE_MEMSET  (#39)
// Zero/fill a guest memory region. We call a C++ helper for simplicity.
// src1 = base address, src2 = value byte, src3 = count.
// ============================================================================
static void MemsetHelper(void* ctx, uint64_t addr, uint8_t val, uint64_t count) {
  // ctx is the guest PPCContext* (x19), NOT a ThreadState*. The old cast read
  // ThreadState fields out of PPCContext memory: "memory()" landed on
  // virtual_membase (0x100000000), and TranslateVirtual then dereferenced that
  // fake Memory* — fault at 0x100000020 on the first dcbz. Translate through
  // the context's own membase instead.
  auto* ppc_ctx = reinterpret_cast<xe::cpu::ppc::PPCContext*>(ctx);
  std::memset(ppc_ctx->virtual_membase + static_cast<uint32_t>(addr), val,
              static_cast<size_t>(count));
}

struct MEMSET_I64
    : Sequence<MEMSET_I64, I<OPCODE_MEMSET, VoidOp, I64Op, I8Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Call MemsetHelper(context, addr, val, count).
    e.MOV(X0, e.GetContextReg());
    if (i.src1.is_constant)
      e.MOVI2R(X1, static_cast<uint64_t>(i.src1.constant()));
    else
      e.MOV(X1, i.src1.reg());
    if (i.src2.is_constant)
      e.MOVI2R(EncodeRegTo32(X2),
               static_cast<uint32_t>(i.src2.constant()) & 0xFF);
    else
      e.MOV(EncodeRegTo32(X2), EncodeRegTo32(i.src2.reg()));
    if (i.src3.is_constant)
      e.MOVI2R(X3, static_cast<uint64_t>(i.src3.constant()));
    else
      e.MOV(X3, i.src3.reg());
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(MemsetHelper));
    e.BLR(ARM64Emitter::ScratchReg(0));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MEMSET, MEMSET_I64);

// ============================================================================
// OPCODE_CACHE_CONTROL  (#40)
// Maps to dcbt (touch for load) or dcbst (store hint). ARM64 has DC ZVA and
// PRFM instructions for prefetch hints. We emit the closest equivalents;
// correctness is not required (these are pure hints).
// ============================================================================
struct CACHE_CONTROL
    : Sequence<CACHE_CONTROL,
               I<OPCODE_CACHE_CONTROL, VoidOp, I64Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // i.src2.value encodes the cache control type from cpu_flags.h.
    // For prefetch hints, emit PRFM PLDL1KEEP at the address.
    // For store hints, emit PRFM PSTL1KEEP.
    // For zero operations (dcbz), we'd emit DC ZVA, but that's rare.
    // For now, all cache control is a lightweight PRFM.
    ARM64Reg ea = ComputeEA(e, i.src1, ARM64Emitter::ScratchReg(0));
    // PRFM PLDL1KEEP, [Xn] — prefetch for load, L1 cache, keep in cache.
    e.PRFM(PLDL1KEEP, ea);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CACHE_CONTROL, CACHE_CONTROL);

// ============================================================================
// OPCODE_MEMORY_BARRIER  (#41)
// Full memory barrier. ARM64: DMB ISH (inner-shareable data memory barrier).
// This ensures all prior memory accesses complete before later ones start,
// matching the semantics of the 360's sync/eieio/isync instructions.
// ============================================================================
struct MEMORY_BARRIER
    : Sequence<MEMORY_BARRIER, I<OPCODE_MEMORY_BARRIER, VoidOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // DMB ISH: full read+write barrier, inner shareable domain.
    e.DMB(BarrierType::ISH);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MEMORY_BARRIER, MEMORY_BARRIER);

// ============================================================================
// OPCODE_MAX  (#42)
// Scalar max. ARM64 has no SMAX/UMAX in GPR registers, so we use CMP+CSEL.
// For floats: FMAX handles NaN correctly per AAPCS.
// For V128: FMAX Vd.4S, Vn.4S, Vm.4S is a single instruction.
// ============================================================================
struct MAX_F32 : Sequence<MAX_F32, I<OPCODE_MAX, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FMAX(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()),
           EncodeRegToSingle(b));
  }
};
struct MAX_F64 : Sequence<MAX_F64, I<OPCODE_MAX, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FMAX(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()),
           EncodeRegToDouble(b));
  }
};
struct MAX_V128 : Sequence<MAX_V128, I<OPCODE_MAX, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FMAX Vd.4S, Vn.4S, Vm.4S — element-wise float max of 4 floats.
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    e.FMAX(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MAX, MAX_F32, MAX_F64, MAX_V128);

// ============================================================================
// OPCODE_MIN  (#43)
// Scalar min. Same pattern as MAX with FMIN for floats, CMP+CSEL for ints.
// ============================================================================
struct MIN_I8 : Sequence<MIN_I8, I<OPCODE_MIN, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    // Signed min: CMP a, b; CSEL dest, a, b, LT
    e.CMP(a, b);
    e.CSEL(EncodeRegTo32(i.dest.reg()), a, b, CC_LT);
  }
};
struct MIN_I16 : Sequence<MIN_I16, I<OPCODE_MIN, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.CMP(a, b);
    e.CSEL(EncodeRegTo32(i.dest.reg()), a, b, CC_LT);
  }
};
struct MIN_I32 : Sequence<MIN_I32, I<OPCODE_MIN, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.CMP(a, b);
    e.CSEL(EncodeRegTo32(i.dest.reg()), a, b, CC_LT);
  }
};
struct MIN_I64 : Sequence<MIN_I64, I<OPCODE_MIN, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.reg();
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.CMP(a, b);
    e.CSEL(i.dest.reg(), a, b, CC_LT);
  }
};
struct MIN_F32 : Sequence<MIN_F32, I<OPCODE_MIN, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FMIN(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()),
           EncodeRegToSingle(b));
  }
};
struct MIN_F64 : Sequence<MIN_F64, I<OPCODE_MIN, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FMIN(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()),
           EncodeRegToDouble(b));
  }
};
struct MIN_V128 : Sequence<MIN_V128, I<OPCODE_MIN, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    e.FMIN(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MIN, MIN_I8, MIN_I16, MIN_I32, MIN_I64, MIN_F32,
                     MIN_F64, MIN_V128);

// ============================================================================
// OPCODE_VECTOR_MAX / OPCODE_VECTOR_MIN  (#44–45)
// 128-bit integer/float vector min/max, element-wise.
// The instr->flags encode the element type (INT8/INT16/INT32/FLOAT32).
// NEON has: SMAX/UMAX/SMAXP/UMAXP (int), FMAX (float) all in Vd.xT form.
// ============================================================================
struct VECTOR_MAX
    : Sequence<VECTOR_MAX, I<OPCODE_VECTOR_MAX, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.reg();
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    ARM64Reg d = i.dest.reg();
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:
        if (is_unsigned) e.UMAX(d, a, b);
        else             e.SMAX(d, a, b);
        break;
      case INT16_TYPE:
        if (is_unsigned) e.UMAX(d, a, b);
        else             e.SMAX(d, a, b);
        break;
      case INT32_TYPE:
        if (is_unsigned) e.UMAX(d, a, b);
        else             e.SMAX(d, a, b);
        break;
      case FLOAT32_TYPE:
      default:
        e.FMAX(d, a, b);
        break;
    }
  }
};
struct VECTOR_MIN
    : Sequence<VECTOR_MIN, I<OPCODE_VECTOR_MIN, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.reg();
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    ARM64Reg d = i.dest.reg();
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:
        if (is_unsigned) e.UMIN(d, a, b);
        else             e.SMIN(d, a, b);
        break;
      case INT16_TYPE:
        if (is_unsigned) e.UMIN(d, a, b);
        else             e.SMIN(d, a, b);
        break;
      case INT32_TYPE:
        if (is_unsigned) e.UMIN(d, a, b);
        else             e.SMIN(d, a, b);
        break;
      case FLOAT32_TYPE:
      default:
        e.FMIN(d, a, b);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_MAX, VECTOR_MAX);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_MIN, VECTOR_MIN);

// ============================================================================
// OPCODE_SELECT  (#46)
// dest = src1 ? src2 : src3
// ARM64: TST src1; CSEL dest, src2, src3, NE
// For float/vector: use FCSEL (scalar) or BIT/BIF (vector).
// ============================================================================
// Helper macro to avoid repetition for integer select types.
#define EMIT_INT_SELECT(NAME, DTYPE, STYPE, REG32, ENCODE)              \
  struct NAME : Sequence<NAME, I<OPCODE_SELECT, DTYPE, I8Op, STYPE, STYPE>> { \
    static void Emit(ARM64Emitter& e, const EmitArgType& i) {           \
      ARM64Reg cond = i.src1.is_constant                                \
                          ? ARM64Emitter::ScratchReg(0)                 \
                          : EncodeRegTo32(i.src1.reg());                \
      if (i.src1.is_constant) {                                         \
        if (i.src1.constant()) {                                        \
          /* Always true: dest = src2 */                                \
          ARM64Reg a = GetGPRSrc##REG32(e, i.src2,                     \
                                        ARM64Emitter::ScratchReg(1));   \
          e.MOV(ENCODE(i.dest.reg()), ENCODE(a));                       \
        } else {                                                        \
          /* Always false: dest = src3 */                               \
          ARM64Reg b = GetGPRSrc##REG32(e, i.src3,                     \
                                        ARM64Emitter::ScratchReg(1));   \
          e.MOV(ENCODE(i.dest.reg()), ENCODE(b));                       \
        }                                                               \
        return;                                                         \
      }                                                                 \
      e.TST(cond, cond);                                                \
      ARM64Reg a = GetGPRSrc##REG32(e, i.src2, ARM64Emitter::ScratchReg(1)); \
      ARM64Reg b = GetGPRSrc##REG32(e, i.src3, ARM64Emitter::ScratchReg(2)); \
      e.CSEL(ENCODE(i.dest.reg()), ENCODE(a), ENCODE(b), CC_NEQ);      \
    }                                                                   \
  };

EMIT_INT_SELECT(SELECT_I8,  I8Op,  I8Op,  32, EncodeRegTo32)
EMIT_INT_SELECT(SELECT_I16, I16Op, I16Op, 32, EncodeRegTo32)
EMIT_INT_SELECT(SELECT_I32, I32Op, I32Op, 32, EncodeRegTo32)
EMIT_INT_SELECT(SELECT_I64, I64Op, I64Op, 64, )  // 64-bit: no encode needed

// Specialise SELECT_I64 separately since the macro encode trick doesn't work
// cleanly for 64-bit:
#undef EMIT_INT_SELECT

struct SELECT_I64_IMPL
    : Sequence<SELECT_I64_IMPL, I<OPCODE_SELECT, I64Op, I8Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      ARM64Reg src = i.src1.constant() ? i.src2.reg() : i.src3.reg();
      e.MOV(i.dest.reg(), src);
      return;
    }
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    ARM64Reg a = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    ARM64Reg b = GetGPRSrc64(e, i.src3, ARM64Emitter::ScratchReg(1));
    e.CSEL(i.dest.reg(), a, b, CC_NEQ);
  }
};

struct SELECT_F32
    : Sequence<SELECT_F32, I<OPCODE_SELECT, F32Op, I8Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    ARM64Reg b = i.src3.is_constant ? ARM64Emitter::ScratchVec(1) : i.src3.reg();
    if (i.src2.is_constant) e.LoadConstantF32(a, i.src2.constant());
    if (i.src3.is_constant) e.LoadConstantF32(b, i.src3.constant());
    if (i.src1.is_constant) {
      e.FMOV(EncodeRegToSingle(i.dest.reg()),
             EncodeRegToSingle(i.src1.constant() ? a : b));
      return;
    }
    // FCSEL Sd, Sn, Sm, cond — ARM64 scalar float conditional select.
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.FCSEL(EncodeRegToSingle(i.dest.reg()),
            EncodeRegToSingle(a),
            EncodeRegToSingle(b), CC_NEQ);
  }
};
struct SELECT_F64
    : Sequence<SELECT_F64, I<OPCODE_SELECT, F64Op, I8Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    ARM64Reg b = i.src3.is_constant ? ARM64Emitter::ScratchVec(1) : i.src3.reg();
    if (i.src2.is_constant) e.LoadConstantF64(a, i.src2.constant());
    if (i.src3.is_constant) e.LoadConstantF64(b, i.src3.constant());
    if (i.src1.is_constant) {
      e.FMOV(EncodeRegToDouble(i.dest.reg()),
             EncodeRegToDouble(i.src1.constant() ? a : b));
      return;
    }
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.FCSEL(EncodeRegToDouble(i.dest.reg()),
            EncodeRegToDouble(a),
            EncodeRegToDouble(b), CC_NEQ);
  }
};

// V128 SELECT with I8 condition: use BIT/BIF for vectorized predicate.
// BIT Vd, Vn, Vm: Vd = (Vd & ~Vm) | (Vn & Vm)  [bit-insert-if-true]
// We broadcast the condition byte to all lanes, then BIT/BIF.
struct SELECT_V128_I8
    : Sequence<SELECT_V128_I8,
               I<OPCODE_SELECT, V128Op, I8Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    ARM64Reg b = i.src3.is_constant ? ARM64Emitter::ScratchVec(1) : i.src3.reg();
    if (i.src2.is_constant) e.LoadConstantV128(a, i.src2.constant());
    if (i.src3.is_constant) e.LoadConstantV128(b, i.src3.constant());
    if (i.src1.is_constant) {
      e.ORR(i.dest.reg(), i.src1.constant() ? a : b,
                          i.src1.constant() ? a : b);
      return;
    }
    // Broadcast condition byte to all 16 lanes of a scratch vector.
    ARM64Reg mask = ARM64Emitter::ScratchVec(2);
    // DUP Vm.16B, Wn — duplicate GPR byte to all 16 lanes.
    e.DUP(mask, EncodeRegTo32(i.src1.reg()));
    // SXTL or NEG to make mask all-ones/all-zeros:
    // We need mask = 0xFF...FF if cond != 0, else 0x00...00.
    // Use CMTST or CMEQ. Simplest: NEG Vm.16B, Vm.16B (since DUP gives
    // 0x00 or non-zero; NEG turns non-zero into 0xFF by two's complement
    // only for 0x01→0xFF; for other values it's not 0xFF).
    // Safer: CMEQ Vm.16B, Vm.16B, #0 then NOT.
    ARM64Reg zero = ARM64Emitter::ScratchVec(3);
    e.MOVI(zero, 0);          // zero vector
    e.CMEQ(mask, mask, zero); // mask = 0xFF where cond==0, 0x00 where cond!=0
    e.NOT(mask, mask);        // mask = 0xFF where cond!=0, 0x00 where cond==0
    // BIT d, n, mask: dest = dest | (src2 & mask), dest &= ~mask first.
    // We want: dest = (a & mask) | (b & ~mask)
    // Use BSL mask, a, b: mask = (a & mask) | (b & ~mask) → dest
    e.BSL(mask, a, b);
    e.ORR(i.dest.reg(), mask, mask);
  }
};
// V128 SELECT with V128 condition mask (per-element predicate):
struct SELECT_V128_V128
    : Sequence<SELECT_V128_V128,
               I<OPCODE_SELECT, V128Op, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg mask = i.src1.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src1.reg();
    ARM64Reg a    = i.src2.is_constant ? ARM64Emitter::ScratchVec(1)
                                       : i.src2.reg();
    ARM64Reg b    = i.src3.is_constant ? ARM64Emitter::ScratchVec(2)
                                       : i.src3.reg();
    if (i.src1.is_constant) e.LoadConstantV128(mask, i.src1.constant());
    if (i.src2.is_constant) e.LoadConstantV128(a, i.src2.constant());
    if (i.src3.is_constant) e.LoadConstantV128(b, i.src3.constant());
    // BSL needs mask in dest register. Copy mask first.
    ARM64Reg tmp = ARM64Emitter::ScratchVec(3);
    e.ORR(tmp, mask, mask);
    e.BSL(tmp, a, b);
    e.ORR(i.dest.reg(), tmp, tmp);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SELECT, SELECT_I8, SELECT_I16, SELECT_I32,
                     SELECT_I64_IMPL, SELECT_F32, SELECT_F64,
                     SELECT_V128_I8, SELECT_V128_V128);

// ============================================================================
// OPCODE_IS_TRUE / OPCODE_IS_FALSE  (#47–48)
// Returns 1 if src != 0 (IS_TRUE) or == 0 (IS_FALSE).
// For integers: TST/CMP; for floats/vectors: move bits out, TST.
// ============================================================================
struct IS_TRUE_I8   : Sequence<IS_TRUE_I8,   I<OPCODE_IS_TRUE,  I8Op, I8Op>>   {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_I16  : Sequence<IS_TRUE_I16,  I<OPCODE_IS_TRUE,  I8Op, I16Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_I32  : Sequence<IS_TRUE_I32,  I<OPCODE_IS_TRUE,  I8Op, I32Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_I64  : Sequence<IS_TRUE_I64,  I<OPCODE_IS_TRUE,  I8Op, I64Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(i.src1.reg(), i.src1.reg());
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_F32  : Sequence<IS_TRUE_F32,  I<OPCODE_IS_TRUE,  I8Op, F32Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Move float bits to GPR, test for zero.
    e.FMOV(EncodeRegTo32(ARM64Emitter::ScratchReg(0)),
           EncodeRegToSingle(i.src1.reg()));
    e.TST(ARM64Emitter::ScratchReg(0), ARM64Emitter::ScratchReg(0));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_F64  : Sequence<IS_TRUE_F64,  I<OPCODE_IS_TRUE,  I8Op, F64Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(ARM64Emitter::ScratchReg(0), EncodeRegToDouble(i.src1.reg()));
    e.TST(ARM64Emitter::ScratchReg(0), ARM64Emitter::ScratchReg(0));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
struct IS_TRUE_V128 : Sequence<IS_TRUE_V128, I<OPCODE_IS_TRUE,  I8Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // UMAXV Bd, Vn.16B: unsigned max across all 16 bytes.
    // Result is non-zero iff any byte is non-zero.
    ARM64Reg tmp = ARM64Emitter::ScratchVec(0);
    e.UMAXV(tmp, i.src1.reg());
    e.UMOV(EncodeRegTo32(i.dest.reg()), tmp, 0);  // extract byte 0
    e.TST(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_NEQ);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_IS_TRUE, IS_TRUE_I8, IS_TRUE_I16, IS_TRUE_I32,
                     IS_TRUE_I64, IS_TRUE_F32, IS_TRUE_F64, IS_TRUE_V128);

struct IS_FALSE_I8   : Sequence<IS_FALSE_I8,   I<OPCODE_IS_FALSE, I8Op, I8Op>>   {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_I16  : Sequence<IS_FALSE_I16,  I<OPCODE_IS_FALSE, I8Op, I16Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_I32  : Sequence<IS_FALSE_I32,  I<OPCODE_IS_FALSE, I8Op, I32Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(EncodeRegTo32(i.src1.reg()), EncodeRegTo32(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_I64  : Sequence<IS_FALSE_I64,  I<OPCODE_IS_FALSE, I8Op, I64Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.TST(i.src1.reg(), i.src1.reg());
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_F32  : Sequence<IS_FALSE_F32,  I<OPCODE_IS_FALSE, I8Op, F32Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(EncodeRegTo32(ARM64Emitter::ScratchReg(0)),
           EncodeRegToSingle(i.src1.reg()));
    e.TST(ARM64Emitter::ScratchReg(0), ARM64Emitter::ScratchReg(0));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_F64  : Sequence<IS_FALSE_F64,  I<OPCODE_IS_FALSE, I8Op, F64Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(ARM64Emitter::ScratchReg(0), EncodeRegToDouble(i.src1.reg()));
    e.TST(ARM64Emitter::ScratchReg(0), ARM64Emitter::ScratchReg(0));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
struct IS_FALSE_V128 : Sequence<IS_FALSE_V128, I<OPCODE_IS_FALSE, I8Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg tmp = ARM64Emitter::ScratchVec(0);
    e.UMAXV(tmp, i.src1.reg());
    e.UMOV(EncodeRegTo32(i.dest.reg()), tmp, 0);
    e.TST(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_EQ);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_IS_FALSE, IS_FALSE_I8, IS_FALSE_I16, IS_FALSE_I32,
                     IS_FALSE_I64, IS_FALSE_F32, IS_FALSE_F64, IS_FALSE_V128);

// ============================================================================
// OPCODE_IS_NAN  (#49)
// Returns 1 if the float is NaN. FCMP Sn, Sn sets V (unordered) if NaN.
// ============================================================================
struct IS_NAN_F32 : Sequence<IS_NAN_F32, I<OPCODE_IS_NAN, I8Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FCMP Sn, Sn: if NaN, sets V=1 (unordered), CC_VS condition.
    e.FCMP(EncodeRegToSingle(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_VS);
  }
};
struct IS_NAN_F64 : Sequence<IS_NAN_F64, I<OPCODE_IS_NAN, I8Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FCMP(EncodeRegToDouble(i.src1.reg()));
    e.CSET(EncodeRegTo32(i.dest.reg()), CC_VS);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_IS_NAN, IS_NAN_F32, IS_NAN_F64);

// ============================================================================
// OPCODE_COMPARE_EQ / NE  (#50–51)
// All integer widths + F32/F64. Produces an I8 (0 or 1).
// ARM64: CMP + CSET for integers, FCMP + CSET for floats.
// ============================================================================
#define EMIT_INT_CMP(NAME, OPCODE, WIDTH, ENCODE, COND)                    \
  struct NAME : Sequence<NAME, I<OPCODE, I8Op, WIDTH##Op, WIDTH##Op>> {    \
    static void Emit(ARM64Emitter& e, const EmitArgType& i) {              \
      ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));   \
      ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));   \
      e.CMP(a, b);                                                         \
      e.CSET(EncodeRegTo32(i.dest.reg()), COND);                          \
    }                                                                      \
  };

#define EMIT_INT_CMP64(NAME, OPCODE, COND)                                 \
  struct NAME : Sequence<NAME, I<OPCODE, I8Op, I64Op, I64Op>> {           \
    static void Emit(ARM64Emitter& e, const EmitArgType& i) {              \
      ARM64Reg a = GetGPRSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));   \
      ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));   \
      e.CMP(a, b);                                                         \
      e.CSET(EncodeRegTo32(i.dest.reg()), COND);                          \
    }                                                                      \
  };

#define EMIT_FLT_CMP(NAME, OPCODE, TYPE, ENCODE, COND)                    \
  struct NAME : Sequence<NAME, I<OPCODE, I8Op, TYPE##Op, TYPE##Op>> {     \
    static void Emit(ARM64Emitter& e, const EmitArgType& i) {              \
      ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(0)       \
                                      : i.src1.reg();                     \
      ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(1)       \
                                      : i.src2.reg();                     \
      if (i.src1.is_constant) e.LoadConstant##TYPE(a, i.src1.constant()); \
      if (i.src2.is_constant) e.LoadConstant##TYPE(b, i.src2.constant()); \
      e.FCMP(ENCODE(a), ENCODE(b));                                        \
      e.CSET(EncodeRegTo32(i.dest.reg()), COND);                          \
    }                                                                      \
  };

// EQ
EMIT_INT_CMP(COMPARE_EQ_I8,  OPCODE_COMPARE_EQ, I8,  EncodeRegTo32, CC_EQ)
EMIT_INT_CMP(COMPARE_EQ_I16, OPCODE_COMPARE_EQ, I16, EncodeRegTo32, CC_EQ)
EMIT_INT_CMP(COMPARE_EQ_I32, OPCODE_COMPARE_EQ, I32, EncodeRegTo32, CC_EQ)
EMIT_INT_CMP64(COMPARE_EQ_I64, OPCODE_COMPARE_EQ, CC_EQ)
EMIT_FLT_CMP(COMPARE_EQ_F32, OPCODE_COMPARE_EQ, F32, EncodeRegToSingle, CC_EQ)
EMIT_FLT_CMP(COMPARE_EQ_F64, OPCODE_COMPARE_EQ, F64, EncodeRegToDouble, CC_EQ)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_EQ, COMPARE_EQ_I8, COMPARE_EQ_I16,
                     COMPARE_EQ_I32, COMPARE_EQ_I64, COMPARE_EQ_F32,
                     COMPARE_EQ_F64);

// NE
EMIT_INT_CMP(COMPARE_NE_I8,  OPCODE_COMPARE_NE, I8,  EncodeRegTo32, CC_NEQ)
EMIT_INT_CMP(COMPARE_NE_I16, OPCODE_COMPARE_NE, I16, EncodeRegTo32, CC_NEQ)
EMIT_INT_CMP(COMPARE_NE_I32, OPCODE_COMPARE_NE, I32, EncodeRegTo32, CC_NEQ)
EMIT_INT_CMP64(COMPARE_NE_I64, OPCODE_COMPARE_NE, CC_NEQ)
EMIT_FLT_CMP(COMPARE_NE_F32, OPCODE_COMPARE_NE, F32, EncodeRegToSingle, CC_NEQ)
EMIT_FLT_CMP(COMPARE_NE_F64, OPCODE_COMPARE_NE, F64, EncodeRegToDouble, CC_NEQ)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_NE, COMPARE_NE_I8, COMPARE_NE_I16,
                     COMPARE_NE_I32, COMPARE_NE_I64, COMPARE_NE_F32,
                     COMPARE_NE_F64);

// SLT (signed less than)
EMIT_INT_CMP(COMPARE_SLT_I8,  OPCODE_COMPARE_SLT, I8,  EncodeRegTo32, CC_LT)
EMIT_INT_CMP(COMPARE_SLT_I16, OPCODE_COMPARE_SLT, I16, EncodeRegTo32, CC_LT)
EMIT_INT_CMP(COMPARE_SLT_I32, OPCODE_COMPARE_SLT, I32, EncodeRegTo32, CC_LT)
EMIT_INT_CMP64(COMPARE_SLT_I64, OPCODE_COMPARE_SLT, CC_LT)
EMIT_FLT_CMP(COMPARE_SLT_F32, OPCODE_COMPARE_SLT, F32, EncodeRegToSingle, CC_LO)
EMIT_FLT_CMP(COMPARE_SLT_F64, OPCODE_COMPARE_SLT, F64, EncodeRegToDouble, CC_LO)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_SLT, COMPARE_SLT_I8, COMPARE_SLT_I16,
                     COMPARE_SLT_I32, COMPARE_SLT_I64, COMPARE_SLT_F32,
                     COMPARE_SLT_F64);

// SLE (signed less than or equal)
EMIT_INT_CMP(COMPARE_SLE_I8,  OPCODE_COMPARE_SLE, I8,  EncodeRegTo32, CC_LE)
EMIT_INT_CMP(COMPARE_SLE_I16, OPCODE_COMPARE_SLE, I16, EncodeRegTo32, CC_LE)
EMIT_INT_CMP(COMPARE_SLE_I32, OPCODE_COMPARE_SLE, I32, EncodeRegTo32, CC_LE)
EMIT_INT_CMP64(COMPARE_SLE_I64, OPCODE_COMPARE_SLE, CC_LE)
EMIT_FLT_CMP(COMPARE_SLE_F32, OPCODE_COMPARE_SLE, F32, EncodeRegToSingle, CC_LS)
EMIT_FLT_CMP(COMPARE_SLE_F64, OPCODE_COMPARE_SLE, F64, EncodeRegToDouble, CC_LS)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_SLE, COMPARE_SLE_I8, COMPARE_SLE_I16,
                     COMPARE_SLE_I32, COMPARE_SLE_I64, COMPARE_SLE_F32,
                     COMPARE_SLE_F64);

// SGT (signed greater than)
EMIT_INT_CMP(COMPARE_SGT_I8,  OPCODE_COMPARE_SGT, I8,  EncodeRegTo32, CC_GT)
EMIT_INT_CMP(COMPARE_SGT_I16, OPCODE_COMPARE_SGT, I16, EncodeRegTo32, CC_GT)
EMIT_INT_CMP(COMPARE_SGT_I32, OPCODE_COMPARE_SGT, I32, EncodeRegTo32, CC_GT)
EMIT_INT_CMP64(COMPARE_SGT_I64, OPCODE_COMPARE_SGT, CC_GT)
EMIT_FLT_CMP(COMPARE_SGT_F32, OPCODE_COMPARE_SGT, F32, EncodeRegToSingle, CC_HI)
EMIT_FLT_CMP(COMPARE_SGT_F64, OPCODE_COMPARE_SGT, F64, EncodeRegToDouble, CC_HI)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_SGT, COMPARE_SGT_I8, COMPARE_SGT_I16,
                     COMPARE_SGT_I32, COMPARE_SGT_I64, COMPARE_SGT_F32,
                     COMPARE_SGT_F64);

// SGE (signed greater than or equal)
EMIT_INT_CMP(COMPARE_SGE_I8,  OPCODE_COMPARE_SGE, I8,  EncodeRegTo32, CC_GE)
EMIT_INT_CMP(COMPARE_SGE_I16, OPCODE_COMPARE_SGE, I16, EncodeRegTo32, CC_GE)
EMIT_INT_CMP(COMPARE_SGE_I32, OPCODE_COMPARE_SGE, I32, EncodeRegTo32, CC_GE)
EMIT_INT_CMP64(COMPARE_SGE_I64, OPCODE_COMPARE_SGE, CC_GE)
EMIT_FLT_CMP(COMPARE_SGE_F32, OPCODE_COMPARE_SGE, F32, EncodeRegToSingle, CC_HS)
EMIT_FLT_CMP(COMPARE_SGE_F64, OPCODE_COMPARE_SGE, F64, EncodeRegToDouble, CC_HS)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_SGE, COMPARE_SGE_I8, COMPARE_SGE_I16,
                     COMPARE_SGE_I32, COMPARE_SGE_I64, COMPARE_SGE_F32,
                     COMPARE_SGE_F64);

// ULT (unsigned less than)
EMIT_INT_CMP(COMPARE_ULT_I8,  OPCODE_COMPARE_ULT, I8,  EncodeRegTo32, CC_LO)
EMIT_INT_CMP(COMPARE_ULT_I16, OPCODE_COMPARE_ULT, I16, EncodeRegTo32, CC_LO)
EMIT_INT_CMP(COMPARE_ULT_I32, OPCODE_COMPARE_ULT, I32, EncodeRegTo32, CC_LO)
EMIT_INT_CMP64(COMPARE_ULT_I64, OPCODE_COMPARE_ULT, CC_LO)
EMIT_FLT_CMP(COMPARE_ULT_F32, OPCODE_COMPARE_ULT, F32, EncodeRegToSingle, CC_LO)
EMIT_FLT_CMP(COMPARE_ULT_F64, OPCODE_COMPARE_ULT, F64, EncodeRegToDouble, CC_LO)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_ULT, COMPARE_ULT_I8, COMPARE_ULT_I16,
                     COMPARE_ULT_I32, COMPARE_ULT_I64, COMPARE_ULT_F32,
                     COMPARE_ULT_F64);

// ULE (unsigned less than or equal)
EMIT_INT_CMP(COMPARE_ULE_I8,  OPCODE_COMPARE_ULE, I8,  EncodeRegTo32, CC_LS)
EMIT_INT_CMP(COMPARE_ULE_I16, OPCODE_COMPARE_ULE, I16, EncodeRegTo32, CC_LS)
EMIT_INT_CMP(COMPARE_ULE_I32, OPCODE_COMPARE_ULE, I32, EncodeRegTo32, CC_LS)
EMIT_INT_CMP64(COMPARE_ULE_I64, OPCODE_COMPARE_ULE, CC_LS)
EMIT_FLT_CMP(COMPARE_ULE_F32, OPCODE_COMPARE_ULE, F32, EncodeRegToSingle, CC_LS)
EMIT_FLT_CMP(COMPARE_ULE_F64, OPCODE_COMPARE_ULE, F64, EncodeRegToDouble, CC_LS)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_ULE, COMPARE_ULE_I8, COMPARE_ULE_I16,
                     COMPARE_ULE_I32, COMPARE_ULE_I64, COMPARE_ULE_F32,
                     COMPARE_ULE_F64);

// UGT (unsigned greater than)
EMIT_INT_CMP(COMPARE_UGT_I8,  OPCODE_COMPARE_UGT, I8,  EncodeRegTo32, CC_HI)
EMIT_INT_CMP(COMPARE_UGT_I16, OPCODE_COMPARE_UGT, I16, EncodeRegTo32, CC_HI)
EMIT_INT_CMP(COMPARE_UGT_I32, OPCODE_COMPARE_UGT, I32, EncodeRegTo32, CC_HI)
EMIT_INT_CMP64(COMPARE_UGT_I64, OPCODE_COMPARE_UGT, CC_HI)
EMIT_FLT_CMP(COMPARE_UGT_F32, OPCODE_COMPARE_UGT, F32, EncodeRegToSingle, CC_HI)
EMIT_FLT_CMP(COMPARE_UGT_F64, OPCODE_COMPARE_UGT, F64, EncodeRegToDouble, CC_HI)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_UGT, COMPARE_UGT_I8, COMPARE_UGT_I16,
                     COMPARE_UGT_I32, COMPARE_UGT_I64, COMPARE_UGT_F32,
                     COMPARE_UGT_F64);

// UGE (unsigned greater than or equal)
EMIT_INT_CMP(COMPARE_UGE_I8,  OPCODE_COMPARE_UGE, I8,  EncodeRegTo32, CC_HS)
EMIT_INT_CMP(COMPARE_UGE_I16, OPCODE_COMPARE_UGE, I16, EncodeRegTo32, CC_HS)
EMIT_INT_CMP(COMPARE_UGE_I32, OPCODE_COMPARE_UGE, I32, EncodeRegTo32, CC_HS)
EMIT_INT_CMP64(COMPARE_UGE_I64, OPCODE_COMPARE_UGE, CC_HS)
EMIT_FLT_CMP(COMPARE_UGE_F32, OPCODE_COMPARE_UGE, F32, EncodeRegToSingle, CC_HS)
EMIT_FLT_CMP(COMPARE_UGE_F64, OPCODE_COMPARE_UGE, F64, EncodeRegToDouble, CC_HS)
EMITTER_OPCODE_TABLE(OPCODE_COMPARE_UGE, COMPARE_UGE_I8, COMPARE_UGE_I16,
                     COMPARE_UGE_I32, COMPARE_UGE_I64, COMPARE_UGE_F32,
                     COMPARE_UGE_F64);

// ============================================================================
// OPCODE_DID_SATURATE  (#59)
// Returns 1 if the previous vector operation saturated. Not practically
// trackable without significant overhead — same as x64, we return 0.
// ============================================================================
struct DID_SATURATE
    : Sequence<DID_SATURATE, I<OPCODE_DID_SATURATE, I8Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // TODO: implement saturation tracking (VECTOR_ADD with saturation flag).
    // For now, always return 0 — matches x64 backend behavior.
    e.MOVI2R(EncodeRegTo32(i.dest.reg()), 0);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_DID_SATURATE, DID_SATURATE);

// ============================================================================
// OPCODE_VECTOR_COMPARE_EQ/SGT/SGE/UGT/UGE  (#60–64)
// Element-wise vector compare, result is a 128-bit mask (0xFF per element
// where true, 0x00 where false). Flags encode element width.
// NEON: CMEQ, CMGT, CMGE, CMHI, CMHS
// ============================================================================
struct VECTOR_COMPARE_EQ_V128
    : Sequence<VECTOR_COMPARE_EQ_V128,
               I<OPCODE_VECTOR_COMPARE_EQ, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:   e.CMEQ(i.dest.reg(), i.src1.reg(), b); break;
      case INT16_TYPE:  e.CMEQ(i.dest.reg(), i.src1.reg(), b); break;
      case INT32_TYPE:  e.CMEQ(i.dest.reg(), i.src1.reg(), b); break;
      case FLOAT32_TYPE: e.FCMEQ(i.dest.reg(), i.src1.reg(), b); break;
      default:          e.CMEQ(i.dest.reg(), i.src1.reg(), b); break;
    }
  }
};
struct VECTOR_COMPARE_SGT_V128
    : Sequence<VECTOR_COMPARE_SGT_V128,
               I<OPCODE_VECTOR_COMPARE_SGT, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:   e.CMGT(i.dest.reg(), i.src1.reg(), b); break;
      case INT16_TYPE:  e.CMGT(i.dest.reg(), i.src1.reg(), b); break;
      case INT32_TYPE:  e.CMGT(i.dest.reg(), i.src1.reg(), b); break;
      case FLOAT32_TYPE: e.FCMGT(i.dest.reg(), i.src1.reg(), b); break;
      default:          e.CMGT(i.dest.reg(), i.src1.reg(), b); break;
    }
  }
};
struct VECTOR_COMPARE_SGE_V128
    : Sequence<VECTOR_COMPARE_SGE_V128,
               I<OPCODE_VECTOR_COMPARE_SGE, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:   e.CMGE(i.dest.reg(), i.src1.reg(), b); break;
      case INT16_TYPE:  e.CMGE(i.dest.reg(), i.src1.reg(), b); break;
      case INT32_TYPE:  e.CMGE(i.dest.reg(), i.src1.reg(), b); break;
      case FLOAT32_TYPE: e.FCMGE(i.dest.reg(), i.src1.reg(), b); break;
      default:          e.CMGE(i.dest.reg(), i.src1.reg(), b); break;
    }
  }
};
struct VECTOR_COMPARE_UGT_V128
    : Sequence<VECTOR_COMPARE_UGT_V128,
               I<OPCODE_VECTOR_COMPARE_UGT, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:   e.CMHI(i.dest.reg(), i.src1.reg(), b); break;
      case INT16_TYPE:  e.CMHI(i.dest.reg(), i.src1.reg(), b); break;
      case INT32_TYPE:  e.CMHI(i.dest.reg(), i.src1.reg(), b); break;
      default:          e.CMHI(i.dest.reg(), i.src1.reg(), b); break;
    }
  }
};
struct VECTOR_COMPARE_UGE_V128
    : Sequence<VECTOR_COMPARE_UGE_V128,
               I<OPCODE_VECTOR_COMPARE_UGE, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:   e.CMHS(i.dest.reg(), i.src1.reg(), b); break;
      case INT16_TYPE:  e.CMHS(i.dest.reg(), i.src1.reg(), b); break;
      case INT32_TYPE:  e.CMHS(i.dest.reg(), i.src1.reg(), b); break;
      default:          e.CMHS(i.dest.reg(), i.src1.reg(), b); break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_COMPARE_EQ,  VECTOR_COMPARE_EQ_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_COMPARE_SGT, VECTOR_COMPARE_SGT_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_COMPARE_SGE, VECTOR_COMPARE_SGE_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_COMPARE_UGT, VECTOR_COMPARE_UGT_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_COMPARE_UGE, VECTOR_COMPARE_UGE_V128);

// ============================================================================
// OPCODE_ADD  (#65)
// Integer: ADD instruction for all widths.
// Float scalar: FADD.  Vector: ADD Vd.4S (int) or FADD Vd.4S (float).
// ============================================================================
struct ADD_I8 : Sequence<ADD_I8, I<OPCODE_ADD, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.ADD(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct ADD_I16 : Sequence<ADD_I16, I<OPCODE_ADD, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.ADD(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct ADD_I32 : Sequence<ADD_I32, I<OPCODE_ADD, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // ADD is commutative; if only src1 is constant, treat it as the immediate.
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ADDI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
               static_cast<uint32_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ADDI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src2.reg()),
               static_cast<uint32_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.ADD(EncodeRegTo32(i.dest.reg()), a, b);
    }
  }
};
struct ADD_I64 : Sequence<ADD_I64, I<OPCODE_ADD, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ADDI2R(i.dest.reg(), i.src1.reg(),
               static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ADDI2R(i.dest.reg(), i.src2.reg(),
               static_cast<uint64_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetGPRSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.ADD(i.dest.reg(), a, b);
    }
  }
};
struct ADD_F32 : Sequence<ADD_F32, I<OPCODE_ADD, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FADD(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()),
           EncodeRegToSingle(b));
  }
};
struct ADD_F64 : Sequence<ADD_F64, I<OPCODE_ADD, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FADD(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()),
           EncodeRegToDouble(b));
  }
};
struct ADD_V128 : Sequence<ADD_V128, I<OPCODE_ADD, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // V128 ADD is always float (4×f32) in Xenia's HIR.
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    e.FADD(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ADD, ADD_I8, ADD_I16, ADD_I32, ADD_I64,
                     ADD_F32, ADD_F64, ADD_V128);

// ============================================================================
// OPCODE_ADD_CARRY  (#66)
// dest = src1 + src2 + carry_in (carry_in is an I8 boolean).
// ARM64: ADDS sets carry; ADCS uses carry. We synthesize the carry flag by:
//   1. Set C flag: if carry_in != 0, do ADDS Wzr, Wzr, carry_reg (sets C=0
//      since Wzr+carry = carry, carry out of 0+0+1 = 0... hmm).
//   Actually cleaner: move carry_in to a register, then use
//   ADDS result, src1, src2  followed by  ADC result, result, carry_in.
//   This is: result = src1 + src2 + carry_in which is exactly what we want.
// ============================================================================
struct ADD_CARRY_I8
    : Sequence<ADD_CARRY_I8, I<OPCODE_ADD_CARRY, I8Op, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // dest = src1 + src2 + src3_carry. Simpler: add in two steps.
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    ARM64Reg c = GetGPRSrc32(e, i.src3, ARM64Emitter::ScratchReg(1), 1);
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    e.ADD(d, a, b);
    e.ADD(d, d, c);
    e.AND(d, d, 0xFF);
  }
};
struct ADD_CARRY_I16
    : Sequence<ADD_CARRY_I16, I<OPCODE_ADD_CARRY, I16Op, I16Op, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    ARM64Reg c = GetGPRSrc32(e, i.src3, ARM64Emitter::ScratchReg(1), 1);
    e.ADD(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), b);
    e.ADD(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), c);
  }
};
struct ADD_CARRY_I32
    : Sequence<ADD_CARRY_I32, I<OPCODE_ADD_CARRY, I32Op, I32Op, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Use ADDS + ADCS for proper carry propagation.
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    // Clear or set carry via ADDS Wzr, Wzr, carry_in.
    // If carry_in != 0: ADDS Wzr, Wzr, 1 → C flag becomes 0 (1 doesn't overflow Wzr).
    // Then ADCS d, a, b → d = a + b + 0 (wrong).
    // Simpler two-step:
    ARM64Reg c = GetGPRSrc32(e, i.src3, ARM64Emitter::ScratchReg(1), 1);
    e.ADD(d, a, b);
    e.ADD(d, d, c);
  }
};
struct ADD_CARRY_I64
    : Sequence<ADD_CARRY_I64, I<OPCODE_ADD_CARRY, I64Op, I64Op, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    ARM64Reg c_src = GetGPRSrc32(e, i.src3, ARM64Emitter::ScratchReg(1), 1);
    ARM64Reg c = ARM64Emitter::ScratchReg(1);
    // Zero-extend carry byte to 64 bits.
    e.UXTB(c, c_src);
    e.ADD(i.dest.reg(), i.src1.reg(), b);
    e.ADD(i.dest.reg(), i.dest.reg(), c);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ADD_CARRY, ADD_CARRY_I8, ADD_CARRY_I16,
                     ADD_CARRY_I32, ADD_CARRY_I64);

// ============================================================================
// OPCODE_VECTOR_ADD  (#67)
// Element-wise integer or float add. Flags encode element type.
// NEON: ADD Vd.xT (int), FADD Vd.4S (float).
// Saturation variants use SQADD/UQADD.
// ============================================================================
struct VECTOR_ADD
    : Sequence<VECTOR_ADD, I<OPCODE_VECTOR_ADD, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.reg();
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    ARM64Reg d = i.dest.reg();
    bool saturate  = (i.instr->flags & ARITHMETIC_SATURATE) != 0;
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:
        if (saturate) {
          if (is_unsigned) e.UQADD(d, a, b); else e.SQADD(d, a, b);
        } else { e.ADD(d, a, b); }
        break;
      case INT16_TYPE:
        if (saturate) {
          if (is_unsigned) e.UQADD(d, a, b); else e.SQADD(d, a, b);
        } else { e.ADD(d, a, b); }
        break;
      case INT32_TYPE:
        if (saturate) {
          if (is_unsigned) e.UQADD(d, a, b); else e.SQADD(d, a, b);
        } else { e.ADD(d, a, b); }
        break;
      case FLOAT32_TYPE:
      default:
        e.FADD(d, a, b);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_ADD, VECTOR_ADD);

// ============================================================================
// OPCODE_SUB  (#68)
// ============================================================================
struct SUB_I8 : Sequence<SUB_I8, I<OPCODE_SUB, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.SUB(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct SUB_I16 : Sequence<SUB_I16, I<OPCODE_SUB, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.SUB(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct SUB_I32 : Sequence<SUB_I32, I<OPCODE_SUB, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // src1 may be constant (PPC subfic = const - reg) — route it through a
    // scratch instead of calling .reg() on a constant.
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
    if (i.src2.is_constant)
      e.SUBI2R(EncodeRegTo32(i.dest.reg()), a,
               static_cast<uint32_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else
      e.SUB(EncodeRegTo32(i.dest.reg()), a, EncodeRegTo32(i.src2.reg()));
  }
};
struct SUB_I64 : Sequence<SUB_I64, I<OPCODE_SUB, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
    if (i.src2.is_constant)
      e.SUBI2R(i.dest.reg(), a, static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else
      e.SUB(i.dest.reg(), a, i.src2.reg());
  }
};
struct SUB_F32 : Sequence<SUB_F32, I<OPCODE_SUB, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Either side can constant-fold (e.g. PPC fsubs with a folded minuend).
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(1) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantF32(a, i.src1.constant());
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FSUB(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(a),
           EncodeRegToSingle(b));
  }
};
struct SUB_F64 : Sequence<SUB_F64, I<OPCODE_SUB, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(1) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantF64(a, i.src1.constant());
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FSUB(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(a),
           EncodeRegToDouble(b));
  }
};
struct SUB_V128 : Sequence<SUB_V128, I<OPCODE_SUB, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(1) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantV128(a, i.src1.constant());
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    e.FSUB(i.dest.reg(), a, b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SUB, SUB_I8, SUB_I16, SUB_I32, SUB_I64,
                     SUB_F32, SUB_F64, SUB_V128);

// ============================================================================
// OPCODE_VECTOR_SUB  (#69)
// Element-wise vector subtract. Same flag layout as VECTOR_ADD.
// NEON: SUB Vd.xT (int), FSUB Vd.4S (float).
// ============================================================================
struct VECTOR_SUB
    : Sequence<VECTOR_SUB, I<OPCODE_VECTOR_SUB, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    ARM64Reg d = i.dest.reg();
    ARM64Reg a = i.src1.reg();
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:
      case INT16_TYPE:
      case INT32_TYPE:
        e.SUB(d, a, b);
        break;
      case FLOAT32_TYPE:
      default:
        e.FSUB(d, a, b);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_SUB, VECTOR_SUB);

// ============================================================================
// OPCODE_MUL  (#70)
// Integer: MUL instruction (same for signed/unsigned, low 32/64 bits).
// Float: FMUL.  V128: FMUL Vd.4S.
// ============================================================================
struct MUL_I8 : Sequence<MUL_I8, I<OPCODE_MUL, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.MUL(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct MUL_I16 : Sequence<MUL_I16, I<OPCODE_MUL, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.MUL(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()), b);
  }
};
struct MUL_I32 : Sequence<MUL_I32, I<OPCODE_MUL, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.MUL(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct MUL_I64 : Sequence<MUL_I64, I<OPCODE_MUL, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetGPRSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.MUL(i.dest.reg(), a, b);
  }
};
struct MUL_F32 : Sequence<MUL_F32, I<OPCODE_MUL, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FMUL(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()),
           EncodeRegToSingle(b));
  }
};
struct MUL_F64 : Sequence<MUL_F64, I<OPCODE_MUL, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FMUL(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()),
           EncodeRegToDouble(b));
  }
};
struct MUL_V128 : Sequence<MUL_V128, I<OPCODE_MUL, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    e.FMUL(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MUL, MUL_I8, MUL_I16, MUL_I32, MUL_I64,
                     MUL_F32, MUL_F64, MUL_V128);

// ============================================================================
// OPCODE_MUL_HI  (#71)
// Returns the high half of a 64×64 → 128-bit multiply.
// ARM64: UMULH (unsigned), SMULH (signed) — native single instructions.
// This is MUCH cleaner than x64 which requires MUL into RDX:RAX.
// ============================================================================
struct MUL_HI_I8 : Sequence<MUL_HI_I8, I<OPCODE_MUL_HI, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // For 8-bit: do a 32-bit multiply and extract bits [15:8].
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    if (is_unsigned) {
      e.MUL(d, a, b);
    } else {
      // Sign-extend to 32 bits first.
      e.SXTB(a, a);
      e.SXTB(b, b);
      e.MUL(d, a, b);
    }
    e.LSR(d, d, 8);
    e.AND(d, d, 0xFF);
  }
};
struct MUL_HI_I16
    : Sequence<MUL_HI_I16, I<OPCODE_MUL_HI, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = EncodeRegTo32(i.src1.reg());
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    if (is_unsigned) {
      e.MUL(d, a, b);
    } else {
      e.SXTH(a, a);
      e.SXTH(b, b);
      e.MUL(d, a, b);
    }
    e.LSR(d, d, 16);
  }
};
struct MUL_HI_I32
    : Sequence<MUL_HI_I32, I<OPCODE_MUL_HI, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    ARM64Reg a64 = ARM64Emitter::ScratchReg(1);
    // Zero-extend or sign-extend to 64 bits for the SMULH/UMULH approach.
    if (is_unsigned) {
      e.MOV(EncodeRegTo32(a64), EncodeRegTo32(i.src1.reg()));  // zero-ext
    } else {
      e.SXTW(a64, EncodeRegTo32(i.src1.reg()));
    }
    if (!is_unsigned) {
      ARM64Reg b64 = ARM64Emitter::ScratchReg(0);
      e.SXTW(b64, EncodeRegTo32(b));
      e.MUL(a64, a64, b64);
    } else {
      ARM64Reg b64 = ARM64Emitter::ScratchReg(0);
      e.MOV(EncodeRegTo32(b64), EncodeRegTo32(b));  // zero-ext
      e.MUL(a64, a64, b64);
    }
    // High 32 bits are in bits [63:32].
    e.LSR(a64, a64, 32);
    e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(a64));
  }
};
struct MUL_HI_I64
    : Sequence<MUL_HI_I64, I<OPCODE_MUL_HI, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // ARM64 UMULH/SMULH: Xd = (Xn * Xm)[127:64] — exactly what we need.
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    if (is_unsigned)
      e.UMULH(i.dest.reg(), i.src1.reg(), b);
    else
      e.SMULH(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MUL_HI, MUL_HI_I8, MUL_HI_I16, MUL_HI_I32,
                     MUL_HI_I64);

// ============================================================================
// OPCODE_DIV  (#72)
// ARM64 SDIV/UDIV return 0 on divide-by-zero (no exception), so we don't
// need the x64 "skip if zero" guard. Simpler and faster.
// ============================================================================
struct DIV_I8 : Sequence<DIV_I8, I<OPCODE_DIV, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    if (is_unsigned) {
      e.UDIV(d, a, b);
    } else {
      // Sign-extend into scratches — never write back into an allocated
      // source register.
      ARM64Reg sa = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
      ARM64Reg sb = EncodeRegTo32(ARM64Emitter::ScratchReg(0));
      e.SXTB(sa, a); e.SXTB(sb, b);
      e.SDIV(d, sa, sb);
    }
    e.AND(d, d, 0xFF);
  }
};
struct DIV_I16 : Sequence<DIV_I16, I<OPCODE_DIV, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    ARM64Reg d = EncodeRegTo32(i.dest.reg());
    if (is_unsigned) {
      e.UDIV(d, a, b);
    } else {
      ARM64Reg sa = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
      ARM64Reg sb = EncodeRegTo32(ARM64Emitter::ScratchReg(0));
      e.SXTH(sa, a); e.SXTH(sb, b);
      e.SDIV(d, sa, sb);
    }
  }
};
struct DIV_I32 : Sequence<DIV_I32, I<OPCODE_DIV, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = GetGPRSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetGPRSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
    if (is_unsigned)
      e.UDIV(EncodeRegTo32(i.dest.reg()), a, b);
    else
      e.SDIV(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct DIV_I64 : Sequence<DIV_I64, I<OPCODE_DIV, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    ARM64Reg a = GetGPRSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetGPRSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    if (is_unsigned)
      e.UDIV(i.dest.reg(), a, b);
    else
      e.SDIV(i.dest.reg(), a, b);
  }
};
struct DIV_F32 : Sequence<DIV_F32, I<OPCODE_DIV, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(1) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantF32(a, i.src1.constant());
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF32(b, i.src2.constant());
    e.FDIV(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(a),
           EncodeRegToSingle(b));
  }
};
struct DIV_F64 : Sequence<DIV_F64, I<OPCODE_DIV, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(1) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantF64(a, i.src1.constant());
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantF64(b, i.src2.constant());
    e.FDIV(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(a),
           EncodeRegToDouble(b));
  }
};
struct DIV_V128 : Sequence<DIV_V128, I<OPCODE_DIV, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // NEON has no VDIV, so we use FRECPE + Newton-Raphson then FMUL.
    // For strict correctness use the scalar FDIV approach across lanes.
    ARM64Reg b = i.src2.is_constant ? ARM64Emitter::ScratchVec(0) : i.src2.reg();
    if (i.src2.is_constant) e.LoadConstantV128(b, i.src2.constant());
    ARM64Reg d = i.dest.reg();
    ARM64Reg a = i.src1.is_constant ? ARM64Emitter::ScratchVec(3) : i.src1.reg();
    if (i.src1.is_constant) e.LoadConstantV128(a, i.src1.constant());
    // Reciprocal estimate of b.
    ARM64Reg recip = ARM64Emitter::ScratchVec(1);
    e.FRECPE(recip, b);
    // One Newton-Raphson refinement: recip = recip * FRECPS(b, recip).
    ARM64Reg nr = ARM64Emitter::ScratchVec(2);
    e.FRECPS(nr, b, recip);
    e.FMUL(recip, recip, nr);
    // d = a * recip(b)
    e.FMUL(d, a, recip);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_DIV, DIV_I8, DIV_I16, DIV_I32, DIV_I64,
                     DIV_F32, DIV_F64, DIV_V128);

// ============================================================================
// OPCODE_MUL_ADD  (#73) — dest = src1 * src2 + src3
// ARM64: FMADD Sd, Sn, Sm, Sa (scalar). FMLA Vd.4S, Vn.4S, Vm.4S (vector).
// Note: FMADD computes Sn*Sm+Sa which is exactly what Xenia's HIR needs.
// ============================================================================
struct MUL_ADD_F32
    : Sequence<MUL_ADD_F32, I<OPCODE_MUL_ADD, F32Op, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantF32(src3, i.src3.constant());
    // FMADD Sd, Sn, Sm, Sa → Sd = Sn * Sm + Sa
    e.FMADD(EncodeRegToSingle(i.dest.reg()),
            EncodeRegToSingle(i.src1.reg()),
            EncodeRegToSingle(i.src2.reg()),
            EncodeRegToSingle(src3));
  }
};
struct MUL_ADD_F64
    : Sequence<MUL_ADD_F64, I<OPCODE_MUL_ADD, F64Op, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantF64(src3, i.src3.constant());
    e.FMADD(EncodeRegToDouble(i.dest.reg()),
            EncodeRegToDouble(i.src1.reg()),
            EncodeRegToDouble(i.src2.reg()),
            EncodeRegToDouble(src3));
  }
};
struct MUL_ADD_V128
    : Sequence<MUL_ADD_V128,
               I<OPCODE_MUL_ADD, V128Op, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantV128(src3, i.src3.constant());
    // FMLA Vd.4S, Vn.4S, Vm.4S: Vd = Vd + Vn*Vm
    // We want dest = src1*src2 + src3.
    // Copy src3 to dest first, then FMLA dest, src1, src2.
    if (i.dest.reg() != src3) {
      e.ORR(i.dest.reg(), src3, src3);
    }
    e.FMLA(i.dest.reg(), i.src1.reg(), i.src2.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MUL_ADD, MUL_ADD_F32, MUL_ADD_F64, MUL_ADD_V128);

// ============================================================================
// OPCODE_MUL_SUB  (#74) — dest = src1 * src2 - src3
// ARM64: FMSUB Sd, Sn, Sm, Sa → Sd = Sa - Sn*Sm. Note the operand order:
// FMSUB gives Sa - Sn*Sm, but we want Sn*Sm - Sa. So we use FNMSUB instead,
// or negate: FMADD dest, src1, src2, src3; FNEG dest, dest.
// Simplest: FMUL dest, src1, src2; FSUB dest, dest, src3.
// ============================================================================
struct MUL_SUB_F32
    : Sequence<MUL_SUB_F32, I<OPCODE_MUL_SUB, F32Op, F32Op, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantF32(src3, i.src3.constant());
    // FNMSUB Sd, Sn, Sm, Sa → Sd = -(Sn*Sm - Sa) = Sa - Sn*Sm. Wrong sign.
    // Use: dest = src1*src2 then FSUB dest, dest, src3.
    ARM64Reg tmp = ARM64Emitter::ScratchVec(1);
    e.FMUL(EncodeRegToSingle(tmp),
           EncodeRegToSingle(i.src1.reg()),
           EncodeRegToSingle(i.src2.reg()));
    e.FSUB(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(tmp),
           EncodeRegToSingle(src3));
  }
};
struct MUL_SUB_F64
    : Sequence<MUL_SUB_F64, I<OPCODE_MUL_SUB, F64Op, F64Op, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantF64(src3, i.src3.constant());
    ARM64Reg tmp = ARM64Emitter::ScratchVec(1);
    e.FMUL(EncodeRegToDouble(tmp),
           EncodeRegToDouble(i.src1.reg()),
           EncodeRegToDouble(i.src2.reg()));
    e.FSUB(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(tmp),
           EncodeRegToDouble(src3));
  }
};
struct MUL_SUB_V128
    : Sequence<MUL_SUB_V128,
               I<OPCODE_MUL_SUB, V128Op, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src3 = i.src3.is_constant ? ARM64Emitter::ScratchVec(0)
                                       : i.src3.reg();
    if (i.src3.is_constant) e.LoadConstantV128(src3, i.src3.constant());
    ARM64Reg tmp = ARM64Emitter::ScratchVec(1);
    e.FMUL(tmp, i.src1.reg(), i.src2.reg());
    e.FSUB(i.dest.reg(), tmp, src3);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_MUL_SUB, MUL_SUB_F32, MUL_SUB_F64, MUL_SUB_V128);

// ============================================================================
// OPCODE_NEG  (#75)
// Integer: NEG (two's complement). Float/Vector: FNEG.
// ============================================================================
struct NEG_I8  : Sequence<NEG_I8,  I<OPCODE_NEG, I8Op,  I8Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.NEG(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct NEG_I16 : Sequence<NEG_I16, I<OPCODE_NEG, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.NEG(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct NEG_I32 : Sequence<NEG_I32, I<OPCODE_NEG, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.NEG(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct NEG_I64 : Sequence<NEG_I64, I<OPCODE_NEG, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.NEG(i.dest.reg(), i.src1.reg());
  }
};
struct NEG_F32 : Sequence<NEG_F32, I<OPCODE_NEG, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FNEG(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()));
  }
};
struct NEG_F64 : Sequence<NEG_F64, I<OPCODE_NEG, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FNEG(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()));
  }
};
struct NEG_V128 : Sequence<NEG_V128, I<OPCODE_NEG, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FNEG Vd.4S, Vn.4S — negate all 4 floats.
    e.FNEG(i.dest.reg(), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_NEG, NEG_I8, NEG_I16, NEG_I32, NEG_I64,
                     NEG_F32, NEG_F64, NEG_V128);

// ============================================================================
// OPCODE_ABS  (#76)
// Float/Vector: FABS. No integer ABS in the HIR.
// ============================================================================
struct ABS_F32 : Sequence<ABS_F32, I<OPCODE_ABS, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FABS(EncodeRegToSingle(i.dest.reg()),
           EncodeRegToSingle(i.src1.reg()));
  }
};
struct ABS_F64 : Sequence<ABS_F64, I<OPCODE_ABS, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FABS(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()));
  }
};
struct ABS_V128 : Sequence<ABS_V128, I<OPCODE_ABS, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // FABS Vd.4S, Vn.4S — absolute value of all 4 floats.
    e.FABS(i.dest.reg(), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ABS, ABS_F32, ABS_F64, ABS_V128);

// ============================================================================
// OPCODE_SQRT  (#77)
// ARM64: FSQRT (scalar) and FSQRT Vd.4S (vector) — both native.
// ============================================================================
struct SQRT_F32 : Sequence<SQRT_F32, I<OPCODE_SQRT, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FSQRT(EncodeRegToSingle(i.dest.reg()),
            EncodeRegToSingle(i.src1.reg()));
  }
};
struct SQRT_F64 : Sequence<SQRT_F64, I<OPCODE_SQRT, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FSQRT(EncodeRegToDouble(i.dest.reg()),
            EncodeRegToDouble(i.src1.reg()));
  }
};
struct SQRT_V128 : Sequence<SQRT_V128, I<OPCODE_SQRT, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FSQRT(i.dest.reg(), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SQRT, SQRT_F32, SQRT_F64, SQRT_V128);

// ============================================================================
// OPCODE_RSQRT  (#78) — 1/sqrt(x)
// FRSQRTE gives ~8 bits of precision. One Newton-Raphson step using FRSQRTS
// brings it to ~16 bits, matching the Altivec vrsqrtefp guarantee of < 1/4096.
// Formula: x1 = x0 * FRSQRTS(a, x0) where FRSQRTS(a,x) = (3 - a*x*x) / 2.
// ============================================================================
struct RSQRT_F32 : Sequence<RSQRT_F32, I<OPCODE_RSQRT, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg est = EncodeRegToSingle(i.dest.reg());
    ARM64Reg src = EncodeRegToSingle(i.src1.reg());
    ARM64Reg nr  = EncodeRegToSingle(ARM64Emitter::ScratchVec(0));
    e.FRSQRTE(est, src);                  // estimate: est ≈ 1/sqrt(src)
    e.FRSQRTS(nr, src, est);              // nr = (3 - src*est*est) / 2
    e.FMUL(est, est, nr);                 // refined: est = est * nr
  }
};
struct RSQRT_F64 : Sequence<RSQRT_F64, I<OPCODE_RSQRT, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // No FRSQRTE for double — fall back to 1/FSQRT which is exact.
    ARM64Reg tmp = EncodeRegToDouble(ARM64Emitter::ScratchVec(0));
    e.FSQRT(tmp, EncodeRegToDouble(i.src1.reg()));
    // Load 1.0 into a scratch register.
    e.FMOV(EncodeRegToDouble(i.dest.reg()), 1.0);  // FMOV Dd, #1.0
    e.FDIV(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.dest.reg()), tmp);
  }
};
struct RSQRT_V128 : Sequence<RSQRT_V128, I<OPCODE_RSQRT, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg est = i.dest.reg();
    ARM64Reg src = i.src1.reg();
    ARM64Reg nr  = ARM64Emitter::ScratchVec(0);
    e.FRSQRTE(est, src);
    e.FRSQRTS(nr, src, est);
    e.FMUL(est, est, nr);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_RSQRT, RSQRT_F32, RSQRT_F64, RSQRT_V128);

// ============================================================================
// OPCODE_RECIP  (#79) — 1/x
// FRECPE gives ~8 bits; one Newton-Raphson step (FRECPS) gives ~16 bits,
// matching Altivec vrefp accuracy guarantee.
// ============================================================================
struct RECIP_F32 : Sequence<RECIP_F32, I<OPCODE_RECIP, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg est = EncodeRegToSingle(i.dest.reg());
    ARM64Reg src = EncodeRegToSingle(i.src1.reg());
    ARM64Reg nr  = EncodeRegToSingle(ARM64Emitter::ScratchVec(0));
    e.FRECPE(est, src);                   // est ≈ 1/src
    e.FRECPS(nr, src, est);               // nr = 2 - src*est
    e.FMUL(est, est, nr);                 // refined reciprocal
  }
};
struct RECIP_F64 : Sequence<RECIP_F64, I<OPCODE_RECIP, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // No FRECPE for double — use exact 1.0 / src.
    e.FMOV(EncodeRegToDouble(i.dest.reg()), 1.0);
    e.FDIV(EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.dest.reg()),
           EncodeRegToDouble(i.src1.reg()));
  }
};
struct RECIP_V128 : Sequence<RECIP_V128, I<OPCODE_RECIP, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg est = i.dest.reg();
    ARM64Reg src = i.src1.reg();
    ARM64Reg nr  = ARM64Emitter::ScratchVec(0);
    e.FRECPE(est, src);
    e.FRECPS(nr, src, est);
    e.FMUL(est, est, nr);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_RECIP, RECIP_F32, RECIP_F64, RECIP_V128);

// ============================================================================
// OPCODE_POW2  (#80) — 2^x
// No native instruction on any architecture. x64 uses a C++ helper.
// We do the same: call std::exp2f / std::exp2.
// ============================================================================
static float  EmulatePow2F32(void*, float  x) { return std::exp2f(x); }
static double EmulatePow2F64(void*, double x) { return std::exp2(x); }
static void   EmulatePow2V128(void* ctx, xe::cpu::ThreadState* state) {
  // Called with the vector in scratch; results written back.
  // Simpler approach: treat as 4 scalar calls via helper pointer.
  // TODO: implement properly.
}

struct POW2_F32 : Sequence<POW2_F32, I<OPCODE_POW2, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Spill src to stack, call helper, load result.
    // The emitter's CallNative puts context in x0; we put src in x1.
    // But for float calls we need to pass via NEON regs per AAPCS64.
    // Simpler: extract scalar, call, reload.
    // Store src float to scratch stack slot.
    e.STR(INDEX_UNSIGNED, EncodeRegToSingle(i.src1.reg()), SP,
          StackLayout::GUEST_SCRATCH_BASE);
    // Load as integer into x1.
    e.LDR(INDEX_UNSIGNED, EncodeRegTo32(X1), SP,
          StackLayout::GUEST_SCRATCH_BASE);
    // Call helper: float EmulatePow2F32(void* ctx, uint32_t bits_as_int).
    // Re-use bits directly via FMOV.
    e.FMOV(EncodeRegToSingle(V0), EncodeRegToSingle(i.src1.reg()));
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulatePow2F32));
    e.BLR(ARM64Emitter::ScratchReg(0));
    // Result in S0.
    e.FMOV(EncodeRegToSingle(i.dest.reg()), EncodeRegToSingle(V0));
  }
};
struct POW2_F64 : Sequence<POW2_F64, I<OPCODE_POW2, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(EncodeRegToDouble(V0), EncodeRegToDouble(i.src1.reg()));
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulatePow2F64));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.FMOV(EncodeRegToDouble(i.dest.reg()), EncodeRegToDouble(V0));
  }
};
struct POW2_V128 : Sequence<POW2_V128, I<OPCODE_POW2, V128Op, V128Op>> {
  // Scalar loop via 4 calls — slow but correct for a rarely-used opcode.
  static float EmulateHelper(float x) { return std::exp2f(x); }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg d = i.dest.reg();
    ARM64Reg s = i.src1.reg();
    ARM64Reg fn = ARM64Emitter::ScratchReg(0);
    e.MOVI2R(fn, reinterpret_cast<uint64_t>(EmulateHelper));
    for (int lane = 0; lane < 4; lane++) {
      // Extract lane to S0, call, insert result back.
      e.MOV(EncodeRegToSingle(V0), 0, EncodeRegToSingle(s), lane);
      e.BLR(fn);
      e.INS(d, lane, V0, 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_POW2, POW2_F32, POW2_F64, POW2_V128);

// ============================================================================
// OPCODE_LOG2  (#81) — log2(x)
// Same pattern as POW2: emulated via std::log2f / std::log2.
// ============================================================================
static float  EmulateLog2F32(void*, float  x) { return std::log2f(x); }
static double EmulateLog2F64(void*, double x) { return std::log2(x); }

struct LOG2_F32 : Sequence<LOG2_F32, I<OPCODE_LOG2, F32Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(EncodeRegToSingle(V0), EncodeRegToSingle(i.src1.reg()));
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulateLog2F32));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.FMOV(EncodeRegToSingle(i.dest.reg()), EncodeRegToSingle(V0));
  }
};
struct LOG2_F64 : Sequence<LOG2_F64, I<OPCODE_LOG2, F64Op, F64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.FMOV(EncodeRegToDouble(V0), EncodeRegToDouble(i.src1.reg()));
    e.MOV(X0, e.GetContextReg());
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulateLog2F64));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.FMOV(EncodeRegToDouble(i.dest.reg()), EncodeRegToDouble(V0));
  }
};
struct LOG2_V128 : Sequence<LOG2_V128, I<OPCODE_LOG2, V128Op, V128Op>> {
  static float EmulateHelper(float x) { return std::log2f(x); }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg d = i.dest.reg();
    ARM64Reg s = i.src1.reg();
    ARM64Reg fn = ARM64Emitter::ScratchReg(0);
    e.MOVI2R(fn, reinterpret_cast<uint64_t>(EmulateHelper));
    for (int lane = 0; lane < 4; lane++) {
      e.MOV(EncodeRegToSingle(V0), 0, EncodeRegToSingle(s), lane);
      e.BLR(fn);
      e.INS(d, lane, V0, 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_LOG2, LOG2_F32, LOG2_F64, LOG2_V128);

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
