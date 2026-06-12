/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * arm64_compat.h — Register name aliases for the sequence emission files.
 *
 * The sequence files were written expecting bare names like X0, SP,
 * INDEX_UNSIGNED to be in scope (the old Dolphin pattern). Since ARM64Reg
 * and IndexType are enum class, `using namespace Arm64Gen;` only brings the
 * types into scope, not the values. This header extends the Arm64Gen
 * namespace with inline constexpr aliases so those bare names work.
 *
 * Include this file AFTER arm64_emitter.h and BEFORE any namespace xe block.
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_COMPAT_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_COMPAT_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include "third_party/arm64emitter/Arm64Emitter.h"

namespace Arm64Gen {

// ---------------------------------------------------------------------------
// IndexType aliases — bring old-style INDEX_* names into scope.
// ---------------------------------------------------------------------------
inline constexpr IndexType INDEX_UNSIGNED = IndexType::Unsigned;
inline constexpr IndexType INDEX_PRE      = IndexType::Pre;
inline constexpr IndexType INDEX_POST     = IndexType::Post;

// ---------------------------------------------------------------------------
// ShiftType aliases — bring ST_* names used in ArithOption() calls.
// ---------------------------------------------------------------------------
inline constexpr ShiftType ST_LSL = ShiftType::LSL;
inline constexpr ShiftType ST_LSR = ShiftType::LSR;
inline constexpr ShiftType ST_ASR = ShiftType::ASR;
inline constexpr ShiftType ST_ROR = ShiftType::ROR;

// ---------------------------------------------------------------------------
// PStateField aliases — FIELD_* names used in older code.
// ---------------------------------------------------------------------------
inline constexpr PStateField FIELD_FPCR = PStateField::FPCR;
inline constexpr PStateField FIELD_FPSR = PStateField::FPSR;
inline constexpr PStateField FIELD_NZCV = PStateField::NZCV;

// ---------------------------------------------------------------------------
// ARM64Reg aliases — bring the most-used register names into scope.
// Only the registers actually referenced by bare name in the sequence files.
// ---------------------------------------------------------------------------
inline constexpr ARM64Reg W0  = ARM64Reg::W0;
inline constexpr ARM64Reg W1  = ARM64Reg::W1;
inline constexpr ARM64Reg W2  = ARM64Reg::W2;
inline constexpr ARM64Reg W3  = ARM64Reg::W3;
inline constexpr ARM64Reg W4  = ARM64Reg::W4;
inline constexpr ARM64Reg X0  = ARM64Reg::X0;
inline constexpr ARM64Reg X1  = ARM64Reg::X1;
inline constexpr ARM64Reg X2  = ARM64Reg::X2;
inline constexpr ARM64Reg X3  = ARM64Reg::X3;
inline constexpr ARM64Reg X4  = ARM64Reg::X4;
inline constexpr ARM64Reg SP  = ARM64Reg::SP;
inline constexpr ARM64Reg WSP = ARM64Reg::WSP;

// V0 — generic vector register alias for Q0 (full 128-bit NEON register).
// Used in sequence files as the call-clobbered float argument/result register.
inline constexpr ARM64Reg V0 = ARM64Reg::Q0;

// PRFM prefetch type constants (encoded in the Rt field).
inline constexpr ARM64Reg PLDL1KEEP = static_cast<ARM64Reg>(0);  // data, L1, keep

}  // namespace Arm64Gen

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_COMPAT_H_
