/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64 JIT compiled function — wrapper around a block of machine code.
 *
 * DESIGN:
 *   A64Function holds a pointer to machine code in the A64CodeCache's exec
 *   view and its size.  CallImpl invokes that code via the HostToGuestThunk,
 *   which sets up x19=context and x20=membase before calling the code.
 *
 * TROUBLESHOOTING:
 *   - If CallImpl crashes immediately, the thunk encoding is wrong.  Check
 *     that HostToGuestThunk saves x19-x28, sets x19=arg1 (context), sets
 *     x20=[x19+8] (virtual_membase), and jumps via BLR.
 *   - If machine_code() returns nullptr the function was never assembled
 *     (A64Assembler::Assemble returned false, check the log).
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_A64_A64_FUNCTION_H_
#define XENIA_CPU_BACKEND_A64_A64_FUNCTION_H_

#include "xenia/cpu/function.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

class A64Function : public GuestFunction {
 public:
  A64Function(Module* module, uint32_t address);
  ~A64Function() override;

  uint8_t* machine_code() const override { return machine_code_; }
  size_t machine_code_length() const override { return machine_code_length_; }

  // Called by A64Assembler::Assemble after successful code generation.
  void Setup(uint8_t* machine_code, size_t machine_code_length);

 protected:
  // Invoked by GuestFunction::Call; transitions from C++ into JIT code.
  bool CallImpl(ThreadState* thread_state, uint32_t return_address) override;

 private:
  uint8_t* machine_code_      = nullptr;
  size_t   machine_code_length_ = 0;
};

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_BACKEND_A64_A64_FUNCTION_H_
