/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_ASSEMBLER_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_ASSEMBLER_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <memory>
#include <vector>

#include "xenia/base/string_buffer.h"
#include "xenia/cpu/backend/assembler.h"
#include "xenia/cpu/function.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

class ARM64Backend;
class ARM64Emitter;

// Drives the compilation of a single guest function:
//   HIRBuilder → ARM64Emitter → code cache
//
// One assembler is created per compiler thread. The emitter is reused across
// multiple Assemble() calls (it manages its own buffer reset internally).
class ARM64Assembler : public Assembler {
 public:
  explicit ARM64Assembler(ARM64Backend* backend);
  ~ARM64Assembler() override;

  bool Initialize() override;
  void Reset() override;

  // Core compilation entry point: lowers HIR to ARM64 machine code and
  // installs the result into the code cache.
  bool Assemble(GuestFunction* function, hir::HIRBuilder* builder,
                uint32_t debug_info_flags,
                std::unique_ptr<FunctionDebugInfo> debug_info) override;

 private:
  // Disassembles the emitted machine code to a string for debug output.
  // Uses the capstone disassembler with CS_ARCH_ARM64.
  void DumpMachineCode(void* machine_code, size_t code_size,
                       const std::vector<SourceMapEntry>& source_map,
                       StringBuffer* str);

  ARM64Backend* arm64_backend_;
  std::unique_ptr<ARM64Emitter> emitter_;

  // capstone handle for ARM64 disassembly (debug builds only).
  uintptr_t capstone_handle_ = 0;

  StringBuffer string_buffer_;
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_ASSEMBLER_H_
