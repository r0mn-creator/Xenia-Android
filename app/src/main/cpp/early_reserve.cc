// Xenia Android (codename "Falcon") — reserve the JIT code-cache fixed address
// ranges at shared-library load time. THIS FILE IS OURS (not part of Canary).
//
// Why: the a64 code cache must map its indirection table at a FIXED address
// (kIndirectionTableBase = 0x80000000) and its generated-code storage at
// 0xA0000000 (see cpu/backend/code_cache_base.h). It reserves them with
// xe::memory::AllocFixed, which on Linux/Android uses MAP_FIXED_NOREPLACE and so
// FAILS if anything — typically an ASLR-placed system library — already sits in
// that low 2 GB region. Observed as intermittent, launch-killing
// "Unable to allocate code cache indirection table" failures.
//
// Fix: claim that whole range as early as possible (a .so-load constructor, which
// runs before the emulator initializes and before most late mmaps) with a
// PROT_NONE placeholder, keeping ASLR out of it. Right before the code cache does
// its own AllocFixed, code_cache_base.h calls the weak
// xe_android_release_lowmem_reservation() (CANARY_PATCHES.md patch #16) to release
// our placeholder so the code cache's MAP_FIXED_NOREPLACE succeeds. Init is
// single-threaded, so there is no window for ASLR to retake the range in between.

#include <sys/mman.h>

#include <android/log.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace {
// Must cover both kIndirectionTableBase (0x80000000, size ~0x20000000) and
// kGeneratedCodeExecuteBase (0xA0000000, size ~0x10000000).
constexpr uintptr_t kReserveBase = 0x80000000;
constexpr size_t kReserveSize = 0x30000000;  // 0x80000000 .. 0xAFFFFFFF
void* g_reservation = MAP_FAILED;
}  // namespace

// Runs when libxeniaapp.so is loaded (priority 101 = earliest user constructors).
__attribute__((constructor(101))) static void XeniaAndroidReserveLowMem() {
  g_reservation =
      mmap(reinterpret_cast<void*>(kReserveBase), kReserveSize, PROT_NONE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (g_reservation == reinterpret_cast<void*>(kReserveBase)) {
    __android_log_print(ANDROID_LOG_INFO, "XeniaAndroid",
                        "Reserved JIT code-cache range 0x%lx-0x%lx at load",
                        (unsigned long)kReserveBase,
                        (unsigned long)(kReserveBase + kReserveSize));
  } else {
    if (g_reservation != MAP_FAILED) {
      munmap(g_reservation, kReserveSize);
    }
    g_reservation = MAP_FAILED;
    __android_log_print(
        ANDROID_LOG_WARN, "XeniaAndroid",
        "Could not pre-reserve JIT code-cache range (already in use) — "
        "code-cache alloc may be flaky this launch");
    // XENIA-ANDROID TEMP: log what already occupies the range, to diagnose.
    FILE* f = fopen("/proc/self/maps", "r");
    if (f) {
      char line[512];
      while (fgets(line, sizeof(line), f)) {
        unsigned long ms = 0, me = 0;
        if (sscanf(line, "%lx-%lx", &ms, &me) == 2 && me > kReserveBase &&
            ms < kReserveBase + kReserveSize) {
          size_t n = 0;
          while (line[n] && line[n] != '\n') ++n;
          line[n] = '\0';
          __android_log_print(ANDROID_LOG_WARN, "XeniaAndroid", "  occupant: %s",
                              line);
        }
      }
      fclose(f);
    }
  }
}

// Called by the code cache (weakly) right before it AllocFixes the fixed ranges.
extern "C" void xe_android_release_lowmem_reservation() {
  if (g_reservation != MAP_FAILED) {
    munmap(g_reservation, kReserveSize);
    g_reservation = MAP_FAILED;
    __android_log_print(ANDROID_LOG_INFO, "XeniaAndroid",
                        "Released JIT code-cache reservation for the code cache");
  }
}
