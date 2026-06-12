/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64 backend — top-level CPU backend for AArch64 / Android arm64-v8a.
 *
 * THUNK CALLING CONVENTIONS:
 *   HostToGuestThunk(void* target, void* context, void* return_address):
 *     x0 = machine_code pointer (guest function to run)
 *     x1 = PPCContext* (thread_state->context())
 *     x2 = return address as a pointer (uintptr_t(return_addr))
 *   The thunk saves all AArch64 callee-saved registers (x19-x28, x29-x30),
 *   sets x19=context and x20=virtual_membase from context, then BLR x0.
 *
 *   GuestToHostThunk(void* function, void* arg0, void* arg1):
 *     x0 = C++ function pointer to call
 *     x1 = arg0 (context)
 *     x2 = arg1
 *   Used by generated code to call back into C++ (extern calls, etc.).
 *
 * REGISTER INVARIANTS (maintained inside JIT-generated code):
 *   x19 = PPCContext*  (set by thunk, never changed inside guest code)
 *   x20 = virtual_membase (set from x19->virtual_membase in thunk)
 *   x25 = HIR value slot base (set in each function's prolog)
 *   x29 = frame pointer
 *   x30 = link register
 *
 * TROUBLESHOOTING:
 *   - "thunk is null" assert: Initialize() was not called, or the code
 *     cache ran out of space during thunk emission.
 *   - Crash inside thunk: verify the STP save area is exactly 96 bytes
 *     (6 pairs × 16 bytes), the prolog and epilog are symmetric.
 *   - If LookupFunction fails for a guest PC: check CommitExecutableRange
 *     is being called (the x64 path uses it for indirection tables; the
 *     A64 path currently ignores it since we don't have an indirection table).
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_A64_A64_BACKEND_H_
#define XENIA_CPU_BACKEND_A64_A64_BACKEND_H_

#include <memory>

#include "xenia/cpu/backend/backend.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

class A64CodeCache;

// Thunk function types (mirror x64 backend for compatibility).
typedef void* (*HostToGuestThunk)(void* target, void* arg0, void* arg1);
typedef void* (*GuestToHostThunk)(void* target, void* arg0, void* arg1);

class A64Backend : public Backend {
 public:
  A64Backend();
  ~A64Backend() override;

  A64CodeCache* a64_code_cache() const { return a64_code_cache_.get(); }

  HostToGuestThunk host_to_guest_thunk() const { return host_to_guest_thunk_; }
  GuestToHostThunk guest_to_host_thunk() const { return guest_to_host_thunk_; }

  // Backend interface
  bool Initialize(Processor* processor) override;

  // No indirection table needed in the A64 backend (we call through
  // function pointers directly).  This is a no-op.
  void CommitExecutableRange(uint32_t guest_low, uint32_t guest_high) override {}

  std::unique_ptr<Assembler> CreateAssembler() override;

  std::unique_ptr<GuestFunction> CreateGuestFunction(Module* module,
                                                     uint32_t address) override;

  // Walk the code cache to find the next host PC after current_pc.
  // For now we return current_pc+4 (one instruction ahead) — good enough
  // for the debugger to single-step through JIT code.
  uint64_t CalculateNextHostInstruction(ThreadDebugInfo* thread_info,
                                        uint64_t current_pc) override;

 private:
  std::unique_ptr<A64CodeCache> a64_code_cache_;
  HostToGuestThunk host_to_guest_thunk_ = nullptr;
  GuestToHostThunk guest_to_host_thunk_ = nullptr;

  // Emit the host-to-guest and guest-to-host thunks into the code cache.
  bool EmitThunks();
};

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_BACKEND_A64_A64_BACKEND_H_
