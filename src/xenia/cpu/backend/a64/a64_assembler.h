/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64Assembler — lowers HIR (High-level IR) to AArch64 machine code.
 *
 * DESIGN OVERVIEW:
 *   The Assembler is called once per guest function via Assemble(ir_builder).
 *   It iterates the HIR blocks in order, emitting AArch64 instructions using
 *   A64Emitter, then applies branch fixups (forward-branch patch-ups), writes
 *   the result to the A64CodeCache, and calls A64Function::Setup().
 *
 * FUNCTION FRAME LAYOUT:
 *   Each JIT function has:
 *     [sp+0  .. sp+8 ] : saved x29 (frame pointer)
 *     [sp+8  .. sp+16] : saved x30 (link register)
 *     [sp+16 .. sp+24] : saved x25 (HIR slot base — see below)
 *     [sp+24 .. sp+32] : saved x26 (spare)
 *     [sp+32 .. ..   ] : HIR value slots (num_values × 8 bytes)
 *
 *   STP prolog lays this down as:
 *     STP X29, X30, [SP, #-(32 + slots_bytes)]!
 *     STP X25, X26, [SP, #16]
 *     ADD X25, SP, #32             // X25 = slot base
 *
 *   Epilog mirrors it:
 *     LDP X25, X26, [SP, #16]
 *     LDP X29, X30, [SP], #(32 + slots_bytes)
 *     RET
 *
 * HIR VALUE SLOT INDEXING:
 *   Each HIR Value has an ordinal (0..num_values-1).  Its slot is at
 *   [X25 + ordinal*8].  X25 never changes within a function, so every
 *   slot access is a simple offset load/store.
 *
 * REGISTER ALLOCATION:
 *   The assembler uses a trivial "always-through-memory" strategy:
 *   All HIR values live in their slots.  Before each instruction that reads
 *   a value, it is loaded into a scratch register (X0..X7).  After each
 *   instruction that writes a value, the result is stored to the dest slot.
 *   This is slow but correct and easy to debug.
 *   Scratch register assignments per instruction:
 *     X0 = dest (written, then stored)
 *     X1 = src1 (loaded before use)
 *     X2 = src2 (loaded before use)
 *
 * BRANCH FIXUP:
 *   Forward branches emit a placeholder 0-offset B/CBNZ/CBZ.  After all
 *   blocks are emitted the A64Emitter's ApplyFixups() patches each
 *   placeholder with the real relative offset to the target block.
 *
 * TROUBLESHOOTING:
 *   - Crash at instruction start (BRK 0xDEAD): the HIR opcode is unimplemented.
 *     Add a handler in a64_assembler.cc's LowerInstruction switch-case.
 *   - Wrong branch target: check block_offsets_ is populated before fixups
 *     and that the fixup (emitter_offset, block_ordinal, kind) tuple is correct.
 *   - Stack alignment fault: SP must be 16-byte aligned before any BLR/BL.
 *     frame_size must be a multiple of 16 (it is rounded up in BuildProlog).
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_A64_A64_ASSEMBLER_H_
#define XENIA_CPU_BACKEND_A64_A64_ASSEMBLER_H_

#include <memory>
#include <vector>

#include "xenia/cpu/backend/assembler.h"
#include "xenia/cpu/backend/a64/a64_emitter.h"

namespace xe {
namespace cpu {
namespace hir {
class Instr;
class Value;
}  // namespace hir
namespace backend {
namespace a64 {

class A64Backend;

class A64Assembler : public Assembler {
 public:
  explicit A64Assembler(A64Backend* backend);
  ~A64Assembler() override;

  // Initialize the assembler (no-op for now; reserved for future state).
  bool Initialize() override;

  // Reset internal state so the assembler can be reused for another function.
  void Reset() override;

  // Lower the HIR in `builder` to AArch64 machine code and commit it to the
  // code cache.  Sets up the function's machine_code pointer on success.
  // Returns false if code generation fails; logs the reason.
  bool Assemble(GuestFunction* function, hir::HIRBuilder* builder,
                uint32_t debug_info_flags,
                std::unique_ptr<FunctionDebugInfo> debug_info) override;

 private:
  A64Backend* a64_backend_;
  A64Emitter emitter_;

  // Maps HIR block ordinal → byte offset in emitter_ output.
  std::vector<size_t> block_offsets_;

  // Set by BuildProlog; used by inline RETURN epilog emission.
  size_t last_frame_size_ = 0;

  // Emit the function prolog (save frame, establish X25 slot base).
  // Returns the total frame size (including slot area, 16-byte aligned).
  size_t BuildProlog(size_t num_slots);

  // Emit the function epilog (restore frame, RET).
  void BuildEpilog(size_t frame_size);

  // Lower a single HIR instruction into one or more A64 instructions.
  void LowerInstruction(const hir::Instr* instr);

  // Load HIR value `v` into AArch64 register `reg` (X0..X7).
  // Constants are materialized with MOVZ/MOVK; non-constants are loaded
  // from the slot at [X25 + ordinal*8].
  void LoadValue(const hir::Value* v, uint32_t reg);

  // Store the content of AArch64 register `reg` into the HIR value slot
  // for `v` at [X25 + ordinal*8].
  void StoreValue(const hir::Value* v, uint32_t reg);

  // Return the byte offset for the given HIR value ordinal.
  static constexpr size_t SlotOffset(int ordinal) {
    return static_cast<size_t>(ordinal) * 8;
  }
};

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_BACKEND_A64_A64_ASSEMBLER_H_
