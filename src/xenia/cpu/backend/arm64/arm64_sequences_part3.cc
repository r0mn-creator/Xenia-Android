/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * ARM64 HIR Opcode Sequences — Part 3 of 3 (opcodes 82–109)
 *
 * Covers:
 *   AND / AND_NOT / OR / XOR / NOT (I8–I64 + V128)
 *   SHL / SHR / SHA (I8–I64 + V128 scalar-shift)
 *   VECTOR_SHL / VECTOR_SHR / VECTOR_SHA (V128 element-wise shift)
 *   ROTATE_LEFT (I8–I64) / VECTOR_ROTATE_LEFT (V128)
 *   VECTOR_AVERAGE (V128 unsigned byte/word/dword)
 *   BYTE_SWAP (I16/I32/I64/V128)
 *   CNTLZ (I8/I16/I32/I64)
 *   INSERT / EXTRACT / SPLAT (V128 element manipulation)
 *   PERMUTE (I32 control + V128 byte-table control)
 *   SWIZZLE (V128 dword/float lane shuffle)
 *   PACK (9 modes: D3DCOLOR, FLOAT16_2/4, SHORT_2/4,
 *                  UINT_2101010, ULONG_4202020, 8_IN_16, 16_IN_32)
 *   UNPACK (9 modes: same as PACK in reverse)
 *   ATOMIC_EXCHANGE / ATOMIC_COMPARE_EXCHANGE
 *   SET_ROUNDING_MODE
 *
 * Key ARM64 notes for this file:
 *
 *  AND_NOT: ARM64 has BIC (bit clear) = dest & ~src2. Maps directly to
 *    OPCODE_AND_NOT which is dest = src1 & ~src2.
 *
 *  Scalar shifts: ARM64 LSLV/LSRV/ASRV use the full register width for the
 *    shift amount (mod 32 or mod 64). For I8/I16 we AND the shift amount.
 *
 *  SHL/SHR/SHA V128: NEON has SSHL/USHL for element-wise shifts by a vector
 *    of shift amounts. For a scalar shift amount (I8Op), we DUP the scalar
 *    to all lanes first. USHL shifts left if the element is positive, right
 *    if negative. For right shifts we negate the shift count.
 *
 *  CNTLZ: ARM64 CLZ instruction — exactly what we need. Works on W (32) or
 *    X (64) registers. For I8/I16 we zero-extend and add the correction.
 *
 *  INSERT/EXTRACT/SPLAT: NEON INS/UMOV/DUP instructions map perfectly.
 *    Endian note: the 360 uses big-endian lane ordering. We store vectors
 *    in little-endian NEON order, so lane indices need VEC128_B/W/D inversion.
 *
 *  PERMUTE/SWIZZLE: NEON TBL (table lookup) performs arbitrary byte permutes
 *    equivalent to x86 VPSHUFB. For two-table permutes we use TBL with 2 regs.
 *
 *  PACK/UNPACK: These are VMX-128 specific format conversions. Where NEON has
 *    direct equivalents (FCVT for half-float, SQXTUN for saturation) we use
 *    them. For the more exotic types (D3DCOLOR, UINT_2101010, ULONG_4202020)
 *    we use scalar C++ helper functions, the same fallback the x64 backend
 *    uses on non-AVX512 hardware.
 *
 *  ATOMIC_EXCHANGE: ARM64 ARMv8.1+ SWPAL instruction (swap + acquire/release).
 *    Falls back to LDXR/STXR loop for base ARMv8.0.
 *
 *  ATOMIC_COMPARE_EXCHANGE: ARM64 ARMv8.1+ CASAL (compare-and-swap).
 *    Falls back to LDXR/STXR loop for base ARMv8.0.
 *
 *  SET_ROUNDING_MODE: Writes the ARM FPCR register's RMode field.
 *    360 FPSCR bits [0:1] → ARM FPCR bits [22:23].
 *    Mapping: 0=Nearest, 1=+Inf, 2=-Inf, 3=Zero (same as FPSCR values).
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_sequences.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <algorithm>
#include <cmath>
#include <cstring>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/base/vec128.h"
#include "xenia/cpu/backend/arm64/arm64_compat.h"
#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/backend/arm64/arm64_op.h"
#include "xenia/cpu/backend/arm64/arm64_stack_layout.h"
#include "xenia/cpu/processor.h"

// Dolphin Arm64Gen — our code generation library.
using namespace Arm64Gen;

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

using namespace xe::cpu;
using namespace xe::cpu::hir;

// ---------------------------------------------------------------------------
// Local helper: get src as 32-bit register, loading constant if needed.
// ---------------------------------------------------------------------------
template <typename OpT>
static ARM64Reg GetSrc32(ARM64Emitter& e, const OpT& op, ARM64Reg scratch,
                          uint64_t mask = 0xFFFFFFFF) {
  if (op.is_constant) {
    e.MOVI2R(scratch, static_cast<uint32_t>(op.constant()) & (uint32_t)mask);
    return EncodeRegTo32(scratch);
  }
  return EncodeRegTo32(op.reg());
}
template <typename OpT>
static ARM64Reg GetSrc64(ARM64Emitter& e, const OpT& op, ARM64Reg scratch) {
  if (op.is_constant) {
    e.MOVI2R(scratch, static_cast<uint64_t>(op.constant()));
    return scratch;
  }
  return op.reg();
}
static ARM64Reg GetVecSrc(ARM64Emitter& e, const V128Op& op, ARM64Reg scratch) {
  if (op.is_constant) {
    e.LoadConstantV128(scratch, op.constant());
    return scratch;
  }
  return op.reg();
}

// ============================================================================
// OPCODE_AND  (#82)
// Bitwise AND. ARM64: AND (GPR), AND Vd.16B (vector).
// ============================================================================
struct AND_I8  : Sequence<AND_I8,  I<OPCODE_AND, I8Op,  I8Op,  I8Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.AND(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct AND_I16 : Sequence<AND_I16, I<OPCODE_AND, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.AND(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct AND_I32 : Sequence<AND_I32, I<OPCODE_AND, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // AND is commutative; if only src1 is constant, treat it as the immediate.
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ANDI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
               static_cast<uint32_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ANDI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src2.reg()),
               static_cast<uint32_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.AND(EncodeRegTo32(i.dest.reg()), a, b);
    }
  }
};
struct AND_I64 : Sequence<AND_I64, I<OPCODE_AND, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ANDI2R(i.dest.reg(), i.src1.reg(),
               static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ANDI2R(i.dest.reg(), i.src2.reg(),
               static_cast<uint64_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.AND(i.dest.reg(), a, b);
    }
  }
};
struct AND_V128 : Sequence<AND_V128, I<OPCODE_AND, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    e.AND(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_AND, AND_I8, AND_I16, AND_I32, AND_I64, AND_V128);

// ============================================================================
// OPCODE_AND_NOT  (#83)
// dest = src1 & ~src2. ARM64: BIC (bit clear) Xd, Xn, Xm = Xn & ~Xm.
// Note operand order: BIC gives us src1 & ~src2 directly.
// ============================================================================
struct AND_NOT_I8
    : Sequence<AND_NOT_I8, I<OPCODE_AND_NOT, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    // BIC Wd, Wn, Wm = Wn & ~Wm
    e.BIC(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct AND_NOT_I16
    : Sequence<AND_NOT_I16, I<OPCODE_AND_NOT, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.BIC(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct AND_NOT_I32
    : Sequence<AND_NOT_I32, I<OPCODE_AND_NOT, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.BIC(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct AND_NOT_I64
    : Sequence<AND_NOT_I64, I<OPCODE_AND_NOT, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
    ARM64Reg b = GetSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
    e.BIC(i.dest.reg(), a, b);
  }
};
struct AND_NOT_V128
    : Sequence<AND_NOT_V128, I<OPCODE_AND_NOT, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    // BIC Vd.16B, Vn.16B, Vm.16B = Vn & ~Vm
    e.BIC(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_AND_NOT, AND_NOT_I8, AND_NOT_I16, AND_NOT_I32,
                     AND_NOT_I64, AND_NOT_V128);

// ============================================================================
// OPCODE_OR  (#84)
// ============================================================================
struct OR_I8  : Sequence<OR_I8,  I<OPCODE_OR, I8Op,  I8Op,  I8Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.ORR(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct OR_I16 : Sequence<OR_I16, I<OPCODE_OR, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.ORR(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct OR_I32 : Sequence<OR_I32, I<OPCODE_OR, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ORRI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
               static_cast<uint32_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ORRI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src2.reg()),
               static_cast<uint32_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.ORR(EncodeRegTo32(i.dest.reg()), a, b);
    }
  }
};
struct OR_I64 : Sequence<OR_I64, I<OPCODE_OR, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.ORRI2R(i.dest.reg(), i.src1.reg(),
               static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.ORRI2R(i.dest.reg(), i.src2.reg(),
               static_cast<uint64_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.ORR(i.dest.reg(), a, b);
    }
  }
};
struct OR_V128 : Sequence<OR_V128, I<OPCODE_OR, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    e.ORR(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_OR, OR_I8, OR_I16, OR_I32, OR_I64, OR_V128);

// ============================================================================
// OPCODE_XOR  (#85)
// ============================================================================
struct XOR_I8  : Sequence<XOR_I8,  I<OPCODE_XOR, I8Op,  I8Op,  I8Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFF);
    e.EOR(EncodeRegTo32(i.dest.reg()), a, b);
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct XOR_I16 : Sequence<XOR_I16, I<OPCODE_XOR, I16Op, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1), 0xFFFF);
    ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0), 0xFFFF);
    e.EOR(EncodeRegTo32(i.dest.reg()), a, b);
  }
};
struct XOR_I32 : Sequence<XOR_I32, I<OPCODE_XOR, I32Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.EORI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
               static_cast<uint32_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.EORI2R(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src2.reg()),
               static_cast<uint32_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc32(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.EOR(EncodeRegTo32(i.dest.reg()), a, b);
    }
  }
};
struct XOR_I64 : Sequence<XOR_I64, I<OPCODE_XOR, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant && !i.src1.is_constant)
      e.EORI2R(i.dest.reg(), i.src1.reg(),
               static_cast<uint64_t>(i.src2.constant()),
               ARM64Emitter::ScratchReg(0));
    else if (i.src1.is_constant && !i.src2.is_constant)
      e.EORI2R(i.dest.reg(), i.src2.reg(),
               static_cast<uint64_t>(i.src1.constant()),
               ARM64Emitter::ScratchReg(0));
    else {
      ARM64Reg a = GetSrc64(e, i.src1, ARM64Emitter::ScratchReg(1));
      ARM64Reg b = GetSrc64(e, i.src2, ARM64Emitter::ScratchReg(0));
      e.EOR(i.dest.reg(), a, b);
    }
  }
};
struct XOR_V128 : Sequence<XOR_V128, I<OPCODE_XOR, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    e.EOR(i.dest.reg(), i.src1.reg(), b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_XOR, XOR_I8, XOR_I16, XOR_I32, XOR_I64, XOR_V128);

// ============================================================================
// OPCODE_NOT  (#86)
// ARM64: MVN (bitwise NOT) for GPRs. NOT Vd.16B for NEON.
// ============================================================================
struct NOT_I8  : Sequence<NOT_I8,  I<OPCODE_NOT, I8Op,  I8Op>>  {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.MVN(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct NOT_I16 : Sequence<NOT_I16, I<OPCODE_NOT, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.MVN(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFFFF);
  }
};
struct NOT_I32 : Sequence<NOT_I32, I<OPCODE_NOT, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.MVN(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct NOT_I64 : Sequence<NOT_I64, I<OPCODE_NOT, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.MVN(i.dest.reg(), i.src1.reg());
  }
};
struct NOT_V128 : Sequence<NOT_V128, I<OPCODE_NOT, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // NOT Vd.16B, Vn.16B — bitwise NOT of all 16 bytes.
    e.NOT(i.dest.reg(), i.src1.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_NOT, NOT_I8, NOT_I16, NOT_I32, NOT_I64, NOT_V128);

// ============================================================================
// OPCODE_SHL  (#87)
// Logical shift left. ARM64: LSL (immediate) or LSLV (register).
// SHL V128: scalar shift amount — DUP to vector, then SSHL.
// ============================================================================
struct SHL_I8 : Sequence<SHL_I8, I<OPCODE_SHL, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg s1 = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(0));
    if (i.src2.is_constant)
      e.LSL(EncodeRegTo32(i.dest.reg()), s1, i.src2.constant() & 7);
    else
      e.LSLV(EncodeRegTo32(i.dest.reg()), s1, EncodeRegTo32(i.src2.reg()));
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct SHL_I16 : Sequence<SHL_I16, I<OPCODE_SHL, I16Op, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg s1 = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(0));
    if (i.src2.is_constant)
      e.LSL(EncodeRegTo32(i.dest.reg()), s1, i.src2.constant() & 15);
    else
      e.LSLV(EncodeRegTo32(i.dest.reg()), s1, EncodeRegTo32(i.src2.reg()));
  }
};
struct SHL_I32 : Sequence<SHL_I32, I<OPCODE_SHL, I32Op, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg s1 = GetSrc32(e, i.src1, ARM64Emitter::ScratchReg(0));
    if (i.src2.is_constant)
      e.LSL(EncodeRegTo32(i.dest.reg()), s1, i.src2.constant() & 31);
    else
      e.LSLV(EncodeRegTo32(i.dest.reg()), s1, EncodeRegTo32(i.src2.reg()));
  }
};
struct SHL_I64 : Sequence<SHL_I64, I<OPCODE_SHL, I64Op, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg s1 = GetSrc64(e, i.src1, ARM64Emitter::ScratchReg(0));
    if (i.src2.is_constant)
      e.LSL(i.dest.reg(), s1, i.src2.constant() & 63);
    else
      e.LSLV(i.dest.reg(), s1, i.src2.reg());
  }
};
// V128 shift left by scalar byte: shift all 128 bits left by shamt bits.
// The x64 version emulates this with a C helper. We use the same approach
// for correctness — a native NEON version would be complex (cross-lane).
struct SHL_V128 : Sequence<SHL_V128, I<OPCODE_SHL, V128Op, V128Op, I8Op>> {
  // Shift the 128-bit vector left by shamt bits (across all 16 bytes).
  static void EmulateShlV128(vec128_t* dst, const vec128_t* src, uint8_t shamt) {
    shamt &= 0x7;  // only bits 0–2 matter
    if (shamt == 0) { *dst = *src; return; }
    for (int j = 0; j < 15; ++j) {
      dst->u8[j ^ 3] = (src->u8[j ^ 3] << shamt) |
                        (src->u8[(j + 1) ^ 3] >> (8 - shamt));
    }
    dst->u8[15 ^ 3] = src->u8[15 ^ 3] << shamt;
  }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Spill src1 to stack, call helper, reload dest.
    e.STR(INDEX_UNSIGNED, i.src1.reg(), SP, StackLayout::GUEST_SCRATCH_BASE);
    e.ADD(X0, SP, StackLayout::GUEST_SCRATCH_BASE);     // dst = src (in-place)
    e.ADD(X1, SP, StackLayout::GUEST_SCRATCH_BASE);     // src ptr
    if (i.src2.is_constant)
      e.MOVI2R(EncodeRegTo32(X2),
               static_cast<uint32_t>(i.src2.constant()) & 0xFF);
    else
      e.MOV(EncodeRegTo32(X2), EncodeRegTo32(i.src2.reg()));
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulateShlV128));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), SP, StackLayout::GUEST_SCRATCH_BASE);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SHL, SHL_I8, SHL_I16, SHL_I32, SHL_I64, SHL_V128);

// ============================================================================
// OPCODE_SHR  (#88) — Logical shift right (unsigned)
// ============================================================================
struct SHR_I8 : Sequence<SHR_I8, I<OPCODE_SHR, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    if (i.src2.is_constant)
      e.LSR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
            i.src2.constant() & 7);
    else
      e.LSRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
             EncodeRegTo32(i.src2.reg()));
  }
};
struct SHR_I16 : Sequence<SHR_I16, I<OPCODE_SHR, I16Op, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    if (i.src2.is_constant)
      e.LSR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
            i.src2.constant() & 15);
    else
      e.LSRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
             EncodeRegTo32(i.src2.reg()));
  }
};
struct SHR_I32 : Sequence<SHR_I32, I<OPCODE_SHR, I32Op, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant)
      e.LSR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
            i.src2.constant() & 31);
    else
      e.LSRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
             EncodeRegTo32(i.src2.reg()));
  }
};
struct SHR_I64 : Sequence<SHR_I64, I<OPCODE_SHR, I64Op, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant)
      e.LSR(i.dest.reg(), i.src1.reg(), i.src2.constant() & 63);
    else
      e.LSRV(i.dest.reg(), i.src1.reg(), i.src2.reg());
  }
};
struct SHR_V128 : Sequence<SHR_V128, I<OPCODE_SHR, V128Op, V128Op, I8Op>> {
  static void EmulateShrV128(vec128_t* dst, const vec128_t* src, uint8_t shamt) {
    shamt &= 0x7;
    if (shamt == 0) { *dst = *src; return; }
    for (int j = 15; j > 0; --j) {
      dst->u8[j ^ 3] = (src->u8[j ^ 3] >> shamt) |
                        (src->u8[(j - 1) ^ 3] << (8 - shamt));
    }
    dst->u8[0 ^ 3] = src->u8[0 ^ 3] >> shamt;
  }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.STR(INDEX_UNSIGNED, i.src1.reg(), SP, StackLayout::GUEST_SCRATCH_BASE);
    e.ADD(X0, SP, StackLayout::GUEST_SCRATCH_BASE);
    e.ADD(X1, SP, StackLayout::GUEST_SCRATCH_BASE);
    if (i.src2.is_constant)
      e.MOVI2R(EncodeRegTo32(X2),
               static_cast<uint32_t>(i.src2.constant()) & 0xFF);
    else
      e.MOV(EncodeRegTo32(X2), EncodeRegTo32(i.src2.reg()));
    e.MOVI2R(ARM64Emitter::ScratchReg(0),
             reinterpret_cast<uint64_t>(EmulateShrV128));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, i.dest.reg(), SP, StackLayout::GUEST_SCRATCH_BASE);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SHR, SHR_I8, SHR_I16, SHR_I32, SHR_I64, SHR_V128);

// ============================================================================
// OPCODE_SHA  (#89) — Arithmetic shift right (signed, sign-extending)
// ARM64: ASR (immediate) or ASRV (register).
// ============================================================================
struct SHA_I8 : Sequence<SHA_I8, I<OPCODE_SHA, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    if (i.src2.is_constant)
      e.ASR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
            i.src2.constant() & 7);
    else
      e.ASRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
             EncodeRegTo32(i.src2.reg()));
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct SHA_I16 : Sequence<SHA_I16, I<OPCODE_SHA, I16Op, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.SXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    if (i.src2.is_constant)
      e.ASR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
            i.src2.constant() & 15);
    else
      e.ASRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
             EncodeRegTo32(i.src2.reg()));
  }
};
struct SHA_I32 : Sequence<SHA_I32, I<OPCODE_SHA, I32Op, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant)
      e.ASR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
            i.src2.constant() & 31);
    else
      e.ASRV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()),
             EncodeRegTo32(i.src2.reg()));
  }
};
struct SHA_I64 : Sequence<SHA_I64, I<OPCODE_SHA, I64Op, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant)
      e.ASR(i.dest.reg(), i.src1.reg(), i.src2.constant() & 63);
    else
      e.ASRV(i.dest.reg(), i.src1.reg(), i.src2.reg());
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SHA, SHA_I8, SHA_I16, SHA_I32, SHA_I64);

// ============================================================================
// OPCODE_VECTOR_SHL / VECTOR_SHR / VECTOR_SHA  (#90–92)
// Element-wise vector shifts by a vector of shift amounts.
// NEON SSHL/USHL shift by signed lane values (positive=left, negative=right).
// For VECTOR_SHR (right shift): negate the shift vector then USHL.
// For VECTOR_SHA (arithmetic right): negate and SSHL.
// ============================================================================
struct VECTOR_SHL_V128
    : Sequence<VECTOR_SHL_V128,
               I<OPCODE_VECTOR_SHL, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg d = i.dest.reg();
    ARM64Reg a = i.src1.reg();
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:  e.USHL(d, a, b); break;  // unsigned shift left
      case INT16_TYPE: e.USHL(d, a, b); break;
      case INT32_TYPE: e.USHL(d, a, b); break;
      default:         e.USHL(d, a, b); break;
    }
  }
};
struct VECTOR_SHR_V128
    : Sequence<VECTOR_SHR_V128,
               I<OPCODE_VECTOR_SHR, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Right shift = negate shift amounts, then USHL.
    ARM64Reg b    = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg neg_b = ARM64Emitter::ScratchVec(1);
    e.NEG(neg_b, b);  // NEG Vd.xT, Vn.xT
    e.USHL(i.dest.reg(), i.src1.reg(), neg_b);
  }
};
struct VECTOR_SHA_V128
    : Sequence<VECTOR_SHA_V128,
               I<OPCODE_VECTOR_SHA, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Arithmetic right shift = negate shift amounts, then SSHL.
    ARM64Reg b    = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg neg_b = ARM64Emitter::ScratchVec(1);
    e.NEG(neg_b, b);
    e.SSHL(i.dest.reg(), i.src1.reg(), neg_b);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_SHL, VECTOR_SHL_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_SHR, VECTOR_SHR_V128);
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_SHA, VECTOR_SHA_V128);

// ============================================================================
// OPCODE_ROTATE_LEFT  (#93)
// ARM64: ROR Wd, Wn, #(32-shift) = rotate left.
// For variable rotation: RORV Wd, Wn, (32-Wm).
// ============================================================================
struct ROTATE_LEFT_I8
    : Sequence<ROTATE_LEFT_I8, I<OPCODE_ROTATE_LEFT, I8Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Extend to 32 bits, rotate, mask back to 8 bits.
    // src1 may be a folded constant (e.g. rotl of a literal by a variable).
    if (i.src1.is_constant)
      e.MOVI2R(EncodeRegTo32(i.dest.reg()),
               static_cast<uint32_t>(i.src1.constant()) & 0xFF);
    else
      e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    // For byte rotate: duplicate byte into upper bits so ROR works correctly.
    e.ORR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
          EncodeRegTo32(i.dest.reg()), ArithOption(EncodeRegTo32(i.dest.reg()),
          ST_LSL, 8));
    e.ORR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
          EncodeRegTo32(i.dest.reg()), ArithOption(EncodeRegTo32(i.dest.reg()),
          ST_LSL, 16));
    int shift = i.src2.is_constant ? (i.src2.constant() & 7) : 0;
    if (i.src2.is_constant)
      e.ROR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
            32 - shift);
    else {
      ARM64Reg tmp = ARM64Emitter::ScratchReg(0);
      e.MOVI2R(tmp, 32);
      e.SUB(tmp, tmp, EncodeRegTo32(i.src2.reg()));
      e.RORV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), tmp);
    }
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
  }
};
struct ROTATE_LEFT_I16
    : Sequence<ROTATE_LEFT_I16, I<OPCODE_ROTATE_LEFT, I16Op, I16Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant)
      e.MOVI2R(EncodeRegTo32(i.dest.reg()),
               static_cast<uint32_t>(i.src1.constant()) & 0xFFFF);
    else
      e.UXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.ORR(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()),
          EncodeRegTo32(i.dest.reg()), ArithOption(EncodeRegTo32(i.dest.reg()),
          ST_LSL, 16));
    if (i.src2.is_constant) {
      int shift = i.src2.constant() & 15;
      if (shift) e.ROR(EncodeRegTo32(i.dest.reg()),
                       EncodeRegTo32(i.dest.reg()), 32 - shift);
    } else {
      ARM64Reg tmp = ARM64Emitter::ScratchReg(0);
      e.MOVI2R(tmp, 32);
      e.SUB(tmp, tmp, EncodeRegTo32(i.src2.reg()));
      e.RORV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), tmp);
    }
    e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFFFF);
  }
};
struct ROTATE_LEFT_I32
    : Sequence<ROTATE_LEFT_I32, I<OPCODE_ROTATE_LEFT, I32Op, I32Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // src1 may be a folded constant.
    ARM64Reg src1 = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
    if (i.src1.is_constant)
      e.MOVI2R(src1, static_cast<uint32_t>(i.src1.constant()));
    else
      src1 = EncodeRegTo32(i.src1.reg());
    if (i.src2.is_constant) {
      int shift = i.src2.constant() & 31;
      if (shift)
        e.ROR(EncodeRegTo32(i.dest.reg()), src1, 32 - shift);
      else
        e.MOV(EncodeRegTo32(i.dest.reg()), src1);
    } else {
      ARM64Reg tmp = ARM64Emitter::ScratchReg(0);
      e.MOVI2R(tmp, 32);
      e.SUB(tmp, tmp, EncodeRegTo32(i.src2.reg()));
      e.RORV(EncodeRegTo32(i.dest.reg()), src1, tmp);
    }
  }
};
struct ROTATE_LEFT_I64
    : Sequence<ROTATE_LEFT_I64, I<OPCODE_ROTATE_LEFT, I64Op, I64Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // src1 may be a folded constant.
    ARM64Reg src1 = ARM64Emitter::ScratchReg(1);
    if (i.src1.is_constant)
      e.MOVI2R(src1, static_cast<uint64_t>(i.src1.constant()));
    else
      src1 = i.src1.reg();
    if (i.src2.is_constant) {
      int shift = i.src2.constant() & 63;
      if (shift)
        e.ROR(i.dest.reg(), src1, 64 - shift);
      else
        e.MOV(i.dest.reg(), src1);
    } else {
      ARM64Reg tmp = ARM64Emitter::ScratchReg(0);
      e.MOVI2R(tmp, 64ull);
      e.SUB(tmp, tmp, i.src2.reg());
      e.RORV(i.dest.reg(), src1, tmp);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ROTATE_LEFT, ROTATE_LEFT_I8, ROTATE_LEFT_I16,
                     ROTATE_LEFT_I32, ROTATE_LEFT_I64);

// ============================================================================
// OPCODE_VECTOR_ROTATE_LEFT  (#94)
// Element-wise rotate left by a vector of amounts.
// NEON has no native rotate — we emulate: (a << s) | (a >> (width - s)).
// For I32 type this is 3 NEON instructions. I8/I16 use a C++ helper.
// ============================================================================
struct VECTOR_ROTATE_LEFT_V128
    : Sequence<VECTOR_ROTATE_LEFT_V128,
               I<OPCODE_VECTOR_ROTATE_LEFT, V128Op, V128Op, V128Op>> {
  static void EmulateU8(vec128_t* dst, const vec128_t* a, const vec128_t* b) {
    for (int i = 0; i < 16; i++) {
      uint8_t s = b->u8[i] & 7;
      dst->u8[i] = (a->u8[i] << s) | (a->u8[i] >> (8 - s));
    }
  }
  static void EmulateU16(vec128_t* dst, const vec128_t* a, const vec128_t* b) {
    for (int i = 0; i < 8; i++) {
      uint16_t s = b->u16[i] & 15;
      dst->u16[i] = (a->u16[i] << s) | (a->u16[i] >> (16 - s));
    }
  }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = i.src1.reg();
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg d = i.dest.reg();
    switch (i.instr->flags & 0xFF) {
      case INT8_TYPE:
      case INT16_TYPE: {
        // Use C++ emulator — too complex for inline NEON without more regs.
        void* fn = (i.instr->flags & 0xFF) == INT8_TYPE
                       ? reinterpret_cast<void*>(EmulateU8)
                       : reinterpret_cast<void*>(EmulateU16);
        e.STR(INDEX_UNSIGNED, a, SP, StackLayout::GUEST_SCRATCH_BASE);
        e.STR(INDEX_UNSIGNED, b, SP, StackLayout::GUEST_SCRATCH_BASE + 16);
        e.ADD(X0, SP, StackLayout::GUEST_SCRATCH_BASE);         // dst
        e.ADD(X1, SP, StackLayout::GUEST_SCRATCH_BASE);         // src a
        e.ADD(X2, SP, StackLayout::GUEST_SCRATCH_BASE + 16);    // src b
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(fn));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case INT32_TYPE: {
        // Native NEON: (a << s) | (a >> (32 - s))
        // USHL Vtmp.4S, Va.4S, Vb.4S  (left shift by b)
        // NEG  Vneg.4S, Vb.4S
        // USHL Vd.4S, Va.4S, Vneg.4S  (right shift by (32-s) = shift by -b)
        // ORR  Vd.16B, Vd.16B, Vtmp.16B
        ARM64Reg tmp  = ARM64Emitter::ScratchVec(1);
        ARM64Reg negb = ARM64Emitter::ScratchVec(2);
        e.USHL(tmp, a, b);             // tmp = a << b
        e.NEG(negb, b);                // negb = -b (right shift)
        e.USHL(d, a, negb);            // d = a >> b (unsigned)
        e.ORR(d, d, tmp);              // d = (a<<b) | (a>>b)
        break;
      }
      default:
        e.UnimplementedInstr(i.instr);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_ROTATE_LEFT, VECTOR_ROTATE_LEFT_V128);

// ============================================================================
// OPCODE_VECTOR_AVERAGE  (#95)
// Element-wise (a + b + 1) / 2, rounding up. NEON: URHADD/SRHADD.
// For I32 which NEON doesn't have natively, fall back to C++ helper.
// ============================================================================
struct VECTOR_AVERAGE
    : Sequence<VECTOR_AVERAGE,
               I<OPCODE_VECTOR_AVERAGE, V128Op, V128Op, V128Op>> {
  static void EmulateI32Signed(vec128_t* dst, const vec128_t* a,
                                const vec128_t* b) {
    for (int i = 0; i < 4; i++) {
      dst->i32[i] =
          static_cast<int32_t>((int64_t(a->i32[i]) + int64_t(b->i32[i]) + 1) >> 1);
    }
  }
  static void EmulateI32Unsigned(vec128_t* dst, const vec128_t* a,
                                  const vec128_t* b) {
    for (int i = 0; i < 4; i++) {
      dst->u32[i] =
          static_cast<uint32_t>((uint64_t(a->u32[i]) + uint64_t(b->u32[i]) + 1) >> 1);
    }
  }
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg d = i.dest.reg();
    ARM64Reg a = i.src1.reg();
    bool is_unsigned = (i.instr->flags & ARITHMETIC_UNSIGNED) != 0;
    uint32_t part_type = i.instr->flags & 0xFF;
    switch (part_type) {
      case INT8_TYPE:
        // URHADD/SRHADD Vd.16B: (a+b+1)>>1
        if (is_unsigned) e.URHADD(d, a, b);
        else             e.SRHADD(d, a, b);
        break;
      case INT16_TYPE:
        if (is_unsigned) e.URHADD(d, a, b);
        else             e.SRHADD(d, a, b);
        break;
      case INT32_TYPE: {
        // No native I32 rounding average — use C++ helper.
        void* fn = is_unsigned
                       ? reinterpret_cast<void*>(EmulateI32Unsigned)
                       : reinterpret_cast<void*>(EmulateI32Signed);
        e.STR(INDEX_UNSIGNED, a, SP, StackLayout::GUEST_SCRATCH_BASE);
        e.STR(INDEX_UNSIGNED, b, SP, StackLayout::GUEST_SCRATCH_BASE + 16);
        e.ADD(X0, SP, StackLayout::GUEST_SCRATCH_BASE);
        e.ADD(X1, SP, StackLayout::GUEST_SCRATCH_BASE);
        e.ADD(X2, SP, StackLayout::GUEST_SCRATCH_BASE + 16);
        e.MOVI2R(ARM64Emitter::ScratchReg(0), reinterpret_cast<uint64_t>(fn));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      default:
        e.UnimplementedInstr(i.instr);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_AVERAGE, VECTOR_AVERAGE);

// ============================================================================
// OPCODE_BYTE_SWAP  (#96)
// ARM64: REV16 (2-byte), REV (4-byte), REV (8-byte). For V128: REV64+EXT.
// ============================================================================
struct BYTE_SWAP_I16
    : Sequence<BYTE_SWAP_I16, I<OPCODE_BYTE_SWAP, I16Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.REV16(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct BYTE_SWAP_I32
    : Sequence<BYTE_SWAP_I32, I<OPCODE_BYTE_SWAP, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.REV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct BYTE_SWAP_I64
    : Sequence<BYTE_SWAP_I64, I<OPCODE_BYTE_SWAP, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.REV(i.dest.reg(), i.src1.reg());
  }
};
struct BYTE_SWAP_V128
    : Sequence<BYTE_SWAP_V128, I<OPCODE_BYTE_SWAP, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // NEON equivalent of x64 vpshufb with XMMByteSwapMask:
    // REV64 reverses bytes in each 64-bit lane. EXT swaps the two lanes.
    // Combined: bytes are fully reversed (big-endian ↔ little-endian).
    e.REV64(i.dest.reg(), i.src1.reg());
    e.EXT(i.dest.reg(), i.dest.reg(), i.dest.reg(), 8);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_BYTE_SWAP, BYTE_SWAP_I16, BYTE_SWAP_I32,
                     BYTE_SWAP_I64, BYTE_SWAP_V128);

// ============================================================================
// OPCODE_CNTLZ  (#97) — Count Leading Zeros
// ARM64: CLZ Wd/Xd, Wn/Xn. Direct native instruction for all widths.
// For I8: extend to 32, CLZ, subtract 24 (extra leading zeros from extension).
// For I16: extend to 32, CLZ, subtract 16.
// ============================================================================
struct CNTLZ_I8 : Sequence<CNTLZ_I8, I<OPCODE_CNTLZ, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.CLZ(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
    // CLZ on zero-extended byte gives 24 + actual_lzcount.
    e.SUB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 24);
  }
};
struct CNTLZ_I16 : Sequence<CNTLZ_I16, I<OPCODE_CNTLZ, I8Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.UXTH(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
    e.CLZ(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
    e.SUB(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 16);
  }
};
struct CNTLZ_I32 : Sequence<CNTLZ_I32, I<OPCODE_CNTLZ, I8Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.CLZ(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.src1.reg()));
  }
};
struct CNTLZ_I64 : Sequence<CNTLZ_I64, I<OPCODE_CNTLZ, I8Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    e.CLZ(i.dest.reg(), i.src1.reg());
    // Truncate result to 8 bits (max value is 64, fits in a byte).
    e.MOV(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_CNTLZ, CNTLZ_I8, CNTLZ_I16, CNTLZ_I32, CNTLZ_I64);

// ============================================================================
// OPCODE_INSERT  (#98)
// Insert a scalar value into a specific lane of a vector register.
// src1 = input vector, src2 = lane index (constant), src3 = scalar value.
// ARM64: INS Vd.xT[lane], Wn
// Endian note: lane indices are inverted (VEC128_B/W/D).
// ============================================================================
struct INSERT_I8
    : Sequence<INSERT_I8, I<OPCODE_INSERT, V128Op, V128Op, I8Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    assert_true(i.src2.is_constant);
    int lane = VEC128_B(i.src2.constant());  // byte lane with endian flip
    // Copy src1 to dest first (if different).
    if (i.dest.reg() != i.src1.reg())
      e.ORR(i.dest.reg(), i.src1.reg(), i.src1.reg());
    // INS Vd.B[lane], Wn
    e.INS(i.dest.reg(), lane, EncodeRegTo32(i.src3.reg()));
  }
};
struct INSERT_I16
    : Sequence<INSERT_I16, I<OPCODE_INSERT, V128Op, V128Op, I8Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    assert_true(i.src2.is_constant);
    int lane = VEC128_W(i.src2.constant());  // 16-bit lane with endian flip
    if (i.dest.reg() != i.src1.reg())
      e.ORR(i.dest.reg(), i.src1.reg(), i.src1.reg());
    // INS Vd.H[lane], Wn
    e.INS(i.dest.reg(), lane, EncodeRegTo32(i.src3.reg()));
  }
};
struct INSERT_I32
    : Sequence<INSERT_I32, I<OPCODE_INSERT, V128Op, V128Op, I8Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    assert_true(i.src2.is_constant);
    int lane = VEC128_D(i.src2.constant());  // 32-bit lane
    if (i.dest.reg() != i.src1.reg())
      e.ORR(i.dest.reg(), i.src1.reg(), i.src1.reg());
    // INS Vd.S[lane], Wn
    e.INS(i.dest.reg(), lane, EncodeRegTo32(i.src3.reg()));
  }
};
EMITTER_OPCODE_TABLE(OPCODE_INSERT, INSERT_I8, INSERT_I16, INSERT_I32);

// ============================================================================
// OPCODE_EXTRACT  (#99)
// Extract a scalar from a specific lane of a vector register.
// ARM64: UMOV Wd, Vn.xT[lane]
// ============================================================================
struct EXTRACT_I8
    : Sequence<EXTRACT_I8, I<OPCODE_EXTRACT, I8Op, V128Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant) {
      int lane = VEC128_B(i.src2.constant());
      // UMOV Wd, Vn.B[lane] — zero-extends to 32 bits.
      e.UMOV(EncodeRegTo32(i.dest.reg()), i.src1.reg(), lane);
      e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
    } else {
      // Variable lane: extract via TBL with a computed index.
      // Build a 1-byte index vector: index = (3 ^ src2) & 0x1F.
      ARM64Reg idx_reg = ARM64Emitter::ScratchReg(0);
      e.AND(idx_reg, EncodeRegTo32(i.src2.reg()), 0x1F);
      e.EOR(idx_reg, idx_reg, 3);  // endian flip for byte lanes
      // DUP a scratch NEON reg with the index, then TBL.
      ARM64Reg idx_vec = ARM64Emitter::ScratchVec(0);
      e.DUP(idx_vec, idx_reg);
      e.TBL(idx_vec, i.src1.reg(), idx_vec);  // TBL with 1-reg table
      e.UMOV(EncodeRegTo32(i.dest.reg()), idx_vec, 0);
      e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFF);
    }
  }
};
struct EXTRACT_I16
    : Sequence<EXTRACT_I16, I<OPCODE_EXTRACT, I16Op, V128Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant) {
      int lane = VEC128_W(i.src2.constant());
      e.UMOV(EncodeRegTo32(i.dest.reg()), i.src1.reg(), lane);
      e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFFFF);
    } else {
      // Variable lane for 16-bit: compute byte offset = (lane^1) * 2.
      ARM64Reg idx = ARM64Emitter::ScratchReg(0);
      e.AND(idx, EncodeRegTo32(i.src2.reg()), 7);
      e.EOR(idx, idx, 1);
      e.LSL(idx, idx, 1);
      // Build a 2-byte index pair.
      ARM64Reg hi = ARM64Emitter::ScratchReg(1);
      e.ADD(hi, idx, 1);
      ARM64Reg idx_vec = ARM64Emitter::ScratchVec(0);
      e.INS(idx_vec, 0, idx);
      e.INS(idx_vec, 1, hi);
      e.TBL(idx_vec, i.src1.reg(), idx_vec);
      e.UMOV(EncodeRegTo32(i.dest.reg()), idx_vec, 0);
      e.AND(EncodeRegTo32(i.dest.reg()), EncodeRegTo32(i.dest.reg()), 0xFFFF);
    }
  }
};
struct EXTRACT_I32
    : Sequence<EXTRACT_I32, I<OPCODE_EXTRACT, I32Op, V128Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src2.is_constant) {
      int lane = VEC128_D(i.src2.constant());
      e.UMOV(EncodeRegTo32(i.dest.reg()), i.src1.reg(), lane);
    } else {
      // Variable lane: AND to [0,3], then use a TBL-based extract.
      ARM64Reg idx = ARM64Emitter::ScratchReg(0);
      e.AND(idx, EncodeRegTo32(i.src2.reg()), 3);
      e.LSL(idx, idx, 2);   // byte offset = lane * 4
      // Build a 4-byte index.
      ARM64Reg idx_vec = ARM64Emitter::ScratchVec(0);
      for (int b = 0; b < 4; b++) {
        ARM64Reg tmp = ARM64Emitter::ScratchReg(1);
        e.ADD(tmp, idx, b);
        e.INS(idx_vec, b, tmp);
      }
      e.TBL(idx_vec, i.src1.reg(), idx_vec);
      // For 32-bit, lane byte order is NOT swapped (VEC128_D is identity).
      // But NEON stores in little-endian, so bytes 0–3 of lane are already
      // in the right order.
      e.UMOV(EncodeRegTo32(i.dest.reg()), idx_vec, 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_EXTRACT, EXTRACT_I8, EXTRACT_I16, EXTRACT_I32);

// ============================================================================
// OPCODE_SPLAT  (#100)
// Broadcast a scalar value to all lanes of a vector.
// ARM64: DUP Vd.xT, Wn (from GPR) or DUP Vd.xT, Vn.xT[0] (from NEON).
// ============================================================================
struct SPLAT_I8 : Sequence<SPLAT_I8, I<OPCODE_SPLAT, V128Op, I8Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src1.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src1.reg());
    if (i.src1.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src1.constant()) & 0xFF);
    // DUP Vd.16B, Wn — broadcast byte to all 16 lanes.
    e.DUP(i.dest.reg(), src);
  }
};
struct SPLAT_I16 : Sequence<SPLAT_I16, I<OPCODE_SPLAT, V128Op, I16Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src1.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src1.reg());
    if (i.src1.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src1.constant()) & 0xFFFF);
    // DUP Vd.8H, Wn — broadcast 16-bit to all 8 lanes.
    e.DUP(i.dest.reg(), src);
  }
};
struct SPLAT_I32 : Sequence<SPLAT_I32, I<OPCODE_SPLAT, V128Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = i.src1.is_constant ? ARM64Emitter::ScratchReg(0)
                                      : EncodeRegTo32(i.src1.reg());
    if (i.src1.is_constant)
      e.MOVI2R(src, static_cast<uint32_t>(i.src1.constant()));
    // DUP Vd.4S, Wn — broadcast 32-bit to all 4 lanes.
    e.DUP(i.dest.reg(), src);
  }
};
struct SPLAT_F32 : Sequence<SPLAT_F32, I<OPCODE_SPLAT, V128Op, F32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    if (i.src1.is_constant) {
      // Load constant, then duplicate lane 0 to all lanes.
      e.LoadConstantF32(i.dest.reg(), i.src1.constant());
      // DUP Vd.4S, Vn.S[0]
      e.DUP(i.dest.reg(), EncodeRegToSingle(i.dest.reg()), 0);
    } else {
      // DUP Vd.4S, Vn.S[0] — broadcast lane 0 of src float register.
      e.DUP(i.dest.reg(), EncodeRegToSingle(i.src1.reg()), 0);
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SPLAT, SPLAT_I8, SPLAT_I16, SPLAT_I32, SPLAT_F32);

// ============================================================================
// OPCODE_PERMUTE  (#101)
// Arbitrary permute of vector elements.
// I32 control: constant 32-bit permutation selector (selects dwords from
//   src2/src3 based on 2-bit field per element).
// V128 control: 128-bit byte index table (like x86 VPSHUFB/vpermi2b).
//   NEON TBL with 2 registers handles the 32-byte lookup.
// ============================================================================
struct PERMUTE_I32
    : Sequence<PERMUTE_I32,
               I<OPCODE_PERMUTE, V128Op, I32Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    assert_true(i.instr->flags == INT32_TYPE);
    assert_true(i.src1.is_constant);
    uint32_t ctrl = i.src1.constant();

    // Extract 2-bit selector per dword: bits[1:0] select which dword from
    // src2 (0–3) or src3 (4–7) goes to dest lane 0..3.
    ARM64Reg a = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(0));
    ARM64Reg b = GetVecSrc(e, i.src3, ARM64Emitter::ScratchVec(1));
    ARM64Reg d = i.dest.reg();

    // Build a 16-byte TBL index vector that selects bytes from a 32-byte
    // table [src2_bytes | src3_bytes].
    // For each destination dword j: selector = (ctrl >> (j*8+2)) & 1
    //   => pick from a if bit=0, from b if bit=1.
    // Byte indices: for dword j selecting dword k from src: k*4+byte_off.
    uint8_t tbl[16];
    for (int j = 0; j < 4; j++) {
      uint8_t sel    = (ctrl >> (j * 8)) & 0x3;   // which dword (0–3)
      uint8_t src_hi = (ctrl >> (j * 8 + 2)) & 1; // 0=src2, 1=src3
      uint8_t base   = src_hi ? (16 + sel * 4) : (sel * 4);
      for (int b = 0; b < 4; b++) {
        tbl[j * 4 + b] = base + b;
      }
    }

    // Load the index table into a NEON register.
    ARM64Reg idx = ARM64Emitter::ScratchVec(2);
    e.LoadConstantV128(idx, *reinterpret_cast<vec128_t*>(tbl));

    // TBL with 2-register table: looks up indices into [a, b] concatenated.
    e.TBL(d, a, b, idx);
  }
};

struct PERMUTE_V128
    : Sequence<PERMUTE_V128,
               I<OPCODE_PERMUTE, V128Op, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ctrl = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg a    = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(1));
    ARM64Reg b    = GetVecSrc(e, i.src3, ARM64Emitter::ScratchVec(2));
    ARM64Reg d    = i.dest.reg();

    // The x64 backend XORs ctrl with XMMSwapWordMask to fix byte ordering.
    // We do the equivalent: XOR ctrl bytes with 0x03030303... (byte swap mask).
    ARM64Reg swap_mask = ARM64Emitter::ScratchVec(3);
    e.MOVI(swap_mask, 0x03);  // all bytes = 0x03
    e.EOR(ctrl, ctrl, swap_mask);

    // Mask to valid range [0,31]: AND with 0x1F.
    ARM64Reg range_mask = ARM64Emitter::ScratchVec(3);
    e.MOVI(range_mask, 0x1F);
    e.AND(ctrl, ctrl, range_mask);

    // TBL Vd.16B, {Va.16B, Vb.16B}, Vm.16B
    // Looks up each byte index from ctrl in the 32-byte table [a|b].
    e.TBL(d, a, b, ctrl);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_PERMUTE, PERMUTE_I32, PERMUTE_V128);

// ============================================================================
// OPCODE_SWIZZLE  (#102)
// Shuffle dword/float lanes within a single vector.
// src2 = 8-bit swizzle mask (same format as x86 VPSHUFD: 2 bits per lane).
// ARM64: No direct equivalent — use TBL with a computed byte-index table.
// ============================================================================
struct SWIZZLE
    : Sequence<SWIZZLE, I<OPCODE_SWIZZLE, V128Op, V128Op, OffsetOp>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    uint32_t mask = static_cast<uint32_t>(i.src2.value) & 0xFF;
    ARM64Reg src  = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg d    = i.dest.reg();

    // Build byte-index table: for each dest dword j, source dword =
    //   (mask >> (j*2)) & 3. Bytes: src_dword * 4 + byte_offset.
    uint8_t tbl[16];
    for (int j = 0; j < 4; j++) {
      int src_lane = (mask >> (j * 2)) & 3;
      for (int b = 0; b < 4; b++) {
        tbl[j * 4 + b] = static_cast<uint8_t>(src_lane * 4 + b);
      }
    }

    ARM64Reg idx = ARM64Emitter::ScratchVec(1);
    e.LoadConstantV128(idx, *reinterpret_cast<vec128_t*>(tbl));
    e.TBL(d, src, idx);  // single-register TBL
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SWIZZLE, SWIZZLE);

// ============================================================================
// OPCODE_PACK  (#103)
// VMX-128 pack instructions. 9 modes selected by instr->flags & PACK_TYPE_MODE.
// ============================================================================

// C++ helpers for complex pack operations we don't implement in native NEON.
// These match the x64 backend's emulation helpers exactly.

static void PackFLOAT16_2(vec128_t* dst, const vec128_t* src) {
  uint16_t* d = reinterpret_cast<uint16_t*>(dst);
  std::memset(dst, 0, sizeof(*dst));
  // Pack src.x and src.y as half-float into d[6] and d[7].
  for (int j = 0; j < 2; j++) {
    float v = src->f32[j];
    // Simple software float32 -> float16 conversion.
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    uint16_t sign = (bits >> 16) & 0x8000;
    int32_t  exp  = ((bits >> 23) & 0xFF) - 127 + 15;
    uint16_t mant = (bits >> 13) & 0x3FF;
    uint16_t half;
    if (exp <= 0)      half = sign;
    else if (exp >= 31) half = sign | 0x7C00;
    else               half = sign | (uint16_t(exp) << 10) | mant;
    d[7 - j] = half;
  }
}

static void PackFLOAT16_4(vec128_t* dst, const vec128_t* src) {
  uint16_t* d = reinterpret_cast<uint16_t*>(dst);
  std::memset(dst, 0, sizeof(*dst));
  for (int j = 0; j < 4; j++) {
    float v = src->f32[j];
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    uint16_t sign = (bits >> 16) & 0x8000;
    int32_t  exp  = ((bits >> 23) & 0xFF) - 127 + 15;
    uint16_t mant = (bits >> 13) & 0x3FF;
    uint16_t half;
    if (exp <= 0)       half = sign;
    else if (exp >= 31) half = sign | 0x7C00;
    else                half = sign | (uint16_t(exp) << 10) | mant;
    d[7 - (j ^ 2)] = half;
  }
}

static void PackD3DCOLOR(vec128_t* dst, const vec128_t* src) {
  // Saturate to [3.0, 3+1/255], extract byte, pack RGBA->ARGB.
  const float kMin = 3.0f, kMax = 3.0f + (255.0f / 256.0f);
  float r = std::min(std::max(src->f32[0], kMin), kMax);
  float g = std::min(std::max(src->f32[1], kMin), kMax);
  float b = std::min(std::max(src->f32[2], kMin), kMax);
  float a = std::min(std::max(src->f32[3], kMin), kMax);
  uint8_t rb = static_cast<uint8_t>((r - 3.0f) * 256.0f);
  uint8_t gb = static_cast<uint8_t>((g - 3.0f) * 256.0f);
  uint8_t bb = static_cast<uint8_t>((b - 3.0f) * 256.0f);
  uint8_t ab = static_cast<uint8_t>((a - 3.0f) * 256.0f);
  dst->u32[0] = (ab << 24) | (rb << 16) | (gb << 8) | bb;
  dst->u32[1] = dst->u32[2] = dst->u32[3] = 0;
}

static void PackSHORT_2(vec128_t* dst, const vec128_t* src) {
  const float kMin = -32768.0f, kMax = 32767.0f;
  int16_t x = static_cast<int16_t>(std::min(std::max(src->f32[0], kMin), kMax));
  int16_t y = static_cast<int16_t>(std::min(std::max(src->f32[1], kMin), kMax));
  dst->u32[0] = (uint32_t(uint16_t(y)) << 16) | uint16_t(x);
  dst->u32[1] = dst->u32[2] = dst->u32[3] = 0;
}

static void PackSHORT_4(vec128_t* dst, const vec128_t* src) {
  const float kMin = -32768.0f, kMax = 32767.0f;
  int16_t x = static_cast<int16_t>(std::min(std::max(src->f32[0], kMin), kMax));
  int16_t y = static_cast<int16_t>(std::min(std::max(src->f32[1], kMin), kMax));
  int16_t z = static_cast<int16_t>(std::min(std::max(src->f32[2], kMin), kMax));
  int16_t w = static_cast<int16_t>(std::min(std::max(src->f32[3], kMin), kMax));
  dst->u32[0] = (uint32_t(uint16_t(y)) << 16) | uint16_t(x);
  dst->u32[1] = (uint32_t(uint16_t(w)) << 16) | uint16_t(z);
  dst->u32[2] = dst->u32[3] = 0;
}

static void PackUINT_2101010(vec128_t* dst, const vec128_t* src) {
  auto clamp10 = [](float v) -> uint32_t {
    return static_cast<uint32_t>(
        std::min(std::max(static_cast<int32_t>(v), -512), 511) & 0x3FF);
  };
  auto clamp2 = [](float v) -> uint32_t {
    return static_cast<uint32_t>(
        std::min(std::max(static_cast<int32_t>(v), 0), 3) & 0x3);
  };
  dst->u32[0] = (clamp10(src->f32[0])) | (clamp10(src->f32[1]) << 10) |
                (clamp10(src->f32[2]) << 20) | (clamp2(src->f32[3]) << 30);
  dst->u32[1] = dst->u32[2] = dst->u32[3] = 0;
}

static void PackULONG_4202020(vec128_t* dst, const vec128_t* src) {
  auto clamp20 = [](float v) -> uint64_t {
    return static_cast<uint64_t>(
        std::min(std::max(static_cast<int32_t>(v), -524288), 524287) & 0xFFFFF);
  };
  auto clamp4 = [](float v) -> uint64_t {
    return static_cast<uint64_t>(
        std::min(std::max(static_cast<int32_t>(v), 0), 15) & 0xF);
  };
  uint64_t val = clamp20(src->f32[0]) | (clamp20(src->f32[1]) << 20) |
                 (clamp20(src->f32[2]) << 40) | (clamp4(src->f32[3]) << 60);
  std::memcpy(dst, &val, 8);
  dst->u32[2] = dst->u32[3] = 0;
}

static void Pack8In16(vec128_t* dst, const vec128_t* a, const vec128_t* b,
                      uint32_t flags) {
  bool in_unsigned  = IsPackInUnsigned(flags);
  bool out_unsigned = IsPackOutUnsigned(flags);
  bool out_saturate = IsPackOutSaturate(flags);
  for (int j = 0; j < 8; j++) {
    int16_t av = a->i16[j];
    uint8_t& dv = dst->u8[j];
    if (out_unsigned && out_saturate) {
      dv = static_cast<uint8_t>(
          std::min(std::max(static_cast<int>(av), 0), 255));
    } else if (!out_unsigned && out_saturate) {
      dv = static_cast<uint8_t>(
          std::min(std::max(static_cast<int>(av), -128), 127) & 0xFF);
    } else {
      dv = static_cast<uint8_t>(av & 0xFF);
    }
  }
  for (int j = 0; j < 8; j++) {
    int16_t bv = b->i16[j];
    uint8_t& dv = dst->u8[8 + j];
    if (out_unsigned && out_saturate)
      dv = static_cast<uint8_t>(std::min(std::max(static_cast<int>(bv), 0), 255));
    else if (!out_unsigned && out_saturate)
      dv = static_cast<uint8_t>(std::min(std::max(static_cast<int>(bv), -128), 127) & 0xFF);
    else
      dv = static_cast<uint8_t>(bv & 0xFF);
  }
}

static void Pack16In32(vec128_t* dst, const vec128_t* a, const vec128_t* b,
                       uint32_t flags) {
  bool out_unsigned = IsPackOutUnsigned(flags);
  bool out_saturate = IsPackOutSaturate(flags);
  for (int j = 0; j < 4; j++) {
    int32_t av = a->i32[j];
    uint16_t& dv = dst->u16[j];
    if (out_unsigned && out_saturate)
      dv = static_cast<uint16_t>(std::min(std::max(av, 0), 65535));
    else if (!out_unsigned && out_saturate)
      dv = static_cast<uint16_t>(std::min(std::max(av, -32768), 32767) & 0xFFFF);
    else
      dv = static_cast<uint16_t>(av & 0xFFFF);
  }
  for (int j = 0; j < 4; j++) {
    int32_t bv = b->i32[j];
    uint16_t& dv = dst->u16[4 + j];
    if (out_unsigned && out_saturate)
      dv = static_cast<uint16_t>(std::min(std::max(bv, 0), 65535));
    else if (!out_unsigned && out_saturate)
      dv = static_cast<uint16_t>(std::min(std::max(bv, -32768), 32767) & 0xFFFF);
    else
      dv = static_cast<uint16_t>(bv & 0xFFFF);
  }
}

// Helper macro to spill a V128 operand to the stack and get its address.
#define SPILL_VEC(REG, SLOT)                                           \
  e.STR(INDEX_UNSIGNED, REG, SP, StackLayout::GUEST_SCRATCH_BASE + (SLOT)*16)
#define ADDR_VEC(XREG, SLOT)                                          \
  e.ADD(XREG, SP, StackLayout::GUEST_SCRATCH_BASE + (SLOT)*16)

struct PACK : Sequence<PACK, I<OPCODE_PACK, V128Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a  = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg b  = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(1));
    ARM64Reg d  = i.dest.reg();
    uint32_t flags = i.instr->flags;

    // For all helper-based modes: spill inputs to stack, call, reload.
    // For native NEON modes: emit inline.
    switch (flags & PACK_TYPE_MODE) {
      case PACK_TYPE_D3DCOLOR: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);   // dst (slot 0)
        ADDR_VEC(X1, 1);   // src
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackD3DCOLOR));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_FLOAT16_2: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackFLOAT16_2));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_FLOAT16_4: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackFLOAT16_4));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_SHORT_2: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackSHORT_2));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_SHORT_4: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackSHORT_4));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_UINT_2101010: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackUINT_2101010));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_ULONG_4202020: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(PackULONG_4202020));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        break;
      }
      case PACK_TYPE_8_IN_16: {
        // Native NEON path for saturation cases:
        // SQXTUN (signed->unsigned saturate byte) or SQXTN (signed->signed).
        bool out_unsigned = IsPackOutUnsigned(flags);
        bool out_saturate = IsPackOutSaturate(flags);
        if (out_saturate) {
          // Use NEON SQXTUN2/SQXTN2 to pack 8 int16 → 8 int8 with saturation.
          // SQXTUN Vd.8B, Vn.8H (narrow with unsigned saturation).
          // We need to combine src1 and src2 into one 16-byte result.
          ARM64Reg tmp = ARM64Emitter::ScratchVec(2);
          if (out_unsigned) {
            e.SQXTUN(tmp, a);     // narrow src1 (low 8 bytes of dest)
            e.SQXTUN2(tmp, b);    // narrow src2 (high 8 bytes of dest)
          } else {
            e.SQXTN(tmp, a);
            e.SQXTN2(tmp, b);
          }
          // Apply byte-order mask (XMMByteOrderMask equivalent):
          // Reorder bytes for big-endian compatibility.
          e.REV32(tmp, tmp);  // byte-reverse within each dword
          e.ORR(d, tmp, tmp);
        } else {
          // Fall back to C++ helper for non-saturating cases.
          SPILL_VEC(a, 1);
          SPILL_VEC(b, 2);
          ADDR_VEC(X0, 0);
          ADDR_VEC(X1, 1);
          ADDR_VEC(X2, 2);
          e.MOVI2R(EncodeRegTo32(X3), flags);
          e.MOVI2R(ARM64Emitter::ScratchReg(0),
                   reinterpret_cast<uint64_t>(Pack8In16));
          e.BLR(ARM64Emitter::ScratchReg(0));
          e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        }
        break;
      }
      case PACK_TYPE_16_IN_32: {
        bool out_unsigned = IsPackOutUnsigned(flags);
        bool out_saturate = IsPackOutSaturate(flags);
        if (out_saturate) {
          ARM64Reg tmp = ARM64Emitter::ScratchVec(2);
          if (out_unsigned) {
            // UQXTN / UQXTN2: unsigned saturation int32 → int16.
            e.UQXTN(tmp, a);
            e.UQXTN2(tmp, b);
          } else {
            // SQXTN / SQXTN2: signed saturation.
            e.SQXTN(tmp, a);
            e.SQXTN2(tmp, b);
          }
          e.REV32(tmp, tmp);
          e.ORR(d, tmp, tmp);
        } else {
          SPILL_VEC(a, 1);
          SPILL_VEC(b, 2);
          ADDR_VEC(X0, 0);
          ADDR_VEC(X1, 1);
          ADDR_VEC(X2, 2);
          e.MOVI2R(EncodeRegTo32(X3), flags);
          e.MOVI2R(ARM64Emitter::ScratchReg(0),
                   reinterpret_cast<uint64_t>(Pack16In32));
          e.BLR(ARM64Emitter::ScratchReg(0));
          e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        }
        break;
      }
      default:
        e.UnimplementedInstr(i.instr);
        break;
    }
  }
};
EMITTER_OPCODE_TABLE(OPCODE_PACK, PACK);

// ============================================================================
// OPCODE_UNPACK  (#104)
// Reverse of PACK — expand packed formats back to float32x4.
// ============================================================================

static void UnpackFLOAT16_2(vec128_t* dst, const vec128_t* src) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int j = 0; j < 2; j++) {
    uint16_t h = s[VEC128_W(6 + j)];
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp  = ((h >> 10) & 0x1F);
    uint32_t mant = (h & 0x3FF) << 13;
    uint32_t bits;
    if (exp == 0)       bits = sign;
    else if (exp == 31) bits = sign | 0x7F800000 | mant;
    else                bits = sign | ((exp + 112) << 23) | mant;
    std::memcpy(&dst->f32[j], &bits, 4);
  }
  dst->f32[2] = 0.0f;
  dst->f32[3] = 1.0f;
}

static void UnpackFLOAT16_4(vec128_t* dst, const vec128_t* src) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int j = 0; j < 4; j++) {
    uint16_t h = s[VEC128_W(4 + j)];
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp  = ((h >> 10) & 0x1F);
    uint32_t mant = (h & 0x3FF) << 13;
    uint32_t bits;
    if (exp == 0)       bits = sign;
    else if (exp == 31) bits = sign | 0x7F800000 | mant;
    else                bits = sign | ((exp + 112) << 23) | mant;
    std::memcpy(&dst->f32[j], &bits, 4);
  }
}

static void UnpackD3DCOLOR(vec128_t* dst, const vec128_t* src) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(
      const_cast<vec128_t*>(src));
  // ARGB → RGBA, each component as float = 3.0 + byte/256
  const float base = 3.0f;
  uint32_t packed = src->u32[0];
  uint8_t a = (packed >> 24) & 0xFF;
  uint8_t r = (packed >> 16) & 0xFF;
  uint8_t g = (packed >> 8)  & 0xFF;
  uint8_t b_val = (packed)   & 0xFF;
  dst->f32[0] = base + r / 256.0f;
  dst->f32[1] = base + g / 256.0f;
  dst->f32[2] = base + b_val / 256.0f;
  dst->f32[3] = base + a / 256.0f;
}

static void UnpackSHORT_2(vec128_t* dst, const vec128_t* src) {
  // (VD.x) = 3.0 + (VB.iw>>16)*2^-22, (VD.y) = 3.0 + (VB.iw)*2^-22
  int16_t x = static_cast<int16_t>(src->u32[0] >> 16);
  int16_t y = static_cast<int16_t>(src->u32[0] & 0xFFFF);
  const float kQNaN = std::numeric_limits<float>::quiet_NaN();
  const float kBase = 3.0f, kScale = 1.0f / (1 << 22);
  const float kOverflow = kBase + 0x10000 * kScale;
  auto conv = [&](int16_t v) -> float {
    float r = kBase + v * kScale;
    return (r == kOverflow) ? kQNaN : r;
  };
  dst->f32[0] = conv(x);
  dst->f32[1] = conv(y);
  dst->f32[2] = 0.0f;
  dst->f32[3] = 1.0f;
}

static void UnpackSHORT_4(vec128_t* dst, const vec128_t* src) {
  const float kBase = 3.0f, kScale = 1.0f / (1 << 22);
  const float kOverflow = kBase + 0x10000 * kScale;
  const float kQNaN = std::numeric_limits<float>::quiet_NaN();
  int16_t x = static_cast<int16_t>(src->u32[0] >> 16);
  int16_t y = static_cast<int16_t>(src->u32[0] & 0xFFFF);
  int16_t z = static_cast<int16_t>(src->u32[1] >> 16);
  int16_t w = static_cast<int16_t>(src->u32[1] & 0xFFFF);
  auto conv = [&](int16_t v) -> float {
    float r = kBase + v * kScale;
    return (r == kOverflow) ? kQNaN : r;
  };
  dst->f32[0] = conv(x);
  dst->f32[1] = conv(y);
  dst->f32[2] = conv(z);
  dst->f32[3] = conv(w);
}

static void UnpackUINT_2101010(vec128_t* dst, const vec128_t* src) {
  uint32_t v = src->u32[0];
  auto signext10 = [](int v) -> int32_t {
    return (v & 0x200) ? (v | ~0x3FF) : v;
  };
  int32_t x = signext10(v & 0x3FF);
  int32_t y = signext10((v >> 10) & 0x3FF);
  int32_t z = signext10((v >> 20) & 0x3FF);
  uint32_t w = (v >> 30) & 0x3;
  const float kBase = 3.0f, kScale = 1.0f / (1 << 22);
  dst->f32[0] = kBase + x * kScale;
  dst->f32[1] = kBase + y * kScale;
  dst->f32[2] = kBase + z * kScale;
  dst->f32[3] = kBase + w * (kScale * (1 << 12));
}

static void UnpackULONG_4202020(vec128_t* dst, const vec128_t* src) {
  uint64_t v = 0;
  std::memcpy(&v, src, 8);
  auto signext20 = [](int64_t x) -> int32_t {
    return static_cast<int32_t>(
        (x & 0x80000LL) ? (x | ~int64_t(0xFFFFF)) : x);
  };
  int32_t x = signext20(v & 0xFFFFF);
  int32_t y = signext20((v >> 20) & 0xFFFFF);
  int32_t z = signext20((v >> 40) & 0xFFFFF);
  uint32_t w = static_cast<uint32_t>((v >> 60) & 0xF);
  const float kBase = 3.0f, kScale = 1.0f / (1 << 22);
  dst->f32[0] = kBase + x * kScale;
  dst->f32[1] = kBase + y * kScale;
  dst->f32[2] = kBase + z * kScale;
  dst->f32[3] = kBase + w * (kScale * (1 << 12));
}

static void Unpack8In16(vec128_t* dst, const vec128_t* src, uint32_t flags) {
  bool in_unsigned = IsPackInUnsigned(flags);
  for (int j = 0; j < 8; j++) {
    uint8_t  bv = src->u8[j];
    int16_t& dv = dst->i16[j];
    if (in_unsigned) dv = static_cast<int16_t>(bv);
    else             dv = static_cast<int16_t>(static_cast<int8_t>(bv));
  }
  for (int j = 0; j < 8; j++) dst->i16[j + 8] = 0;
}

static void Unpack16In32(vec128_t* dst, const vec128_t* src, uint32_t flags) {
  bool in_unsigned = IsPackInUnsigned(flags);
  for (int j = 0; j < 4; j++) {
    uint16_t sv = src->u16[j];
    int32_t& dv = dst->i32[j];
    if (in_unsigned) dv = static_cast<int32_t>(sv);
    else             dv = static_cast<int32_t>(static_cast<int16_t>(sv));
  }
  for (int j = 0; j < 4; j++) dst->i32[j + 4] = 0;
}

struct UNPACK : Sequence<UNPACK, I<OPCODE_UNPACK, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg d = i.dest.reg();
    uint32_t flags = i.instr->flags;

    // All unpack modes use C++ helpers for correctness.
    // They write to GUEST_SCRATCH_BASE (slot 0) and read src from slot 1.
    void* helper = nullptr;
    switch (flags & PACK_TYPE_MODE) {
      case PACK_TYPE_D3DCOLOR:    helper = reinterpret_cast<void*>(UnpackD3DCOLOR);    break;
      case PACK_TYPE_FLOAT16_2:   helper = reinterpret_cast<void*>(UnpackFLOAT16_2);   break;
      case PACK_TYPE_FLOAT16_4:   helper = reinterpret_cast<void*>(UnpackFLOAT16_4);   break;
      case PACK_TYPE_SHORT_2:     helper = reinterpret_cast<void*>(UnpackSHORT_2);     break;
      case PACK_TYPE_SHORT_4:     helper = reinterpret_cast<void*>(UnpackSHORT_4);     break;
      case PACK_TYPE_UINT_2101010: helper = reinterpret_cast<void*>(UnpackUINT_2101010); break;
      case PACK_TYPE_ULONG_4202020: helper = reinterpret_cast<void*>(UnpackULONG_4202020); break;
      case PACK_TYPE_8_IN_16: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(EncodeRegTo32(X2), flags);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(Unpack8In16));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        return;
      }
      case PACK_TYPE_16_IN_32: {
        SPILL_VEC(a, 1);
        ADDR_VEC(X0, 0);
        ADDR_VEC(X1, 1);
        e.MOVI2R(EncodeRegTo32(X2), flags);
        e.MOVI2R(ARM64Emitter::ScratchReg(0),
                 reinterpret_cast<uint64_t>(Unpack16In32));
        e.BLR(ARM64Emitter::ScratchReg(0));
        e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
        return;
      }
      default:
        e.UnimplementedInstr(i.instr);
        return;
    }

    // All single-operand unpack helpers: (dst*, src*)
    SPILL_VEC(a, 1);
    ADDR_VEC(X0, 0);
    ADDR_VEC(X1, 1);
    e.MOVI2R(ARM64Emitter::ScratchReg(0), reinterpret_cast<uint64_t>(helper));
    e.BLR(ARM64Emitter::ScratchReg(0));
    e.LDR(INDEX_UNSIGNED, d, SP, StackLayout::GUEST_SCRATCH_BASE);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_UNPACK, UNPACK);

// Clean up helper macros.
#undef SPILL_VEC
#undef ADDR_VEC

// ============================================================================
// OPCODE_ATOMIC_EXCHANGE  (#105)
// Atomically swap a value with guest memory. Returns the old value.
// ARM64 ARMv8.1+: SWPAL Ws, Wt, [Xn] — swap with full barrier.
// ARMv8.0 fallback: LDXR / STXR loop.
// The 360 is big-endian, so byte-swap before store and after load.
// ============================================================================
struct ATOMIC_EXCHANGE_I32
    : Sequence<ATOMIC_EXCHANGE_I32,
               I<OPCODE_ATOMIC_EXCHANGE, I32Op, I64Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    // Compute effective address: membase + guest_addr.
    ARM64Reg ea  = ARM64Emitter::ScratchReg(0);
    e.MOV(EncodeRegTo32(ea), EncodeRegTo32(i.src1.reg()));  // zero-extend guest addr
    e.ADD(ea, e.GetMembaseReg(), ea);

    // Byte-swap the value we're writing (little→big endian for guest).
    // Must be a W reg: REV/SWPAL/STLXR pick 32- vs 64-bit form from the
    // data register width — an X reg here would byteswap into the top half
    // and store 8 bytes.
    ARM64Reg val = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
    if (i.src2.is_constant)
      e.MOVI2R(val, static_cast<uint32_t>(i.src2.constant()));
    else
      e.MOV(val, EncodeRegTo32(i.src2.reg()));
    e.REV(val, val);

    ARM64Reg old_val = EncodeRegTo32(i.dest.reg());

#if defined(__ARM_FEATURE_ATOMICS)
    // ARMv8.1+ SWPAL: atomic swap with acquire+release semantics.
    // SWPAL Ws (value to store), Wt (receives old value), [Xn]
    e.SWPAL(val, old_val, ea);
#else
    // ARMv8.0 fallback: exclusive load/store retry loop.
    // LDAXR Wt, [Xn]   — load-exclusive with acquire
    // STLXR Ws, Wt, [Xn] — store-exclusive with release
    ARM64Reg status = ARM64Emitter::ScratchReg(2);
    auto retry = e.GetCodePtr();
    e.LDAXR(old_val, ea);
    e.STLXR(status, val, ea);
    // CBNZ status, retry — retry if store failed (another CPU modified ea).
    e.CBNZ(status, retry);
#endif

    // Byte-swap the loaded old value back to little-endian.
    e.REV(old_val, old_val);
  }
};
struct ATOMIC_EXCHANGE_I64
    : Sequence<ATOMIC_EXCHANGE_I64,
               I<OPCODE_ATOMIC_EXCHANGE, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    e.MOV(EncodeRegTo32(ea), EncodeRegTo32(i.src1.reg()));  // zero-extend guest addr
    e.ADD(ea, e.GetMembaseReg(), ea);
    ARM64Reg val = ARM64Emitter::ScratchReg(1);
    if (i.src2.is_constant)
      e.MOVI2R(val, static_cast<uint64_t>(i.src2.constant()));
    else
      e.MOV(val, i.src2.reg());
    e.REV(val, val);
    ARM64Reg old_val = i.dest.reg();
#if defined(__ARM_FEATURE_ATOMICS)
    e.SWPAL(val, old_val, ea);
#else
    ARM64Reg status = ARM64Emitter::ScratchReg(2);
    auto retry = e.GetCodePtr();
    e.LDAXR(old_val, ea);
    e.STLXR(status, val, ea);
    e.CBNZ(status, retry);
#endif
    e.REV(old_val, old_val);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ATOMIC_EXCHANGE, ATOMIC_EXCHANGE_I32,
                     ATOMIC_EXCHANGE_I64);

// ============================================================================
// OPCODE_ATOMIC_COMPARE_EXCHANGE  (#106)
// If *addr == expected, atomically set *addr = desired. Returns 1 if swapped.
// ARM64 ARMv8.1+: CASAL Ws (expected/result), Wt (desired), [Xn].
// ARMv8.0 fallback: LDAXR / STLXR loop with comparison.
// ============================================================================
struct ATOMIC_COMPARE_EXCHANGE_I32
    : Sequence<ATOMIC_COMPARE_EXCHANGE_I32,
               I<OPCODE_ATOMIC_COMPARE_EXCHANGE, I8Op, I64Op, I32Op, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    if (i.src1.is_constant)  // address can constant-fold (e.g. fixed lock word)
      e.MOVI2R(EncodeRegTo32(ea), static_cast<uint32_t>(i.src1.constant()));
    else
      e.MOV(EncodeRegTo32(ea), EncodeRegTo32(i.src1.reg()));  // zero-extend
    e.ADD(ea, e.GetMembaseReg(), ea);

    // NO byte-swapping here: the PPC frontend (InstrEmit_stwcx/stdcx) already
    // passes ByteSwap(reserved) / ByteSwap(rt), i.e. expected and desired are
    // the raw guest-memory representation. The x64 reference compares them
    // against memory directly (lock cmpxchg, no bswap). REV'ing here was a
    // DOUBLE swap: it stored 1 as 0x01000000 into spin-lock words and made the
    // release CAS (expected==1) never match — guest threads deadlocked
    // spinning on locks that could never be observed as released.
    ARM64Reg expected = EncodeRegTo32(ARM64Emitter::ScratchReg(1));
    ARM64Reg desired  = EncodeRegTo32(ARM64Emitter::ScratchReg(2));
    if (i.src2.is_constant)
      e.MOVI2R(expected, static_cast<uint32_t>(i.src2.constant()));
    else
      e.MOV(expected, EncodeRegTo32(i.src2.reg()));
    if (i.src3.is_constant)
      e.MOVI2R(desired, static_cast<uint32_t>(i.src3.constant()));
    else
      e.MOV(desired, EncodeRegTo32(i.src3.reg()));

    ARM64Reg dest = EncodeRegTo32(i.dest.reg());

#if defined(__ARM_FEATURE_ATOMICS)
    // CASAL Ws, Wt, [Xn]:
    //   If [Xn] == Ws: [Xn] = Wt (swap happened), Ws unchanged.
    //   Else: Ws = [Xn] (loaded value), no swap.
    // After: Z flag is set if exchange occurred.
    e.CASAL(expected, desired, ea);
    // Check if exchange succeeded: expected still holds its original value
    // if swap occurred, otherwise it holds what we loaded.
    // Use a separate scratch for the original expected to compare.
    // Simpler: CASAL sets no flags — compare expected with result.
    // If expected still == byte-swapped(src2), the CAS succeeded.
    ARM64Reg orig_expected = EncodeRegTo32(ARM64Emitter::ScratchReg(3));
    if (i.src2.is_constant)
      e.MOVI2R(orig_expected, static_cast<uint32_t>(i.src2.constant()));
    else
      e.MOV(orig_expected, EncodeRegTo32(i.src2.reg()));
    // If expected still == orig_expected (no one changed it), swap succeeded.
    e.CMP(expected, orig_expected);
    e.CSET(dest, CC_EQ);
#else
    // ARMv8.0 fallback: LDAXR / CMP / STLXR loop.
    //   retry: ldaxr loaded, [ea]
    //          cmp loaded, expected
    //          b.ne fail            ; value differs -> no store
    //          stlxr status, desired, [ea]
    //          cbnz status, retry   ; lost exclusivity -> retry
    //          mov dest, 1
    //          b done
    //   fail:  clrex                ; drop the exclusive monitor
    //          mov dest, 0
    //   done:
    ARM64Reg loaded = EncodeRegTo32(ARM64Emitter::ScratchReg(3));
    ARM64Reg status = dest;  // dest is free until the result is set
    auto retry = e.GetCodePtr();
    e.LDAXR(loaded, ea);
    e.CMP(loaded, expected);
    auto fail = e.B(CC_NEQ);
    e.STLXR(status, desired, ea);
    e.CBNZ(status, retry);
    e.MOVI2R(dest, 1);
    auto done = e.B();
    e.SetJumpTarget(fail);
    e.CLREX();
    e.MOVI2R(dest, 0);
    e.SetJumpTarget(done);
#endif
  }
};
// 64-bit variant for stdcx. (ldarx/stdcx pairs). Same shape as I32 with X
// data registers; expected/desired arrive pre-byteswapped from the frontend.
struct ATOMIC_COMPARE_EXCHANGE_I64
    : Sequence<ATOMIC_COMPARE_EXCHANGE_I64,
               I<OPCODE_ATOMIC_COMPARE_EXCHANGE, I8Op, I64Op, I64Op, I64Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg ea = ARM64Emitter::ScratchReg(0);
    if (i.src1.is_constant)  // address can constant-fold (e.g. fixed lock word)
      e.MOVI2R(EncodeRegTo32(ea), static_cast<uint32_t>(i.src1.constant()));
    else
      e.MOV(EncodeRegTo32(ea), EncodeRegTo32(i.src1.reg()));  // zero-extend
    e.ADD(ea, e.GetMembaseReg(), ea);

    ARM64Reg expected = ARM64Emitter::ScratchReg(1);
    ARM64Reg desired  = ARM64Emitter::ScratchReg(2);
    if (i.src2.is_constant)
      e.MOVI2R(expected, static_cast<uint64_t>(i.src2.constant()));
    else
      e.MOV(expected, i.src2.reg());
    if (i.src3.is_constant)
      e.MOVI2R(desired, static_cast<uint64_t>(i.src3.constant()));
    else
      e.MOV(desired, i.src3.reg());

    ARM64Reg dest = EncodeRegTo32(i.dest.reg());

#if defined(__ARM_FEATURE_ATOMICS)
    e.CASAL(expected, desired, ea);
    ARM64Reg orig_expected = ARM64Emitter::ScratchReg(3);
    if (i.src2.is_constant)
      e.MOVI2R(orig_expected, static_cast<uint64_t>(i.src2.constant()));
    else
      e.MOV(orig_expected, i.src2.reg());
    e.CMP(expected, orig_expected);
    e.CSET(dest, CC_EQ);
#else
    ARM64Reg loaded = ARM64Emitter::ScratchReg(3);
    ARM64Reg status = dest;  // dest is free until the result is set
    auto retry = e.GetCodePtr();
    e.LDAXR(loaded, ea);
    e.CMP(loaded, expected);
    auto fail = e.B(CC_NEQ);
    e.STLXR(status, desired, ea);
    e.CBNZ(status, retry);
    e.MOVI2R(dest, 1);
    auto done = e.B();
    e.SetJumpTarget(fail);
    e.CLREX();
    e.MOVI2R(dest, 0);
    e.SetJumpTarget(done);
#endif
  }
};
EMITTER_OPCODE_TABLE(OPCODE_ATOMIC_COMPARE_EXCHANGE,
                     ATOMIC_COMPARE_EXCHANGE_I32, ATOMIC_COMPARE_EXCHANGE_I64);

// ============================================================================
// OPCODE_SET_ROUNDING_MODE  (#107)
// Sets the floating-point rounding mode from a 360 FPSCR value.
// 360 FPSCR[1:0]: 0=Nearest, 1=+Inf, 2=-Inf, 3=Zero.
// ARM64 FPCR[23:22] (RMode): 0=Nearest, 1=+Inf, 2=-Inf, 3=Zero.
// Mapping is identical! We just need to shift the 2 bits into FPCR position.
// ============================================================================
struct SET_ROUNDING_MODE_I32
    : Sequence<SET_ROUNDING_MODE_I32,
               I<OPCODE_SET_ROUNDING_MODE, VoidOp, I32Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg mode = ARM64Emitter::ScratchReg(0);
    ARM64Reg fpcr = ARM64Emitter::ScratchReg(1);

    if (i.src1.is_constant) {
      e.MOVI2R(mode, static_cast<uint32_t>(i.src1.constant()) & 0x3);
    } else {
      e.AND(mode, EncodeRegTo32(i.src1.reg()), 0x3);
    }

    // Read current FPCR.
    e.MRS(fpcr, PStateField::FPCR);

    // Clear the two RMode bits [23:22] in FPCR.
    // BIC Xd, Xn, #(3 << 22)
    e.ANDI2R(fpcr, fpcr, ~(uint64_t(0x3) << 22), mode);

    // Shift the 2-bit mode value into FPCR[23:22].
    e.LSL(mode, mode, 22);
    e.ORR(fpcr, fpcr, mode);

    // Write back to FPCR.
    e._MSR(PStateField::FPCR, fpcr);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_SET_ROUNDING_MODE, SET_ROUNDING_MODE_I32);

// ============================================================================
// OPCODE_VECTOR_CONVERT_I2F  — packed int32 → float32, 4 lanes.
// ARM SCVTF/UCVTF.4S convert with the current rounding mode (round-to-nearest-
// even), matching AltiVec semantics directly in hardware (no manual rounding
// like the x64 path needs).
// ============================================================================
struct VECTOR_CONVERT_I2F
    : Sequence<VECTOR_CONVERT_I2F,
               I<OPCODE_VECTOR_CONVERT_I2F, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    if (i.instr->flags & ARITHMETIC_UNSIGNED)
      e.UCVTF_4S(i.dest.reg(), src);
    else
      e.SCVTF_4S(i.dest.reg(), src);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_CONVERT_I2F, VECTOR_CONVERT_I2F);

// ============================================================================
// OPCODE_VECTOR_CONVERT_F2I  — packed float32 → int32, round toward zero.
// ARM FCVTZS/FCVTZU.4S saturate out-of-range to INT_MAX/MIN (or UINT_MAX / 0)
// and map NaN → 0, matching AltiVec vctsxs/vctuxs and the x64 saturation path.
// ============================================================================
struct VECTOR_CONVERT_F2I
    : Sequence<VECTOR_CONVERT_F2I,
               I<OPCODE_VECTOR_CONVERT_F2I, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg src = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    if (i.instr->flags & ARITHMETIC_UNSIGNED)
      e.FCVTZU_4S(i.dest.reg(), src);
    else
      e.FCVTZS_4S(i.dest.reg(), src);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_VECTOR_CONVERT_F2I, VECTOR_CONVERT_F2I);

// ============================================================================
// OPCODE_DOT_PRODUCT_3 / _4  — float dot product of the low 3 / all 4 lanes.
// Result scalar lands in lane 0 of dest; the scalar FADDP/FADD writes zero the
// upper lanes of the dest V register, matching x64 dpps with result-mask 0001.
// ============================================================================
static void EmitDotProduct(ARM64Emitter& e, ARM64Reg dest_q, ARM64Reg a,
                           ARM64Reg b, bool include_lane3) {
  // prod = a * b lane-wise, then horizontally sum the wanted lanes into
  // dest.S[0] using only 32-bit-lane ops (DUP broadcasts a lane, scalar FADD
  // accumulates). Upper dest lanes are left undefined; the result type is F32
  // so consumers read only lane 0.
  ARM64Reg prod = ARM64Emitter::ScratchVec(2);
  ARM64Reg tmp  = ARM64Emitter::ScratchVec(3);
  ARM64Reg dest_s = EncodeRegToSingle(dest_q);
  ARM64Reg tmp_s  = EncodeRegToSingle(tmp);
  e.FMUL(prod, a, b);                  // vector 4S: [p0,p1,p2,p3]
  e.INS(dest_q, 0, prod, 0);           // dest.S0 = p0
  e.DUP(tmp, prod, 1); e.FADD(dest_s, dest_s, tmp_s);  // += p1
  e.DUP(tmp, prod, 2); e.FADD(dest_s, dest_s, tmp_s);  // += p2
  if (include_lane3) {
    e.DUP(tmp, prod, 3); e.FADD(dest_s, dest_s, tmp_s);  // += p3
  }
}
struct DOT_PRODUCT_3_V128
    : Sequence<DOT_PRODUCT_3_V128,
               I<OPCODE_DOT_PRODUCT_3, F32Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(1));
    EmitDotProduct(e, i.dest.reg(), a, b, false);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_DOT_PRODUCT_3, DOT_PRODUCT_3_V128);
struct DOT_PRODUCT_4_V128
    : Sequence<DOT_PRODUCT_4_V128,
               I<OPCODE_DOT_PRODUCT_4, F32Op, V128Op, V128Op>> {
  static void Emit(ARM64Emitter& e, const EmitArgType& i) {
    ARM64Reg a = GetVecSrc(e, i.src1, ARM64Emitter::ScratchVec(0));
    ARM64Reg b = GetVecSrc(e, i.src2, ARM64Emitter::ScratchVec(1));
    EmitDotProduct(e, i.dest.reg(), a, b, true);
  }
};
EMITTER_OPCODE_TABLE(OPCODE_DOT_PRODUCT_4, DOT_PRODUCT_4_V128);

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
