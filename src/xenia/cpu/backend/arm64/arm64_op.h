/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_OP_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_OP_H_

#include "xenia/base/platform.h"
#include "xenia/base/logging.h"
#if XE_ARCH_ARM64

#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/hir/instr.h"

// Dolphin Arm64Emitter register types
#include "third_party/arm64emitter/Arm64Emitter.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

using namespace xe::cpu;
using namespace xe::cpu::hir;
using namespace Arm64Gen;

// ---------------------------------------------------------------------------
// Vector byte-order macros
//
// The Xbox 360 is big-endian. NEON is little-endian. When we load a vec128
// from guest memory, lane indices are logically reversed compared to x86 SSE.
// These macros convert between the logical 360 lane index (as used in the
// HIR) and the physical NEON lane index.
//
// For bytes   (16 lanes): logical 0 = physical 15, etc.
// For int16   (8 lanes):  logical 0 = physical 7, etc.
// For int32   (4 lanes):  logical 0 = physical 3, etc.
// For float32 (4 lanes):  logical 0 = physical 3, etc.
//
// These match the VEC128_B/W/D/F macros in x64_op.h.
// ---------------------------------------------------------------------------
#define VEC128_B(n) ((n) ^ 0x3)   // byte lane
#define VEC128_W(n) ((n) ^ 0x1)   // word (int16) lane
#define VEC128_D(n) (n)            // dword (int32/float) lane
#define VEC128_F(n) (n)            // float lane

// ---------------------------------------------------------------------------
// InstrKey — encodes opcode + operand types into a 32-bit dispatch key.
// This is identical to x64_op.h; the key encoding is shared.
// ---------------------------------------------------------------------------
enum KeyType {
  KEY_TYPE_X = OPCODE_SIG_TYPE_X,
  KEY_TYPE_L = OPCODE_SIG_TYPE_L,
  KEY_TYPE_O = OPCODE_SIG_TYPE_O,
  KEY_TYPE_S = OPCODE_SIG_TYPE_S,
  KEY_TYPE_V_I8   = OPCODE_SIG_TYPE_V + INT8_TYPE,
  KEY_TYPE_V_I16  = OPCODE_SIG_TYPE_V + INT16_TYPE,
  KEY_TYPE_V_I32  = OPCODE_SIG_TYPE_V + INT32_TYPE,
  KEY_TYPE_V_I64  = OPCODE_SIG_TYPE_V + INT64_TYPE,
  KEY_TYPE_V_F32  = OPCODE_SIG_TYPE_V + FLOAT32_TYPE,
  KEY_TYPE_V_F64  = OPCODE_SIG_TYPE_V + FLOAT64_TYPE,
  KEY_TYPE_V_V128 = OPCODE_SIG_TYPE_V + VEC128_TYPE,
};

#pragma pack(push, 1)
union InstrKey {
  uint32_t value;
  struct {
    uint32_t opcode : 8;
    uint32_t dest   : 5;
    uint32_t src1   : 5;
    uint32_t src2   : 5;
    uint32_t src3   : 5;
    uint32_t reserved : 4;
  };

  operator uint32_t() const { return value; }

  InstrKey() : value(0) {}
  InstrKey(uint32_t v) : value(v) {}
  InstrKey(const Instr* i) : value(0) {
    opcode = i->opcode->num;
    uint32_t sig = i->opcode->signature;
    dest = GET_OPCODE_SIG_TYPE_DEST(sig)
               ? OPCODE_SIG_TYPE_V + i->dest->type
               : 0;
    src1 = GET_OPCODE_SIG_TYPE_SRC1(sig);
    if (src1 == OPCODE_SIG_TYPE_V) src1 += i->src1.value->type;
    src2 = GET_OPCODE_SIG_TYPE_SRC2(sig);
    if (src2 == OPCODE_SIG_TYPE_V) src2 += i->src2.value->type;
    src3 = GET_OPCODE_SIG_TYPE_SRC3(sig);
    if (src3 == OPCODE_SIG_TYPE_V) src3 += i->src3.value->type;
  }

  template <Opcode OPCODE,
            KeyType DEST = KEY_TYPE_X, KeyType SRC1 = KEY_TYPE_X,
            KeyType SRC2 = KEY_TYPE_X, KeyType SRC3 = KEY_TYPE_X>
  struct Construct {
    static const uint32_t value =
        (OPCODE) | (DEST << 8) | (SRC1 << 13) | (SRC2 << 18) | (SRC3 << 23);
  };
};
#pragma pack(pop)
static_assert(sizeof(InstrKey) <= 4, "InstrKey must be 4 bytes");

// ---------------------------------------------------------------------------
// Op base types — used to describe what each source/dest operand type is
// ---------------------------------------------------------------------------
struct OpBase {};

template <typename T, KeyType KEY_TYPE>
struct Op : OpBase {
  static const KeyType key_type = KEY_TYPE;
};

struct VoidOp : Op<VoidOp, KEY_TYPE_X> {
  void Load(const Instr::Op& op) {}
};

struct OffsetOp : Op<OffsetOp, KEY_TYPE_O> {
  uint64_t value;
  void Load(const Instr::Op& op) { value = op.offset; }
};

struct SymbolOp : Op<SymbolOp, KEY_TYPE_S> {
  Function* value;
  bool Load(const Instr::Op& op) {
    value = op.symbol;
    return true;
  }
};

struct LabelOp : Op<LabelOp, KEY_TYPE_L> {
  hir::Label* value;
  void Load(const Instr::Op& op) { value = op.label; }
};

// ---------------------------------------------------------------------------
// ValueOp — typed operand wrapper for HIR values.
// REG_TYPE is the Arm64Gen register type (ARM64Reg).
// CONST_TYPE is the C++ scalar type for constants.
// ---------------------------------------------------------------------------
// Materializes a constant operand into a reserved fallback register
// (x14/x15 or q20/q21) at the current emission point. Defined in
// arm64_emitter.cc. Used when a sequence with no explicit constant handling
// calls .reg() on a constant operand (~150 sequences only handle registers;
// most constants are folded before reaching the backend, but stragglers
// used to abort the whole run via the assert below, one opcode per run).
ARM64Reg Arm64MaterializeConstFallback(const hir::Value* v);

template <typename T, KeyType KEY_TYPE, typename CONST_TYPE>
struct ValueOp : Op<ValueOp<T, KEY_TYPE, CONST_TYPE>, KEY_TYPE> {
  const Value* value;
  bool is_constant;
  mutable ARM64Reg reg_ = ARM64Reg::INVALID_REG;

  const ARM64Reg& reg() const {
    if (is_constant) {
      // Generic fallback: emit the constant into a reserved register and use
      // that. Logged (rate-limited) in the helper so missing constant
      // handling stays visible without killing the run.
      reg_ = Arm64MaterializeConstFallback(value);
    }
    return reg_;
  }
  operator const ARM64Reg&() const { return reg(); }

  void Load(const Instr::Op& op) {
    value = op.value;
    is_constant = value->IsConstant();
    if (!is_constant) {
      // Resolve the HIR register index to the physical ARM64 register
      // using the appropriate width mapping.
      reg_ = ResolveReg(value);
    }
  }

 private:
  static ARM64Reg ResolveReg(const Value* v);
};

// Specializations for each HIR value type:

struct I8Op : ValueOp<I8Op, KEY_TYPE_V_I8, int8_t> {
  int8_t constant() const { return value->constant.i8; }
};
struct I16Op : ValueOp<I16Op, KEY_TYPE_V_I16, int16_t> {
  int16_t constant() const { return value->constant.i16; }
};
struct I32Op : ValueOp<I32Op, KEY_TYPE_V_I32, int32_t> {
  int32_t constant() const { return value->constant.i32; }
};
struct I64Op : ValueOp<I64Op, KEY_TYPE_V_I64, int64_t> {
  int64_t constant() const { return value->constant.i64; }
};
struct F32Op : ValueOp<F32Op, KEY_TYPE_V_F32, float> {
  float constant() const { return value->constant.f32; }
};
struct F64Op : ValueOp<F64Op, KEY_TYPE_V_F64, double> {
  double constant() const { return value->constant.f64; }
};
struct V128Op : ValueOp<V128Op, KEY_TYPE_V_V128, vec128_t> {
  const vec128_t& constant() const { return value->constant.v128; }
};

// ---------------------------------------------------------------------------
// I<OPCODE, DEST, SRC...> — compile-time instruction descriptor.
//
// Encodes the opcode + operand types into the InstrKey and provides a
// typed EmitArgType that sequence Emit() functions receive.
// ---------------------------------------------------------------------------
template <hir::Opcode OPCODE, typename DEST, typename SRC1 = VoidOp,
          typename SRC2 = VoidOp, typename SRC3 = VoidOp>
struct I {
  static const hir::Opcode opcode = OPCODE;
  typedef DEST dest_type;
  typedef SRC1 src1_type;
  typedef SRC2 src2_type;
  typedef SRC3 src3_type;

  static const uint32_t head_key() {
    return InstrKey::Construct<OPCODE,
                               DEST::key_type, SRC1::key_type,
                               SRC2::key_type, SRC3::key_type>::value;
  }

  // EmitArgType is what each Emit() function receives.
  struct EmitArgType {
    ARM64Emitter& e;
    const hir::Instr* instr;
    DEST dest;
    SRC1 src1;
    SRC2 src2;
    SRC3 src3;
  };
};

// ---------------------------------------------------------------------------
// Sequence<SELF, INSTR_TYPE> — base for all opcode sequence structs.
//
// Usage:
//   struct ADD_I32 : Sequence<ADD_I32, I<OPCODE_ADD, I32Op, I32Op, I32Op>> {
//     static void Emit(ARM64Emitter& e, const EmitArgType& i) { ... }
//   };
// ---------------------------------------------------------------------------
template <typename SELF, typename INSTR_TYPE>
struct Sequence {
  typedef typename INSTR_TYPE::EmitArgType EmitArgType;

  static uint32_t head_key() { return INSTR_TYPE::head_key(); }

  static bool Select(ARM64Emitter& e, const hir::Instr* i) {
    EmitArgType args = {e, i, {}, {}, {}, {}};
    // Load operands from the instruction.
    if constexpr (!std::is_same_v<typename INSTR_TYPE::dest_type, VoidOp>) {
      if (i->dest) args.dest.Load({.value = i->dest});
    }
    if constexpr (!std::is_same_v<typename INSTR_TYPE::src1_type, VoidOp>) {
      args.src1.Load(i->src1);
    }
    if constexpr (!std::is_same_v<typename INSTR_TYPE::src2_type, VoidOp>) {
      args.src2.Load(i->src2);
    }
    if constexpr (!std::is_same_v<typename INSTR_TYPE::src3_type, VoidOp>) {
      args.src3.Load(i->src3);
    }
    SELF::Emit(e, args);
    return true;
  }
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_OP_H_
