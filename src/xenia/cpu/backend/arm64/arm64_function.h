/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_FUNCTION_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_FUNCTION_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include "xenia/cpu/function.h"
#include "xenia/cpu/thread_state.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

// Represents a single compiled guest function — holds a pointer into the
// code cache and its size. Mirrors X64Function exactly.
class ARM64Function : public GuestFunction {
 public:
  ARM64Function(Module* module, uint32_t address);
  ~ARM64Function() override;

  uint8_t* machine_code() const override { return machine_code_; }
  size_t machine_code_length() const override { return machine_code_length_; }

  // Called by the assembler after code is placed in the cache.
  void Setup(uint8_t* machine_code, size_t machine_code_length);

 protected:
  // Invokes the compiled machine code via the backend's HostToGuestThunk.
  bool CallImpl(ThreadState* thread_state, uint32_t return_address) override;

 private:
  uint8_t* machine_code_ = nullptr;
  size_t machine_code_length_ = 0;
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_FUNCTION_H_
