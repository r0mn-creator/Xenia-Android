/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/memory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstddef>

#include "xenia/base/math.h"
#include "xenia/base/platform.h"
#include "xenia/base/string.h"

#if XE_PLATFORM_ANDROID
#include <dlfcn.h>
#include <linux/ashmem.h>
#include <string.h>
#include <sys/ioctl.h>

#include "xenia/base/main_android.h"
#endif

namespace xe {
namespace memory {

#if XE_PLATFORM_ANDROID
// May be null if no dynamically loaded functions are required.
static void* libandroid_;
// API 26+.
static int (*android_ASharedMemory_create_)(const char* name, size_t size);

void AndroidInitialize() {
  if (xe::GetAndroidApiLevel() >= 26) {
    libandroid_ = dlopen("libandroid.so", RTLD_NOW);
    assert_not_null(libandroid_);
    if (libandroid_) {
      android_ASharedMemory_create_ =
          reinterpret_cast<decltype(android_ASharedMemory_create_)>(
              dlsym(libandroid_, "ASharedMemory_create"));
      assert_not_null(android_ASharedMemory_create_);
    }
  }
}

void AndroidShutdown() {
  android_ASharedMemory_create_ = nullptr;
  if (libandroid_) {
    dlclose(libandroid_);
    libandroid_ = nullptr;
  }
}
#endif

size_t page_size() { return getpagesize(); }
size_t allocation_granularity() { return page_size(); }

uint32_t ToPosixProtectFlags(PageAccess access) {
  switch (access) {
    case PageAccess::kNoAccess:
      return PROT_NONE;
    case PageAccess::kReadOnly:
      return PROT_READ;
    case PageAccess::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageAccess::kExecuteReadOnly:
      return PROT_READ | PROT_EXEC;
    case PageAccess::kExecuteReadWrite:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      assert_unhandled_case(access);
      return PROT_NONE;
  }
}

bool IsWritableExecutableMemorySupported() { return true; }

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

void* AllocFixed(void* base_address, size_t length,
                 AllocationType allocation_type, PageAccess access) {
  // mmap does not support reserve / commit, so ignore allocation_type for the
  // protection, but use it to decide whether we may replace existing mappings.
  uint32_t prot = ToPosixProtectFlags(access);
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (base_address != nullptr) {
    // IMPORTANT: a plain MAP_FIXED silently unmaps whatever is already at
    // base_address. On Android that can clobber critical system mappings that
    // happen to land in our preferred reservation ranges (e.g. ART's JIT code
    // caches), corrupting the runtime. For an initial reservation we therefore
    // use MAP_FIXED_NOREPLACE so the call fails (rather than overwriting) when
    // the range is occupied, letting the caller fall back to a kernel-chosen
    // address. Committing (re-mapping over our own prior reservation) still
    // needs to replace, so it keeps MAP_FIXED.
    // MAP_FIXED_NOREPLACE is only needed on Android, where ART's mappings can
    // occupy our preferred reservation ranges. On desktop Linux the guest
    // address space is free, and plain MAP_FIXED is the upstream behavior (the
    // x64 code cache reserves fixed host addresses 0x80000000/0xA0000000 and
    // relies on getting them).
#if XE_PLATFORM_ANDROID
    if (allocation_type == AllocationType::kReserve) {
      flags |= MAP_FIXED_NOREPLACE;
    } else {
      flags |= MAP_FIXED;
    }
#else
    flags |= MAP_FIXED;
#endif
  }
  void* result = mmap(base_address, length, prot, flags, -1, 0);
  if (result == MAP_FAILED) {
    return nullptr;
  }
  // With MAP_FIXED_NOREPLACE an old kernel may ignore the flag and place the
  // mapping elsewhere; if we required the exact address the caller treats a
  // mismatch as failure by checking the returned pointer.
  return result;
}

bool DeallocFixed(void* base_address, size_t length,
                  DeallocationType deallocation_type) {
  return munmap(base_address, length) == 0;
}

bool Protect(void* base_address, size_t length, PageAccess access,
             PageAccess* out_old_access) {
  // Linux does not have a syscall to query memory permissions.
  assert_null(out_old_access);

  uint32_t prot = ToPosixProtectFlags(access);
  return mprotect(base_address, length, prot) == 0;
}

bool QueryProtect(void* base_address, size_t& length, PageAccess& access_out) {
  return false;
}

FileMappingHandle CreateFileMappingHandle(const std::filesystem::path& path,
                                          size_t length, PageAccess access,
                                          bool commit) {
#if XE_PLATFORM_ANDROID
  // TODO(Triang3l): Check if memfd can be used instead on API 30+.
  if (android_ASharedMemory_create_) {
    int sharedmem_fd = android_ASharedMemory_create_(path.c_str(), length);
    return sharedmem_fd >= 0 ? sharedmem_fd : kFileMappingHandleInvalid;
  }

  // Use /dev/ashmem on API versions below 26, which added ASharedMemory.
  // /dev/ashmem was disabled on API 29 for apps targeting it.
  // https://chromium.googlesource.com/chromium/src/+/master/third_party/ashmem/ashmem-dev.c
  int ashmem_fd = open("/" ASHMEM_NAME_DEF, O_RDWR);
  if (ashmem_fd < 0) {
    return kFileMappingHandleInvalid;
  }
  char ashmem_name[ASHMEM_NAME_LEN];
  strlcpy(ashmem_name, path.c_str(), xe::countof(ashmem_name));
  if (ioctl(ashmem_fd, ASHMEM_SET_NAME, ashmem_name) < 0 ||
      ioctl(ashmem_fd, ASHMEM_SET_SIZE, length) < 0) {
    close(ashmem_fd);
    return kFileMappingHandleInvalid;
  }
  return ashmem_fd;
#else
  int oflag;
  switch (access) {
    case PageAccess::kNoAccess:
      oflag = 0;
      break;
    case PageAccess::kReadOnly:
    case PageAccess::kExecuteReadOnly:
      oflag = O_RDONLY;
      break;
    case PageAccess::kReadWrite:
    case PageAccess::kExecuteReadWrite:
      oflag = O_RDWR;
      break;
    default:
      assert_always();
      return kFileMappingHandleInvalid;
  }
  oflag |= O_CREAT;
  auto full_path = "/" / path;
  int ret = shm_open(full_path.c_str(), oflag, 0777);
  if (ret < 0) {
    return kFileMappingHandleInvalid;
  }
  ftruncate64(ret, length);
  return ret;
#endif
}

void CloseFileMappingHandle(FileMappingHandle handle,
                            const std::filesystem::path& path) {
  close(handle);
#if !XE_PLATFORM_ANDROID
  auto full_path = "/" / path;
  shm_unlink(full_path.c_str());
#endif
}

void* MapFileView(FileMappingHandle handle, void* base_address, size_t length,
                  PageAccess access, size_t file_offset) {
  uint32_t prot = ToPosixProtectFlags(access);
  // MAP_SHARED (not MAP_PRIVATE|MAP_ANONYMOUS): use the file-backed handle so
  // multiple views of the same fd alias the same physical pages — this is how
  // the Xbox 360 virtual/physical address aliasing (e.g. 0xA0000000 == phys
  // 0x00000000) actually works. MAP_FIXED ensures the mapping lands at the
  // requested address so virtual_membase_ + guest_addr always works.
  // Use MAP_FIXED_NOREPLACE rather than MAP_FIXED so that probing candidate
  // base addresses (Memory::MapViews tries 2^32, 2^33, ... until a whole set of
  // views fits) does NOT clobber whatever already lives at the trial address.
  // A plain MAP_FIXED there would silently destroy system mappings such as
  // ART's JIT code caches, corrupting the runtime. If the kernel ignores the
  // flag (pre-4.17) and places the view elsewhere, we reject it so the caller
  // treats it as a failure and tries the next base.
  int flags = MAP_SHARED;
  if (base_address != nullptr) {
#if XE_PLATFORM_ANDROID
    flags |= MAP_FIXED_NOREPLACE;
#else
    flags |= MAP_FIXED;
#endif
  }
  void* result = mmap64(base_address, length, prot, flags, handle, file_offset);
  if (result == MAP_FAILED) {
    return nullptr;
  }
  if (base_address != nullptr && result != base_address) {
    munmap(result, length);
    return nullptr;
  }
  return result;
}

bool UnmapFileView(FileMappingHandle handle, void* base_address,
                   size_t length) {
  return munmap(base_address, length) == 0;
}

}  // namespace memory
}  // namespace xe
