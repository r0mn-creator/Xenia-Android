/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64Function — invokes JIT-compiled AArch64 machine code via the thunk.
 *
 * CALL FLOW:
 *   GuestFunction::Call(thread_state, return_address)
 *     → A64Function::CallImpl(thread_state, return_address)
 *       → A64Backend::host_to_guest_thunk()(machine_code_, ctx, ret)
 *           ↳ HostToGuestThunk saves all callee-saved regs, sets up x19/x20,
 *              BLR to machine_code_, then restores and RET
 *
 * TROUBLESHOOTING:
 *   - "machine_code_ is null" → A64Assembler::Assemble was never called for
 *     this function, or it returned false.  Check the assembler log output.
 *   - Crash inside HostToGuestThunk → see a64_backend.cc comment block.
 *   - Return-address mismatch: return_address is a 32-bit GUEST PPC address.
 *     It is widened to a uintptr_t for thunk parameter passing but the JIT
 *     code writes it back as a 32-bit field into the context's LR slot (0x10).
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_function.h"

#include "xenia/base/assert.h"
#include "xenia/cpu/backend/a64/a64_backend.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

A64Function::A64Function(Module* module, uint32_t address)
    : GuestFunction(module, address) {}

A64Function::~A64Function() = default;

void A64Function::Setup(uint8_t* machine_code, size_t machine_code_length) {
  machine_code_        = machine_code;
  machine_code_length_ = machine_code_length;
}

bool A64Function::CallImpl(ThreadState* thread_state,
                           uint32_t return_address) {
  assert_not_null(machine_code_);

  // Retrieve the backend and thunk.  The processor owns the backend; if this
  // assert fires the function outlived its backend which is a lifetime bug.
  auto* backend = reinterpret_cast<A64Backend*>(
      thread_state->processor()->backend());
  auto thunk = backend->host_to_guest_thunk();
  assert_not_null(thunk);

  // Invoke the thunk:
  //   arg0 (x0) = machine_code_       — JIT function to execute
  //   arg1 (x1) = thread context      — PPCContext*; thunk sets x19 = this
  //   arg2 (x2) = return_address      — guest return addr (passed as pointer)
  thunk(machine_code_,
        thread_state->context(),
        reinterpret_cast<void*>(static_cast<uintptr_t>(return_address)));
  return true;
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
