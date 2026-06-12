/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_CODE_CACHE_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_CODE_CACHE_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "xenia/base/memory.h"
#include "xenia/base/mutex.h"
#include "xenia/cpu/backend/code_cache.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

// Mirrors EmitFunctionInfo from x64_code_cache.h.
// Tracks the size breakdown of each emitted function's prolog/body/epilog.
struct EmitFunctionInfo {
  struct _code_size {
    size_t prolog;
    size_t body;
    size_t epilog;
    size_t total;
  } code_size;
  size_t stack_size;
};

class ARM64CodeCache : public CodeCache {
 public:
  ~ARM64CodeCache() override;

  static std::unique_ptr<ARM64CodeCache> Create();

  virtual bool Initialize();

  const std::filesystem::path& file_name() const override { return file_name_; }
  uintptr_t execute_base_address() const override {
    return kGeneratedCodeExecuteBase;
  }
  size_t total_size() const override { return kGeneratedCodeSize; }

  // Indirection table management (same as x64 — maps guest PC -> host PC).
  bool has_indirection_table() { return indirection_table_base_ != nullptr; }
  void set_indirection_default(uint32_t default_value);
  void AddIndirection(uint32_t guest_address, uint32_t host_offset);

  void CommitExecutableRange(uint32_t guest_low, uint32_t guest_high);

  // Place machine code from the emitter into the cache.
  // Returns the final execute and write addresses.
  void PlaceHostCode(uint32_t guest_address, void* machine_code,
                     const EmitFunctionInfo& func_info,
                     void*& code_execute_address_out,
                     void*& code_write_address_out);

  void PlaceGuestCode(uint32_t guest_address, void* machine_code,
                      const EmitFunctionInfo& func_info,
                      GuestFunction* function_info,
                      void*& code_execute_address_out,
                      void*& code_write_address_out);

  uint32_t PlaceData(const void* data, size_t length);

  GuestFunction* LookupFunction(uint64_t host_pc) override;
  void* LookupUnwindInfo(uint64_t host_pc) override { return nullptr; }

  // ---------------------------------------------------------------------------
  // Memory layout — same strategy as x64 backend.
  //
  // Guest PPC code lives in 0x00000000–0x1FFFFFFF (512 MB).
  // We map the indirection table at a fixed host address so the JIT can emit
  // absolute-addressed indirection lookups using a single LDR instruction
  // with a 32-bit offset from a base register.
  //
  // On AArch64 we don't have the same 2GB constraint that x64 has (x64 uses
  // RIP-relative addressing limited to ±2GB). However, we still want the
  // code region to be compact so that B/BL instructions (±128MB) can reach
  // helpers and thunks without needing veneers.
  // ---------------------------------------------------------------------------
  static const uint64_t kIndirectionTableBase    = 0x0000000090000000ull;
  static const uint64_t kIndirectionTableSize    = 0x0000000010000000ull;  // 256 MB
  static const uint64_t kGeneratedCodeExecuteBase = 0x00000000A0000000ull;
  static const uint64_t kGeneratedCodeSize        = 0x0000000040000000ull;  // 1 GB

  static const size_t kMaximumFunctionCount = 100000;

 protected:
  ARM64CodeCache();

  std::filesystem::path file_name_;
  xe::memory::FileMappingHandle mapping_ =
      xe::memory::kFileMappingHandleInvalid;

  xe::global_critical_region global_critical_region_;

  uint32_t indirection_default_value_ = 0xFEEDF00D;

  uint8_t* indirection_table_base_      = nullptr;
  uint8_t* generated_code_execute_base_ = nullptr;
  uint8_t* generated_code_write_base_   = nullptr;
  size_t generated_code_offset_         = 0;
  std::atomic<size_t> generated_code_commit_mark_ = {0};

  // Sorted by [host_start | host_end] for binary search by host PC.
  std::vector<std::pair<uint64_t, GuestFunction*>> generated_code_map_;
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_CODE_CACHE_H_
