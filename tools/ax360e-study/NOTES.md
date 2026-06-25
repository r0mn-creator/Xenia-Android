# aX360e deep-dive: how it gets past the black screen

**Goal:** aX360e (Canary `2b4c889e` "1.16" + the official `a64` JIT + their `XE_PLATFORM_AX360E`
adaptations) **renders Xbox 360 games on Android**; our build (Canary `e7d0e45` + same a64 backend
+ our `XE_PLATFORM_ANDROID` adaptations) gets stuck at a black screen with a flaky heap corruption
(`PosixThread::set_name` double-free) / deadlock. This studies *their* differences to find what we
lack.

- aX360e source: `/home/roman/Android/ax360e/app/src/main/cpp/xenia-canary/`
- Our source: `/home/roman/Android/Xenia-Android/third_party/xenia-canary/`
- aX360e marks all its changes with `#if XE_PLATFORM_AX360E`. **19 files** touched.
- Tool: `extract_ax360e.sh <relpath>` — dumps a file's `XE_PLATFORM_AX360E` blocks.

## The 19 AX360E-modified files (by subsystem)
```
emulator.h / emulator.cc / kernel/kernel_flags.cc / kernel/xam/profile_manager.cc   -- boot/config
memory.cc / base/memory_posix.cc / base/mapped_memory.h / base/mapped_memory_posix.cc / base/memory_posix.cc  -- MEMORY
base/threading_posix.cc / base/logging.cc / base/platform.h                          -- platform
cpu/backend/a64/a64_code_cache.cc / a64_code_cache_posix.cc                          -- JIT code cache / UNWIND
gpu/graphics_system.cc                                                               -- presenter
ui/imgui_drawer.cc / ui/renderdoc_api.cc / ui/vulkan/vulkan_api.h / vulkan_instance.cc / vulkan_presenter.cc -- Vulkan UI
```

---

## FINDING 1 — Memory base address is FIXED, not dynamic (STRONG suspect for our corruption)
`memory.cc` (`MapViews`/heap init):
- **Upstream/our build:** loops `for n in 32..63 { base = 1<<n; if MapViews(base) ok -> use it }`
  — picks the FIRST 2^n host base that maps. DYNAMIC, varies per launch.
- **aX360e:** `base = (cvars::mmap_address_high) << 32` with `assert(1<=high<=124)` — a single
  CONFIGURED fixed base; aborts if it can't map there. They CONTROL the exact memory layout.

Why this matters for us: our build *also* relocates the JIT indirection table to `0x280000000`
(patch #18, because ART heap squats on `0x80000000`) and puts generated code at `0xA0000000`. If
our dynamic membase picks e.g. `0x200000000` (2^33), the 4 GB of guest memory views can **overlap
the relocated indirection table / code cache** -> guest stores scribble JIT structures (or vice
versa) -> flaky corruption. aX360e sidesteps this entirely by pinning membase via a cvar and
keeping the whole layout deterministic. **ACTION: verify our membase vs our indirection-table /
code-cache ranges for overlap; consider pinning membase like aX360e.**

## FINDING 2 — JIT exception-unwinding (`a64_code_cache_posix.cc`) is reworked (suspect)
The a64 backend uses **C++ exceptions to unwind through JIT frames** (`FiberReentryException` in
`XThread::Execute`, used for guest fiber/stack switching e.g. `KeSetCurrentStackPointers`). That
requires correct DWARF `.eh_frame` for every JIT function, registered via `__register_frame`.
- **Both** ours and aX360e hand-build CIE/FDE and record the callee-saved saves (x19-x28, d8-d15,
  x29/x30, LR) for thunks — so our unwind tables are NOT obviously missing register info.
- **aX360e differences:**
  1. CIE augmentation `"zPLR"` (Personality `__gxx_personality_v0` + LSDA + FDE-enc); ours is `"zR"`
     (FDE-enc only, no personality).
  2. eh_frame lives in a **separate 64 MB heap table** (`new uint8_t[64MB]`), `__register_frame`d
     from there; ours writes the eh_frame **into the code-cache write region** and `__register_frame`s
     the **execute-side** address.
  3. A custom `__jit_personality`/`trace` for debugging unwinds.
- **Open question / suspect:** if our code-cache *execute* mapping is `PROT_EXEC` without `PROT_READ`,
  the unwinder can't READ the eh_frame we registered there -> unwinding through JIT frames fails ->
  stack/heap corruption when a `FiberReentryException` crosses JIT frames. aX360e's heap table is
  always readable. **ACTION: check our code-cache execute-mapping protection; check if the guest
  actually throws FiberReentryException (KeSetCurrentStackPointers) before the black screen.**

## FINDING 3 — Vulkan presenter created directly, not on the UI thread
`gpu/graphics_system.cc`: `#if !XE_PLATFORM_AX360E` guards the
`app_context_->CallInUIThreadSynchronous([]{ presenter_ = CreatePresenter(...); })`; on AX360E it
falls through to creating the presenter **directly** on the GPU-init thread (the "offscreen" path).
Avoids a UI-thread round-trip/deadlock. NOTE: our presenter already works (we present frames), so
this is likely NOT our blocker — but worth keeping if we ever see presenter-init hangs.

## FINDING 4 — Shared memory via direct `ASharedMemory_create`
`base/memory_posix.cc`: aX360e calls `ASharedMemory_create()` directly (`<android/sharedmem.h>`);
we dlsym `android_ASharedMemory_create_` or fall back to `/dev/ashmem`. Functionally equivalent;
minor.

---

## STILL TO STUDY
- `base/threading_posix.cc` AX360E changes (our corruption is in the thread lifecycle).
- `emulator.cc` / `emulator.h` / `kernel_flags.cc` (boot/config: cvars incl. `mmap_address_high`).
- `base/platform.h` (what XE_PLATFORM_AX360E enables).
- Whether aX360e's newer canary (2b4c889e) fixes something our e7d0e45 lacks (separate from the
  AX360E #ifdefs).

## FINDING 5 — Robust mutexes disabled (threading_posix.cc) — WE MATCH
aX360e `#if !XE_PLATFORM_AX360E` guards out the robust-mutex init + EOWNERDEAD recovery, using a
plain `std::mutex`. Our patch #4 does the same for `XE_PLATFORM_ANDROID`. **No difference.** (Also:
aX360e adds `kThreadTerminate` signal + `g_thr_user_callback` atomic for its APC/terminate path —
similar in spirit to our #15.)

## FINDING 6 — `headless = true` on AX360E (kernel/kernel_flags.cc) — STRONG suspect for the stall
- aX360e: `DEFINE_bool(headless, true, ...)`.
- upstream/ours: `headless = false`.
`headless` controls whether the emulator shows UI dialogs/prompts and waits for input. With
`headless=false` (our build), kernel/XAM paths that pop a UI prompt (ImGui MessageBox, sign-in,
content/disc selection, KeBugCheck dialog) call `app_context_->CallInUIThreadSynchronous(...)` and
**block the calling guest thread on the Android UI thread**. If that UI thread isn't pumping (or it
deadlocks), the guest thread is stuck forever — exactly our "Main joins a XAM task that never
finishes" symptom. aX360e (`headless=true`) takes default answers and never blocks on UI.
**ACTION: set headless=true (or via cvar) and check if the stall clears. Cheap to test.**

## FINDING 7 — SAF (Storage Access Framework) VFS devices (emulator.cc)
aX360e mounts games through `SAF_XexDevice / SAF_StfsDevice / SAF_DiscImageDevice /
SAF_DiscZarchiveDevice` (Android DocumentFile-backed). Their whole storage stack is SAF. We use
file paths + HostPathDevice/DiscImageDevice. Different approach, both can load a game; not the
black-screen cause, but explains different storage code paths.

---
# SUMMARY: the 3 real AX360E-only differences + PRIORITIZED ACTIONS

The Vulkan/rendering/UI Android code is SHARED (`||XE_PLATFORM_ANDROID`), so our build already has
aX360e's render path. **The black screen is NOT a rendering bug** — the game never reaches content.
The only AX360E-vs-our differences that can explain the stall/corruption are:

### ACTION 1 (cheapest, do first) — `headless = true`
The game IMPORTS `XamShowMessageBoxUIEx`, `XamUserGetSigninInfo`, `XamShowCustomMessageComposeUI`
(seen in the import table at boot). With `headless=false` (our default), if the game pops a startup
message box / sign-in prompt, Xenia routes it through `app_context_->CallInUIThreadSynchronous(...)`
and BLOCKS the guest thread on the Android UI thread — matching our "Main joins a XAM task that
never finishes" deadlock. aX360e ships `headless=true` so these return defaults immediately.
- Our intent cvar passthrough does NOT currently apply `--es xe_headless true` (log still shows
  `headless = false`). So test it by either (a) fixing the passthrough, or (b) changing the
  DEFINE_bool default to true (cheapest), then run Halo and watch DRAW count / shaders.
- Verifies/refutes in ONE rebuild. Highest value.

### ACTION 2 — JIT exception unwinding (eh_frame) — for the flaky CORRUPTION
The flaky `set_name` double-free + deadlock is heap/stack corruption (a RACE/wild-write, since the
timing varies 9s..2.5min). The a64 backend unwinds through JIT frames via C++ exceptions
(`FiberReentryException`, thrown by guest `KeSetCurrentStackPointers`). aX360e reworked the eh_frame
(separate 64MB heap table + `zPLR` personality) — implying the in-code-region `__register_frame`
approach (ours) is unreliable on bionic. If unwinding through JIT frames mis-restores callee-saved
regs, that's exactly a flaky wild-write. ACTION: confirm the guest hits FiberReentryException before
the black screen; if so, port aX360e's eh_frame table approach. (Diff: `diff_vs_ax360e.sh
cpu/backend/a64/a64_code_cache_posix.cc`.)

### ACTION 3 — Fixed membase (weakened, but verify)
aX360e pins membase = `cvars::mmap_address_high << 32`; ours is dynamic (first 2^n). Runtime check
showed the indirection table relocated to `0x280000000` on its FIRST try (so it was free => membase
didn't overlap it that launch). Static-overlap is therefore unlikely, but pinning membase removes a
variable and matches aX360e. Lower priority than 1 & 2.

### ACTION 4 — Canary version
aX360e = Canary `2b4c889e` ("1.16"); ours = `e7d0e45`. Newer canary may carry kernel/threading
fixes independent of the `#ifdef`s. Consider rebasing onto a newer canary later.

## TOOLS in this folder
- `extract_ax360e.sh <relpath>` — dump a file's XE_PLATFORM_AX360E blocks.
- `diff_vs_ax360e.sh <relpath>` — unified diff of a file (ours vs aX360e).
