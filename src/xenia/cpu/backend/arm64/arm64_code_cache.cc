/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_code_cache.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <algorithm>
#include <cstring>

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/memory.h"
#include "xenia/cpu/function.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

// ---------------------------------------------------------------------------
// AndroidARM64CodeCache
//
// On Android (and Linux ARM64 in general), we use anonymous mmap with
// PROT_READ | PROT_WRITE | PROT_EXEC.
//
// On Android 10+, W^X (Write XOR Execute) is enforced for some memory
// regions by SELinux. To work around this:
//  - We allocate a single large RWX region at init time using a memfd
//    or by using mprotect cycling (write, then exec).
//  - Alternatively, on newer Android (API 29+), use ASharedMemory + mmap
//    to get two views (RW and RX) of the same physical pages.
//
// For maximum compatibility we use the dual-mapping strategy:
//   1. Create a memfd (memory-backed file descriptor).
//   2. mmap it twice: once as RW (for writing code), once as RX (for exec).
//   3. Write to the RW view, execute from the RX view.
//
// This is exactly what LLVM's JIT does on Android.
// ---------------------------------------------------------------------------

class AndroidARM64CodeCache : public ARM64CodeCache {
 public:
  AndroidARM64CodeCache();
  ~AndroidARM64CodeCache() override;

  bool Initialize() override;

  void* LookupUnwindInfo(uint64_t host_pc) override { return nullptr; }
};

std::unique_ptr<ARM64CodeCache> ARM64CodeCache::Create() {
  return std::make_unique<AndroidARM64CodeCache>();
}

AndroidARM64CodeCache::AndroidARM64CodeCache() = default;
AndroidARM64CodeCache::~AndroidARM64CodeCache() {
  if (indirection_table_base_) {
    xe::memory::DeallocFixed(indirection_table_base_,
                             kIndirectionTableSize,
                             xe::memory::DeallocationType::kRelease);
  }
  if (generated_code_execute_base_ &&
      generated_code_execute_base_ != generated_code_write_base_) {
    xe::memory::DeallocFixed(generated_code_execute_base_,
                             kGeneratedCodeSize,
                             xe::memory::DeallocationType::kRelease);
  }
  if (generated_code_write_base_) {
    xe::memory::DeallocFixed(generated_code_write_base_,
                             kGeneratedCodeSize,
                             xe::memory::DeallocationType::kRelease);
  }
}

bool AndroidARM64CodeCache::Initialize() {
  // ------------------------------------------------------------------
  // Indirection table: maps guest PPC addresses (0–512MB) to host
  // offsets in the code cache. Each entry is 4 bytes (uint32_t offset).
  // Total size: 512MB / 4 bytes * 4 = 512MB entries × 4 = 2GB … too big.
  // Instead, we use a sparse table: guest_address >> 2 as index, so
  // 512MB / 4 = 128M entries × 4 bytes = 512MB. Still large.
  //
  // Practical approach (matching x64 backend): the table covers the
  // actual guest code range observed in 360 titles, which is much smaller.
  // We commit pages on demand in CommitExecutableRange().
  // ------------------------------------------------------------------
  indirection_table_base_ = reinterpret_cast<uint8_t*>(
      xe::memory::AllocFixed(reinterpret_cast<void*>(kIndirectionTableBase),
                             kIndirectionTableSize,
                             xe::memory::AllocationType::kReserve,
                             xe::memory::PageAccess::kNoAccess));
  if (!indirection_table_base_) {
    // Couldn't get the fixed address — try at any address.
    indirection_table_base_ = reinterpret_cast<uint8_t*>(
        xe::memory::AllocFixed(nullptr, kIndirectionTableSize,
                               xe::memory::AllocationType::kReserveCommit,
                               xe::memory::PageAccess::kReadWrite));
    if (!indirection_table_base_) {
      XELOGE("ARM64CodeCache: failed to allocate indirection table");
      return false;
    }
  }

  // ------------------------------------------------------------------
  // Generated code region — dual mapped for W^X compliance.
  //
  // We allocate a large anonymous region and attempt to map it at our
  // preferred base address. If the fixed address isn't available we fall
  // back to any address — the JIT code uses x19/x20 as base registers
  // for all context/memory accesses, so absolute code addresses only
  // matter for the veneer range of B/BL (±128MB).
  // ------------------------------------------------------------------

  // Try to get a dual mapping (write view + execute view at same PA).
  // Fall back to a single RWX mapping if that's not available.
#if defined(__ANDROID__) && __ANDROID_API__ >= 29
  // TODO: Use ASharedMemory_create() + two mmap() calls for true dual
  // mapping on Android 10+. This is cleaner than PROT_RWX.
  // For now, fall through to the PROT_RWX path.
#endif

  // Single RWX allocation — simplest, works on all Android versions
  // when selinux allows it (which it does for the app's own memory).
  generated_code_write_base_ = reinterpret_cast<uint8_t*>(
      xe::memory::AllocFixed(reinterpret_cast<void*>(kGeneratedCodeExecuteBase),
                             kGeneratedCodeSize,
                             xe::memory::AllocationType::kReserve,
                             xe::memory::PageAccess::kNoAccess));
  if (!generated_code_write_base_) {
    generated_code_write_base_ = reinterpret_cast<uint8_t*>(
        xe::memory::AllocFixed(nullptr, kGeneratedCodeSize,
                               xe::memory::AllocationType::kReserveCommit,
                               xe::memory::PageAccess::kExecuteReadWrite));
    if (!generated_code_write_base_) {
      XELOGE("ARM64CodeCache: failed to allocate code region");
      return false;
    }
  }
  generated_code_execute_base_ = generated_code_write_base_;

  XELOGI("ARM64CodeCache: indirection table @ {:016X}, code @ {:016X}",
         reinterpret_cast<uint64_t>(indirection_table_base_),
         reinterpret_cast<uint64_t>(generated_code_execute_base_));
  return true;
}

// ---------------------------------------------------------------------------
// Common ARM64CodeCache methods (shared across platforms)
// ---------------------------------------------------------------------------

ARM64CodeCache::ARM64CodeCache() = default;
ARM64CodeCache::~ARM64CodeCache() = default;

// Base-class no-op; concrete subclass overrides do the real work.
bool ARM64CodeCache::Initialize() { return true; }

void ARM64CodeCache::set_indirection_default(uint32_t default_value) {
  indirection_default_value_ = default_value;
}

void ARM64CodeCache::AddIndirection(uint32_t guest_address,
                                    uint32_t host_offset) {
  if (!indirection_table_base_) return;
  // Guest addresses are 4-byte aligned; divide by 4 to get table index.
  uint32_t* table = reinterpret_cast<uint32_t*>(indirection_table_base_);
  table[guest_address / 4] = host_offset;
}

void ARM64CodeCache::CommitExecutableRange(uint32_t guest_low,
                                           uint32_t guest_high) {
  // Commit indirection table pages covering [guest_low, guest_high).
  if (!indirection_table_base_) return;

  auto page_size = xe::memory::page_size();
  uint8_t* low_ptr =
      indirection_table_base_ + (guest_low / 4) * sizeof(uint32_t);
  uint8_t* high_ptr =
      indirection_table_base_ + (guest_high / 4) * sizeof(uint32_t);

  // Align to page boundaries.
  // round_down: align down to page boundary
  low_ptr = reinterpret_cast<uint8_t*>(
      (reinterpret_cast<size_t>(low_ptr) / page_size) * page_size);
  high_ptr = reinterpret_cast<uint8_t*>(
      xe::round_up(reinterpret_cast<size_t>(high_ptr), page_size));

  size_t length = high_ptr - low_ptr;
  if (length == 0) return;

  xe::memory::AllocFixed(low_ptr, length,
                         xe::memory::AllocationType::kCommit,
                         xe::memory::PageAccess::kReadWrite);

  // Fill newly committed pages with the default "not yet compiled" value.
  std::memset(low_ptr, indirection_default_value_, length);
}

void ARM64CodeCache::PlaceHostCode(uint32_t guest_address, void* machine_code,
                                   const EmitFunctionInfo& func_info,
                                   void*& code_execute_address_out,
                                   void*& code_write_address_out) {
  auto global_lock = global_critical_region_.Acquire();

  // Ensure we have space.
  size_t code_size = func_info.code_size.total;
  // ARM64 instructions must be 4-byte aligned.
  generated_code_offset_ = xe::round_up(generated_code_offset_, 4);

  // Commit more code pages if needed.
  size_t current_commit = generated_code_commit_mark_.load();
  if (generated_code_offset_ + code_size > current_commit) {
    auto page_size = xe::memory::page_size();
    size_t new_commit =
        xe::round_up(generated_code_offset_ + code_size, page_size);
    xe::memory::AllocFixed(generated_code_write_base_ + current_commit,
                           new_commit - current_commit,
                           xe::memory::AllocationType::kCommit,
                           xe::memory::PageAccess::kExecuteReadWrite);
    generated_code_commit_mark_.store(new_commit);
  }

  uint8_t* write_addr =
      generated_code_write_base_ + generated_code_offset_;
  uint8_t* exec_addr =
      generated_code_execute_base_ + generated_code_offset_;

  // Copy from the emitter's scratch buffer into the cache.
  std::memcpy(write_addr, machine_code, code_size);


  // Flush the instruction cache so the CPU sees the new code.
  // This is mandatory on ARM — the I-cache and D-cache are not coherent.
  __builtin___clear_cache(reinterpret_cast<char*>(write_addr),
                          reinterpret_cast<char*>(write_addr + code_size));

  code_execute_address_out = exec_addr;
  code_write_address_out = write_addr;
  generated_code_offset_ += code_size;
}

void ARM64CodeCache::PlaceGuestCode(uint32_t guest_address, void* machine_code,
                                    const EmitFunctionInfo& func_info,
                                    GuestFunction* function_info,
                                    void*& code_execute_address_out,
                                    void*& code_write_address_out) {
  PlaceHostCode(guest_address, machine_code, func_info, code_execute_address_out,
                code_write_address_out);

  // Record in the sorted map for LookupFunction().
  auto global_lock = global_critical_region_.Acquire();
  uint64_t exec_addr = reinterpret_cast<uint64_t>(code_execute_address_out);
  uint64_t end_addr = exec_addr + func_info.code_size.total;
  generated_code_map_.emplace_back(
      (exec_addr << 32) | (end_addr & 0xFFFFFFFF), function_info);
}

uint32_t ARM64CodeCache::PlaceData(const void* data, size_t length) {
  auto global_lock = global_critical_region_.Acquire();

  // Align to 16 bytes for NEON loads.
  generated_code_offset_ = xe::round_up(generated_code_offset_, 16);

  // Grow commit if needed (same logic as PlaceHostCode).
  size_t current_commit = generated_code_commit_mark_.load();
  if (generated_code_offset_ + length > current_commit) {
    auto page_size = xe::memory::page_size();
    size_t new_commit =
        xe::round_up(generated_code_offset_ + length, page_size);
    xe::memory::AllocFixed(generated_code_write_base_ + current_commit,
                           new_commit - current_commit,
                           xe::memory::AllocationType::kCommit,
                           xe::memory::PageAccess::kExecuteReadWrite);
    generated_code_commit_mark_.store(new_commit);
  }

  uint32_t data_offset = static_cast<uint32_t>(generated_code_offset_);
  std::memcpy(generated_code_write_base_ + generated_code_offset_, data, length);
  generated_code_offset_ += length;
  return data_offset;
}

GuestFunction* ARM64CodeCache::LookupFunction(uint64_t host_pc) {
  auto global_lock = global_critical_region_.Acquire();

  // Entries are appended in placement order, which is monotonically increasing
  // host address, so the map is sorted by start address. Each entry's key is
  // [start_addr(32) | end_addr_low32(32)]. The function CONTAINING host_pc is
  // the last one whose start <= host_pc — find it with upper_bound, then step
  // back. (The old lower_bound looked for start >= host_pc, which is the
  // function AFTER the one we want, so it always missed.)
  auto it = std::upper_bound(
      generated_code_map_.begin(), generated_code_map_.end(), host_pc,
      [](uint64_t pc, const std::pair<uint64_t, GuestFunction*>& e) {
        return pc < (e.first >> 32);
      });
  if (it == generated_code_map_.begin()) {
    return nullptr;
  }
  --it;
  uint64_t start = it->first >> 32;
  // end was stored as only its low 32 bits; a function never spans a 4 GiB
  // boundary, so restore the high bits from start.
  uint64_t end = (start & ~uint64_t(0xFFFFFFFF)) | (it->first & 0xFFFFFFFF);
  if (host_pc >= start && host_pc < end) {
    return it->second;
  }
  return nullptr;
}

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
