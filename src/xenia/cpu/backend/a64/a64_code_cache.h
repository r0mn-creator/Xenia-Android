/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64 JIT code cache — manages executable memory for AArch64 generated code.
 *
 * DESIGN:
 *   On Android 7–9  PROT_READ|PROT_WRITE|PROT_EXEC is allowed on a single
 *   anonymous mapping.  On Android 10+ the kernel enforces W^X (write-xor-
 *   execute), so we fall back to a dual-mapping via memfd_create:
 *     write_view_ — writable, not executable (code is written here)
 *     exec_view_  — executable, not writable  (code runs from here)
 *   Both views are backed by the same physical pages (shared fd).
 *
 *   The allocator is a simple bump-pointer inside a single pre-reserved
 *   region.  Allocations are 64-byte aligned (cache-line aligned).
 *
 * TROUBLESHOOTING:
 *   - SIGSEGV in generated code with fault_address near 0:
 *       The code jumped to exec_view_ but the instructions were written
 *       to write_view_ (or the wrong offset).  Check Alloc() returns the
 *       exec pointer from exec_view_, not write_view_.
 *   - SIGBUS / SIGILL right after a function call:
 *       __builtin___clear_cache was not called before executing the new code.
 *   - mmap fails (returns MAP_FAILED):
 *       memfd_create might require API level 30; fall back to tmpfile or
 *       check SELinux policy.  On Android, the process needs the
 *       "execmem" SELinux permission to use RWX mappings.
 *   - LookupFunction returns nullptr for a valid host PC:
 *       The function was not registered via RegisterFunction, or the PC
 *       is in the prolog/epilog area outside the recorded [start,end) range.
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_A64_A64_CODE_CACHE_H_
#define XENIA_CPU_BACKEND_A64_A64_CODE_CACHE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <vector>

#include "xenia/cpu/backend/code_cache.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

class A64CodeCache : public CodeCache {
 public:
  // Total reserved virtual address space for JIT code.
  // 64 MB is generous; most sessions use far less.
  static constexpr size_t kCacheSize = 64u * 1024u * 1024u;

  // Alignment for every allocation (cache-line size).
  static constexpr size_t kAllocAlign = 64u;

  A64CodeCache();
  ~A64CodeCache() override;

  // Initialize must be called before any allocations.
  // Returns false if mmap fails (unlikely unless OOM or SELinux blocks it).
  bool Initialize();

  // Allocate `size` bytes of JIT memory.
  // Returns the EXECUTABLE address (exec_view_) for the caller to store as
  // the function's machine_code pointer.
  // Also sets *write_ptr to the WRITABLE alias — caller writes generated
  // bytes there, then calls FlushInstrCache() before executing.
  // Returns nullptr if the cache is exhausted.
  uint8_t* Alloc(size_t size, uint8_t** write_ptr_out);

  // Flush the instruction cache for the given range in the WRITE view
  // so the CPU sees the newly written bytes in the EXEC view.
  // Must be called between writing code and executing it.
  void FlushInstrCache(uint8_t* write_start, size_t size);

  // Register a function so LookupFunction can find it by host PC.
  void RegisterFunction(GuestFunction* fn,
                        uint8_t* exec_code,
                        size_t code_size);

  // ----- CodeCache interface -----------------------------------------------

  const std::filesystem::path& file_name() const override {
    return file_name_;
  }
  uintptr_t execute_base_address() const override {
    return reinterpret_cast<uintptr_t>(exec_view_);
  }
  size_t total_size() const override { return kCacheSize; }

  // Binary-search for the function whose code range contains host_pc.
  GuestFunction* LookupFunction(uint64_t host_pc) override;

  // No unwind info for now (no unwinder integration).
  void* LookupUnwindInfo(uint64_t host_pc) override { return nullptr; }

 private:
  uint8_t* exec_view_  = nullptr;   // executable, read-only mapping
  uint8_t* write_view_ = nullptr;   // writable, non-executable mapping
  bool dual_mapped_ = false;        // true when exec != write (W^X mode)

  std::atomic<size_t> offset_{0};   // bump-pointer offset in bytes

  struct Entry {
    GuestFunction* fn;
    uintptr_t start;  // exec address of first byte
    uintptr_t end;    // exec address one past last byte
  };

  std::mutex entries_mutex_;
  std::vector<Entry> entries_;      // sorted by start for binary search

  std::filesystem::path file_name_;
};

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_BACKEND_A64_A64_CODE_CACHE_H_
