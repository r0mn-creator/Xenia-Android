/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_function.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include "xenia/base/logging.h"
#include "xenia/cpu/backend/arm64/arm64_backend.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

ARM64Function::ARM64Function(Module* module, uint32_t address)
    : GuestFunction(module, address) {}

ARM64Function::~ARM64Function() {
  // The machine code lives in the code cache and is freed with it.
  // We don't own this memory.
  machine_code_ = nullptr;
}

void ARM64Function::Setup(uint8_t* machine_code, size_t machine_code_length) {
  machine_code_ = machine_code;
  machine_code_length_ = machine_code_length;
}

bool ARM64Function::CallImpl(ThreadState* thread_state,
                              uint32_t return_address) {
  // Retrieve the HostToGuestThunk from the backend.
  // This thunk saves all callee-saved host registers, sets up x19/x20
  // (context/membase), then branches to the compiled guest code.
  auto backend =
      reinterpret_cast<ARM64Backend*>(thread_state->processor()->backend());
  auto thunk = backend->host_to_guest_thunk();

  // Call signature: thunk(machine_code, context, return_address)
  //   x0 = pointer to compiled guest function (machine_code_)
  //   x1 = PPCContext* (becomes x19, the guest context register)
  //        PPCContext has thread_state at +0x0 and virtual_membase at +0x8,
  //        which the thunk and the resolver rely on.
  //   x2 = return_address (32-bit PPC return address, zero-extended)
  thunk(machine_code_,
        reinterpret_cast<void*>(thread_state->context()),
        reinterpret_cast<void*>(static_cast<uint64_t>(return_address)));

  return true;
}

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
