/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * A64 JIT code cache implementation — see a64_code_cache.h for design notes.
 ******************************************************************************
 */

#include "xenia/cpu/backend/a64/a64_code_cache.h"

#include <algorithm>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

// memfd_create syscall (Android 8.0+, Linux 3.17+)
#if defined(__ANDROID__) || defined(__linux__)
#  include <sys/syscall.h>
#  ifndef MFD_CLOEXEC
#    define MFD_CLOEXEC 0x0001U
#  endif
static inline int xe_memfd_create(const char* name) {
  return static_cast<int>(syscall(SYS_memfd_create, name, MFD_CLOEXEC));
}
#endif

#include "xenia/cpu/function.h"

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

A64CodeCache::A64CodeCache() = default;

A64CodeCache::~A64CodeCache() {
  if (exec_view_ && exec_view_ != MAP_FAILED) {
    munmap(exec_view_, kCacheSize);
  }
  if (dual_mapped_ && write_view_ && write_view_ != MAP_FAILED) {
    munmap(write_view_, kCacheSize);
  }
}

bool A64CodeCache::Initialize() {
  // --- Attempt 1: single RWX mapping (works on Android 7-9, old kernels) ---
  exec_view_ = reinterpret_cast<uint8_t*>(
      mmap(nullptr, kCacheSize,
           PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  if (exec_view_ != MAP_FAILED) {
    // Single mapping — write_view aliases exec_view exactly.
    write_view_  = exec_view_;
    dual_mapped_ = false;
    return true;
  }
  exec_view_ = nullptr;

  // --- Attempt 2: dual mapping via memfd (Android 10+ W^X enforcement) ---
  // Create a shared memory object, then map it twice: once RW, once RX.
  // Both mappings share the same physical pages, so a write to write_view_
  // is immediately visible through exec_view_.
#if defined(__ANDROID__) || defined(__linux__)
  int fd = xe_memfd_create("xenia_a64_jit");
  if (fd >= 0) {
    if (ftruncate(fd, static_cast<off_t>(kCacheSize)) == 0) {
      write_view_ = reinterpret_cast<uint8_t*>(
          mmap(nullptr, kCacheSize, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0));
      exec_view_ = reinterpret_cast<uint8_t*>(
          mmap(nullptr, kCacheSize, PROT_READ | PROT_EXEC,
               MAP_SHARED, fd, 0));
    }
    close(fd);  // fd no longer needed once both mappings exist
  }
#endif

  if (exec_view_ != nullptr && exec_view_ != MAP_FAILED &&
      write_view_ != nullptr && write_view_ != MAP_FAILED) {
    dual_mapped_ = true;
    return true;
  }

  // Clean up partial failures
  if (exec_view_ && exec_view_ != MAP_FAILED)  munmap(exec_view_,  kCacheSize);
  if (write_view_ && write_view_ != MAP_FAILED) munmap(write_view_, kCacheSize);
  exec_view_ = write_view_ = nullptr;
  return false;
}

uint8_t* A64CodeCache::Alloc(size_t size, uint8_t** write_ptr_out) {
  // Round up to alignment
  size = (size + kAllocAlign - 1) & ~(kAllocAlign - 1);

  size_t old_offset = offset_.fetch_add(size, std::memory_order_relaxed);
  if (old_offset + size > kCacheSize) {
    // Cache exhausted — cannot compile more functions.
    // This should never happen in practice (64 MB is very large for JIT code).
    offset_.fetch_sub(size, std::memory_order_relaxed);
    return nullptr;
  }

  *write_ptr_out = write_view_ + old_offset;
  return exec_view_ + old_offset;
}

void A64CodeCache::FlushInstrCache(uint8_t* write_start, size_t size) {
  // Flush both the write view (data cache) and the exec view (instruction cache)
  // so the CPU's I-cache sees the updated instructions.
  __builtin___clear_cache(reinterpret_cast<char*>(write_start),
                          reinterpret_cast<char*>(write_start + size));
  if (dual_mapped_) {
    // Compute the corresponding exec_view_ address and flush that too.
    ptrdiff_t off = write_start - write_view_;
    uint8_t* exec_start = exec_view_ + off;
    __builtin___clear_cache(reinterpret_cast<char*>(exec_start),
                            reinterpret_cast<char*>(exec_start + size));
  }
}

void A64CodeCache::RegisterFunction(GuestFunction* fn,
                                    uint8_t* exec_code,
                                    size_t code_size) {
  Entry entry;
  entry.fn    = fn;
  entry.start = reinterpret_cast<uintptr_t>(exec_code);
  entry.end   = entry.start + code_size;

  std::lock_guard<std::mutex> lock(entries_mutex_);
  // Insert in sorted order so binary search works
  auto it = std::lower_bound(entries_.begin(), entries_.end(), entry,
    [](const Entry& a, const Entry& b) { return a.start < b.start; });
  entries_.insert(it, entry);
}

GuestFunction* A64CodeCache::LookupFunction(uint64_t host_pc) {
  std::lock_guard<std::mutex> lock(entries_mutex_);
  // Binary search: find the last entry whose start <= host_pc
  auto it = std::upper_bound(entries_.begin(), entries_.end(), host_pc,
    [](uint64_t pc, const Entry& e) { return pc < e.start; });
  if (it == entries_.begin()) return nullptr;
  --it;
  if (host_pc < it->end) return it->fn;
  return nullptr;
}

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe
