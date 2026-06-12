/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64 backend implementation — includes thunk emission.
 *
 * THUNK LAYOUT (HostToGuestThunk):
 *
 *   Prolog  — 7 instructions (saves 6 register pairs, 96 bytes total):
 *     stp x29, x30, [sp, #-96]!   // sp-=96; [sp+0]=fp, [sp+8]=lr
 *     stp x27, x28, [sp, #16]
 *     stp x25, x26, [sp, #32]
 *     stp x23, x24, [sp, #48]
 *     stp x21, x22, [sp, #64]
 *     stp x19, x20, [sp, #80]     // [sp+80]=context, [sp+88]=membase
 *
 *   Setup  — 3 instructions:
 *     mov x19, x1                 // x19 = PPCContext*
 *     ldr x20, [x19, #8]          // x20 = virtual_membase (offset 0x8)
 *     blr x0                      // call guest machine code
 *
 *   Epilog — 7 instructions (mirrors prolog):
 *     ldp x19, x20, [sp, #80]
 *     ldp x21, x22, [sp, #64]
 *     ldp x23, x24, [sp, #48]
 *     ldp x25, x26, [sp, #32]
 *     ldp x27, x28, [sp, #16]
 *     ldp x29, x30, [sp], #96    // sp+=96; restore fp and lr
 *     ret
 *
 * THUNK SIZE: 17 instructions = 68 bytes
 *
 * TROUBLESHOOTING:
 *   If the thunk crashes:
 *   1. Verify STP/LDP offsets are symmetric (prolog saves at +80, epilog
 *      loads from +80 — same slot for x19).
 *   2. Verify LDR X20, [X19, #8] — virtual_membase is at PPCContext+0x8
 *      per ppc_context.h layout.  If the context layout changes this breaks.
 *   3. BLR X0 is the 10th instruction (offset 36 from thunk start).
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_backend.h"

#include <cstring>

#include "xenia/base/logging.h"
#include "xenia/cpu/backend/a64/a64_assembler.h"
#include "xenia/cpu/backend/a64/a64_code_cache.h"
#include "xenia/cpu/backend/a64/a64_emitter.h"
#include "xenia/cpu/backend/a64/a64_function.h"
#include "xenia/cpu/processor.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

A64Backend::A64Backend() = default;
A64Backend::~A64Backend() = default;

bool A64Backend::Initialize(Processor* processor) {
  if (!Backend::Initialize(processor)) {
    return false;
  }

  a64_code_cache_ = std::make_unique<A64CodeCache>();
  if (!a64_code_cache_->Initialize()) {
    XELOGE("A64Backend: failed to initialize code cache (mmap failed)");
    return false;
  }
  // Expose the base-class code_cache_ pointer (used by Processor, debugger, etc.)
  code_cache_ = a64_code_cache_.get();

  // Provide dummy register sets so RegisterAllocationPass terminates.
  // The A64Assembler uses a "values live in memory" strategy and ignores
  // reg assignments, but the pass still needs non-zero counts to not loop.
  auto& gprs = machine_info_.register_sets[0];
  gprs.id = 0;
  std::strcpy(gprs.name, "gpr");
  gprs.types = MachineInfo::RegisterSet::INT_TYPES;
  gprs.count = 24;

  auto& fprs = machine_info_.register_sets[1];
  fprs.id = 1;
  std::strcpy(fprs.name, "fpr");
  fprs.types = MachineInfo::RegisterSet::FLOAT_TYPES |
               MachineInfo::RegisterSet::VEC_TYPES;
  fprs.count = 32;

  if (!EmitThunks()) {
    XELOGE("A64Backend: failed to emit thunks");
    return false;
  }

  XELOGI("A64Backend: initialized, thunks at h2g={:#x} g2h={:#x}",
         reinterpret_cast<uintptr_t>(host_to_guest_thunk_),
         reinterpret_cast<uintptr_t>(guest_to_host_thunk_));
  return true;
}

std::unique_ptr<Assembler> A64Backend::CreateAssembler() {
  XELOGI("A64Backend: CreateAssembler called");
  return std::make_unique<A64Assembler>(this);
}

std::unique_ptr<GuestFunction> A64Backend::CreateGuestFunction(
    Module* module, uint32_t address) {
  return std::make_unique<A64Function>(module, address);
}

uint64_t A64Backend::CalculateNextHostInstruction(
    ThreadDebugInfo* /*thread_info*/, uint64_t current_pc) {
  // AArch64 instructions are always 4 bytes, so the next instruction is +4.
  return current_pc + 4;
}

bool A64Backend::EmitThunks() {
  A64Emitter e;

  // ===========================================================================
  // HostToGuestThunk
  //   Signature (AAPCS64): void* thunk(void* target, void* context, void* ret)
  //     x0 = target       (guest machine code pointer)
  //     x1 = context      (PPCContext*)
  //     x2 = return_addr  (uint32 guest return address, widened to pointer)
  // ===========================================================================
  size_t h2g_start = e.Size();

  // --- Prolog: save all callee-saved registers (6 pairs = 96 bytes) ---
  e.Emit(STP64_PRE(X29, X30, SP, -96));  // sp-=96; saves frame-ptr and LR
  e.Emit(STP64_OFF(X27, X28, SP, 16));
  e.Emit(STP64_OFF(X25, X26, SP, 32));
  e.Emit(STP64_OFF(X23, X24, SP, 48));
  e.Emit(STP64_OFF(X21, X22, SP, 64));
  e.Emit(STP64_OFF(X19, X20, SP, 80));   // context / membase saved here

  // --- Setup JIT registers ---
  e.Emit(MOV64rr(X19, X1));             // x19 = PPCContext* (context arg)
  // virtual_membase is the second field of PPCContext at offset 8 (see
  // ppc_context.h: thread_state* at 0x0, virtual_membase* at 0x8).
  e.Emit(LDR64(X20, X19, 8));           // x20 = virtual_membase

  // --- Call guest function ---
  e.Emit(BLR(X0));                       // call target; x30 updated to next instr

  // --- Epilog: restore in reverse order ---
  e.Emit(LDP64_OFF(X19, X20, SP, 80));
  e.Emit(LDP64_OFF(X21, X22, SP, 64));
  e.Emit(LDP64_OFF(X23, X24, SP, 48));
  e.Emit(LDP64_OFF(X25, X26, SP, 32));
  e.Emit(LDP64_OFF(X27, X28, SP, 16));
  e.Emit(LDP64_POST(X29, X30, SP, 96)); // sp+=96; restores frame-ptr and LR
  e.Emit(RET_instr());

  size_t h2g_size = e.Size() - h2g_start;

  // ===========================================================================
  // GuestToHostThunk
  //   Signature: void* thunk(void* cpp_function, void* arg0, void* arg1)
  //     x0 = C++ function pointer to call
  //     x1 = arg0 (PPCContext* — already in x19, but passed explicitly)
  //     x2 = arg1
  //   Used by JIT code to call back into C++ (e.g., CallExtern, exceptions).
  //   Saves caller-saved (volatile) registers so guest code state survives.
  // ===========================================================================
  size_t g2h_start = e.Size();

  // Standard callee-save: just preserve frame and LR
  e.Emit(STP64_PRE(X29, X30, SP, -16));
  e.Emit(ADD64ri(X29, SP, 0));   // fp = sp

  // Rearrange args for the C++ call:
  //   C++ ABI: fn(PPCContext*, arg0, arg1) → x0=ctx, x1=arg0, x2=arg1
  //   Incoming: x0=fn_ptr, x1=arg0, x2=arg1
  // Save function pointer, then reorganize
  e.Emit(MOV64rr(X9, X0));          // x9 = function pointer (scratch)
  e.Emit(MOV64rr(X0, X19));         // x0 = PPCContext* (context)
  // x1 and x2 are already arg0 and arg1
  e.Emit(BLR(X9));                   // call C++ function

  e.Emit(LDP64_POST(X29, X30, SP, 16));
  e.Emit(RET_instr());

  size_t g2h_size = e.Size() - g2h_start;

  // --- Write both thunks to code cache ---
  size_t total_size = e.Size();
  uint8_t* write_ptr = nullptr;
  uint8_t* exec_ptr  = a64_code_cache_->Alloc(total_size, &write_ptr);
  if (!exec_ptr) {
    XELOGE("A64Backend: code cache full during thunk emission");
    return false;
  }

  // Copy instruction words into the writable view
  std::memcpy(write_ptr, e.Data(), total_size);

  // Flush I-cache so the CPU sees the new thunk code
  a64_code_cache_->FlushInstrCache(write_ptr, total_size);

  host_to_guest_thunk_ = reinterpret_cast<HostToGuestThunk>(exec_ptr + h2g_start);
  guest_to_host_thunk_ = reinterpret_cast<GuestToHostThunk>(exec_ptr + g2h_start);

  (void)h2g_size;
  (void)g2h_size;
  return true;
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
