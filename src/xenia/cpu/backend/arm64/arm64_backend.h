/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_BACKEND_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_BACKEND_H_

#include "xenia/base/platform.h"

// This backend is only compiled on AArch64 targets (Android, Apple Silicon,
// Linux ARM64). On x86-64, the x64 backend is used instead.
#if XE_ARCH_ARM64

#include <memory>

#include "xenia/base/cvar.h"
#include "xenia/cpu/backend/backend.h"

namespace xe {
class Exception;
}  // namespace xe

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

class ARM64CodeCache;
class ARM64Emitter;

// ---------------------------------------------------------------------------
// Calling convention thunk types
//
// These mirror the x64 backend thunks exactly — the core Processor and
// kernel code calls them via function pointers without knowing the arch.
//
//   HostToGuestThunk  — host C++ → compiled guest code
//   GuestToHostThunk  — guest JIT'd code → host C++ helper
//   ResolveFunctionThunk — lazy resolution of not-yet-compiled functions
// ---------------------------------------------------------------------------
typedef void* (*HostToGuestThunk)(void* target, void* arg0, void* arg1);
typedef void* (*GuestToHostThunk)(void* target, void* arg0, void* arg1);
typedef void (*ResolveFunctionThunk)();

class ARM64Backend : public Backend {
 public:
  // Guest address used as a sentinel "force return" value, same as x64.
  static const uint32_t kForceReturnAddress = 0x9FFF0000u;

  explicit ARM64Backend();
  ~ARM64Backend() override;

  ARM64CodeCache* code_cache() const { return code_cache_.get(); }

  // Thunks used by Processor to cross the host/guest boundary.
  HostToGuestThunk host_to_guest_thunk() const { return host_to_guest_thunk_; }
  GuestToHostThunk guest_to_host_thunk() const { return guest_to_host_thunk_; }
  ResolveFunctionThunk resolve_function_thunk() const {
    return resolve_function_thunk_;
  }

  // ----- Backend interface --------------------------------------------------
  bool Initialize(Processor* processor) override;

  void CommitExecutableRange(uint32_t guest_low,
                             uint32_t guest_high) override;

  std::unique_ptr<Assembler> CreateAssembler() override;

  std::unique_ptr<GuestFunction> CreateGuestFunction(
      Module* module, uint32_t address) override;

  uint64_t CalculateNextHostInstruction(ThreadDebugInfo* thread_info,
                                        uint64_t current_pc) override;

  void InstallBreakpoint(Breakpoint* breakpoint) override;
  void InstallBreakpoint(Breakpoint* breakpoint, Function* fn) override;
  void UninstallBreakpoint(Breakpoint* breakpoint) override;

 private:
  // Emits the three cross-boundary thunks into the code cache at init time.
  bool EmitThunks();

  std::unique_ptr<ARM64CodeCache> code_cache_;

  // The thunks are emitted into this emitter's executable code buffer and
  // called directly (they are not copied into the code cache). It must
  // therefore outlive the backend — otherwise its buffer is freed and the
  // thunk pointers dangle into zeroed memory.
  std::unique_ptr<ARM64Emitter> thunk_emitter_;

  HostToGuestThunk host_to_guest_thunk_ = nullptr;
  GuestToHostThunk guest_to_host_thunk_ = nullptr;
  ResolveFunctionThunk resolve_function_thunk_ = nullptr;
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_BACKEND_H_
