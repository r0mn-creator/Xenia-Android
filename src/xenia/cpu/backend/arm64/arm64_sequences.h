/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_SEQUENCES_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_SEQUENCES_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include "xenia/cpu/hir/instr.h"

#include <unordered_map>

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

class ARM64Emitter;

// Each sequence handler is a function: given an emitter and an instruction,
// emit the ARM64 code for it and return true, or return false if this handler
// doesn't match (allowing fallthrough to the next registered handler for the
// same opcode key).
typedef bool (*SequenceSelectFn)(ARM64Emitter&, const hir::Instr*);

// Global dispatch table: InstrKey -> handler. Populated at startup by the
// EMITTER_OPCODE_TABLE macros in arm64_sequences.cc.
//
// Implemented as a function-local static (Meyers singleton) rather than a
// namespace-scope global: the EMITTER_OPCODE_TABLE registrations run as static
// initializers spread across three translation units, and a namespace-scope
// global could be constructed AFTER some of those initializers ran, wiping
// their inserts (static initialization order fiasco). A function-local static
// is constructed on first use — i.e. on the first Register() call — so every
// registration sees a live table.
std::unordered_map<uint32_t, SequenceSelectFn>& sequence_table();

// Variadic registration helper — registers ALL of the given sequence types in
// the dispatch table in one call. Implemented as a C++17 fold expression so
// that every type variant of an opcode (e.g. ADD_I8 .. ADD_V128) is inserted;
// an earlier recursive two-overload version only registered a single variant
// per opcode, leaving most type-specific keys unmapped.
//
// Note: insert() does not overwrite, so the first registration for a given key
// wins. Keys are unique per (opcode, operand-type) tuple, so this is fine.
template <typename... Ts>
bool Register() {
  (sequence_table().insert({Ts::head_key(), Ts::Select}), ...);
  return true;
}

// Macro used at the bottom of each opcode block in arm64_sequences.cc.
// Example:
//   EMITTER_OPCODE_TABLE(OPCODE_ADD, ADD_I8, ADD_I16, ADD_I32, ADD_I64, ...)
#define EMITTER_OPCODE_TABLE(name, ...) \
  const auto ARM64_INSTR_##name = Register<__VA_ARGS__>();

// Called by the assembler for each HIR instruction. Looks up the handler
// in the dispatch table and calls it. Returns false if no handler found.
bool SelectSequence(ARM64Emitter* e, const hir::Instr* i,
                    const hir::Instr** new_tail);

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_SEQUENCES_H_
