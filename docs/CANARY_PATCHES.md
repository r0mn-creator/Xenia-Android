# Canary Patch Registry

> **Purpose of this file:** This project keeps the Xenia-Canary engine as a *pristine
> upstream submodule* and layers Android support on top. Every edit we make to a file
> **inside `third_party/xenia-canary/`** is recorded here. This is the single most important
> document for the project's stated goal: **updating to the latest Canary should only require
> re-applying these small, well-understood edits.**
>
> **Rule:** If you change a file inside `third_party/xenia-canary/`, you MUST add an entry
> here. If you can solve a problem *without* touching Canary (by adding a file in our `app/`
> module instead), always prefer that — it costs nothing on update.

Pinned Canary commit: `e7d0e45` (canary_experimental, 2026-06-19).

---

## Patch philosophy (read before editing Canary)

1. **Prefer additive, guarded edits.** Wrap Android changes in `#if XE_PLATFORM_ANDROID` /
   `if(ANDROID)` so they never affect desktop and are easy for upstream to accept.
2. **Touch as few Canary files as possible.** Each file here is a potential merge conflict on
   update. Everything that *can* live in `app/` (our build, JNI glue, UI) MUST live there.
3. **Keep each patch small and self-contained**, with a comment tag `// XENIA-ANDROID:` (C++)
   or `# XENIA-ANDROID:` (CMake) at every edit site so they're greppable inside Canary.
4. **Goal: these patches are upstreamable.** If Canary ever merges Android support, this list
   shrinks toward zero and updates become "just bump the submodule."

To find every in-tree edit at any time:
```
grep -rn "XENIA-ANDROID:" third_party/xenia-canary/
```

---

## How to update Canary later (the payoff)

See `docs/UPDATING_CANARY.md` for the full procedure. In short: rebase our `android` branch
of the fork onto newer upstream, resolve conflicts **only** in the files listed below, rebuild.

---

## Registry of edits to `third_party/xenia-canary/`

> Format: each entry = file, why, what, and how risky it is on update.
> (Populated as the Android build port proceeds — currently being authored.)

| # | File | Why | Risk on update |
|---|------|-----|----------------|
| 1 | `CMakeLists.txt` | Set `XE_PLATFORM_NAME "Android"` for build output/log clarity | low |
| 2 | `cmake/XeniaHelpers.cmake` | Add Android branch to `xe_platform_sources` so `_posix`+`_android` sources compile | low-med |
| 3 | `src/xenia/base/threading_posix.cc` | Mark `SetAndroidPreApi26Name` `const` (+ `mutable` name buffer); latent const bug | low |
| 4 | `src/xenia/base/threading_posix.cc` | Guard robust-mutex code — bionic has none; plain-mutex fallback | med |
| 5 | `src/xenia/ui/imgui_drawer.cc` | Exclude desktop fontconfig on Android | low-med |
| 6 | `src/xenia/ui/window_android.cc` | Update stale `OnActualSizeUpdate` call | low |
| 7 | `src/xenia/base/filesystem_posix.cc` | Guard posix `SetAttributes` on Android — duplicate symbol | low |
| 8 | `src/xenia/app/xenia_main.cc` | Guard ALSA audio backend with `!XE_PLATFORM_ANDROID` (no ALSA on Android) | low |
| 9 | `src/xenia/app/xenia_main.cc` | Comment out Discord rich presence (desktop extra, not for playing games) | low |
| 10 | `src/xenia/app/xenia_main.cc` | Comment out the desktop debug-UI window | low |
| 11 | `src/xenia/CMakeLists.txt` + NEW `src/xenia/apu/aaudio/` | Add AAudio audio backend (Android) | low |
| 12 | `src/xenia/app/xenia_main.cc` | Register AAudio in the audio-system factory (Android) | low |
| 13 | `src/xenia/base/threading_posix.cc` | Fix thread-start lost-wakeup (suspend_count_ set atomically with state_) | med |
| 14 | `src/xenia/CMakeLists.txt` + NEW `src/xenia/hid/android/` + `xenia_main.cc` | Add Android input (HID) backend | low |
| 15 | `src/xenia/base/threading_posix.cc` | Fix QueueUserCallback (APC wakeup) to target the specific thread via rt_tgsigqueueinfo | med |
| 16 | `src/xenia/cpu/backend/code_cache_base.h` (+ app early_reserve.cc) | Release early low-mem reservation before code-cache AllocFixed (fixes flaky 0x80000000 alloc) | low |
| 17 | `src/xenia/kernel/xam/profile_manager.cc` | Auto-create default offline profile 'Jimmy' if none exist | low |
| 18 | `code_cache_base.h` + `a64/a64_emitter.cc` | Relocate JIT indirection table off 0x80000000 when ART Java heap collides | med |
| 19 | `src/xenia/kernel/xobject.cc` | GetNativeObject: handle null native_ptr gracefully | low |
| 20 | `src/xenia/kernel/xthread.cc` | XThread::Exit: defer ReleaseHandle via pthread cleanup (fix use-after-destroy of PosixThread on detached-thread exit) | med |

### 1. `third_party/xenia-canary/CMakeLists.txt` — `XE_PLATFORM_NAME`
- **Why:** Canary sets `XE_PLATFORM_NAME` to Windows/macOS/Linux only; Android fell through
  to "Linux". Android shares `XE_PLATFORM_LINUX` in `platform.h` (correct), but the build's
  output dirs/logs should identify as "Android".
- **What:** Inserted an additive `elseif(ANDROID) set(XE_PLATFORM_NAME "Android")` branch
  before the `else()`/Linux fallthrough. Canary's original lines are untouched. Tagged
  `# XENIA-ANDROID:`.
- **Upstreamable?:** Yes (trivial, harmless to desktop).
- **Update risk:** **Low** — tiny additive branch; only conflicts if upstream rewrites the
  platform-name block.

### 2. `third_party/xenia-canary/cmake/XeniaHelpers.cmake` — `xe_platform_sources` Android branch
- **Why:** The macro that globs platform-specific source files (`*_win`, `*_posix`,
  `*_android`, …) only had `if(WIN32) … elseif(Linux) …`. On Android (`CMAKE_SYSTEM_NAME` =
  "Android", not "Linux") neither matched, so **no** platform sources were added — the
  `_posix` base impls and the `_android` windowing (windowed_app_context_android.cc,
  surface_android.cc, window_android.cc, file_picker_android.cc) would be silently dropped.
- **What:** Added an additive `elseif(ANDROID)` branch globbing `*_posix.*` + `*_android.*`
  (NOT the desktop `_linux/_gnulinux/_x11/_gtk`). Upstream's Win/Linux branches untouched.
  Tagged `# XENIA-ANDROID:`.
- **Upstreamable?:** Yes — clean, guarded, fills a real gap in Android support.
- **Update risk:** **Low-Med** — only conflicts if upstream rewrites this glob block; the
  intent ("Android = posix + android sources") is easy to re-apply.

### 3. `third_party/xenia-canary/src/xenia/base/threading_posix.cc` — const fix
- **Why:** `SetAndroidPreApi26Name` is called from a `const` member function but wasn't
  marked `const`; only fails when compiled for Android (upstream never does). It only reads
  cached members + calls pthread, so `const` is correct.
- **What:** Added `const` to the method (one keyword), tagged `// XENIA-ANDROID:`.
- **Upstreamable?:** Yes (a genuine bug fix).
- **Update risk:** **Low**.

### 4. `third_party/xenia-canary/src/xenia/base/threading_posix.cc` — robust-mutex guard
- **Why:** `PosixConditionBase` reinitializes its `std::mutex` as a POSIX *robust* mutex and
  recovers from `EOWNERDEAD` in `Wait`/`WaitMultiple`. **bionic does not implement robust
  mutexes** (`PTHREAD_MUTEX_ROBUST`, `pthread_mutex_consistent`, `pthread_mutexattr_setrobust`
  are glibc-only, at *no* Android API level). So this is a hard compile error on Android.
- **What:** Wrapped all three sites in `#if XE_PLATFORM_ANDROID` (plain `std::mutex` lock /
  `try_to_lock`) `#else` (Canary's exact original robust code, untouched) `#endif`. Canary's
  desktop behavior is unchanged; Android uses a normal mutex.
- **Rationale for safety:** robust mutexes were Canary's defense against a thread dying while
  holding a lock — which in our old project was a *symptom of the buggy hand-written arm64
  JIT*, not normal behavior. With correct `a64` codegen threads don't die unexpectedly, so a
  plain mutex is acceptable.
- **Upstreamable?:** Yes — Android needs a non-robust path regardless.
- **Update risk:** **Med** — if upstream restructures `PosixConditionBase` locking, re-apply
  the same `#if XE_PLATFORM_ANDROID` guards around the new robust calls.

### 5. `third_party/xenia-canary/src/xenia/ui/imgui_drawer.cc` — fontconfig guard
- **Why:** ROOT ISSUE (will recur): upstream `platform.h` defines BOTH `XE_PLATFORM_ANDROID`
  and `XE_PLATFORM_LINUX` on Android (for POSIX sharing). So desktop code under
  `#if XE_PLATFORM_LINUX` (here: `<fontconfig/fontconfig.h>` + `Fc*` CJK-font discovery) leaks
  into the Android build and fails ('fontconfig/fontconfig.h' not found). Android has no
  fontconfig.
- **What:** Changed this file's `XE_PLATFORM_LINUX` guards to
  `XE_PLATFORM_LINUX && !XE_PLATFORM_ANDROID`. The imgui debug overlay just uses default
  fonts on Android (CJK in the debug UI may not display — non-essential).
- **Upstreamable?:** Yes.
- **Update risk:** **Low-Med** — re-apply the `&& !XE_PLATFORM_ANDROID` if upstream font code
  moves. EXPECT MORE files needing the same pattern (any desktop-Linux code under
  XE_PLATFORM_LINUX).

### 6. `third_party/xenia-canary/src/xenia/ui/window_android.cc` — stale API call
- **Why:** `Window::OnActualSizeUpdate` gained a `WindowResizeAction cause_action` parameter,
  but the Android backend (never compiled upstream) still called it with 3 args.
- **What:** Added `WindowResizeAction::kManual` (matches the GTK backend's value for an
  OS-driven resize). Original 3-arg call kept commented above for reference.
- **Upstreamable?:** Yes (a real bug — the Android backend is out of sync).
- **Update risk:** **Low**, but EXPECT MORE: the whole `*_android.cc` UI backend bit-rotted
  because upstream never builds it, so other stale-API calls are likely as compilation
  proceeds.

### 7. `third_party/xenia-canary/src/xenia/base/filesystem_posix.cc` — duplicate symbol
- **Why:** Both `filesystem_posix.cc` and `filesystem_android.cc` define
  `xe::filesystem::SetAttributes`. Our build compiles both `_posix` and `_android` sources
  (patch #2), so the posix one collides at link time (duplicate symbol). Android's version
  is the correct one (Android storage semantics).
- **What:** Wrapped the posix `SetAttributes` in `#if !XE_PLATFORM_ANDROID` (original kept,
  just guarded). Tagged `// XENIA-ANDROID:`.
- **Upstreamable?:** Yes.
- **Update risk:** **Low** — but EXPECT MORE such posix/android duplicate-symbol pairs as
  linking proceeds (any function the android file overrides). Same fix each time.

### 8–10. `third_party/xenia-canary/src/xenia/app/xenia_main.cc` — desktop app features
The emulator app references desktop-only features that don't exist on Android and caused
undefined symbols at link. Per project direction ("only keep what plays Xbox 360 games"),
these are **commented out, not deleted** (easy to restore / diff), all tagged `XENIA-ANDROID:`.
- **#8 ALSA audio:** the `factory.Add<ALSAAudioSystem>("alsa")` line was under `#if
  XE_PLATFORM_LINUX` (true on Android); changed to `&& !XE_PLATFORM_ANDROID` (matches how the
  SDL backend right below it was already guarded). Android uses the nop audio system for now
  (AAudio later).
- **#9 Discord rich presence:** 4 `if (cvars::discord) { discord::DiscordPresence::… }` blocks
  commented out — desktop status integration, meaningless on Android.
- **#10 Debug UI window:** the `if (cvars::debug) { … DebugWindow::Create(…) … }` block
  commented out — a desktop developer tool (xenia-debug-ui isn't built for Android).
- **Update risk:** **Low**, but these live in `xenia_main.cc` which we also touch elsewhere;
  re-apply when that file changes upstream.

### 11–12. AAudio audio backend (Android)
The nop audio system refuses to create a driver (returns NOT_IMPLEMENTED), so the guest's
XAudio client registration fails and the game stalls. We added a real AAudio output backend.
- **NEW (ours, not a canary edit really)** `src/xenia/apu/aaudio/aaudio_audio_system.{h,cc}` +
  `CMakeLists.txt`: ported from the old X360 project (BSD), adapted to Canary's AudioDriver
  interface (no Memory* in ctor; SubmitFrame takes a ready `float*` 6ch frame which we
  down-mix to stereo front L/R; added Pause/Resume/SetVolume + the 2nd CreateDriver overload).
  libaaudio is dlopen'd at runtime.
- **#11** `src/xenia/CMakeLists.txt`: `if(ANDROID) add_subdirectory(apu/aaudio)`.
- **#12** `src/xenia/app/xenia_main.cc`: include + `factory.Add<aaudio::AAudioAudioSystem>
  ("aaudio")` before nop so `apu=any` prefers it. EmulatorActivity sets `apu=aaudio`.
- **Update risk:** Low. The driver dir is ours; the two factory/CMake edits are tiny.

### Notes on patches we DELIBERATELY did NOT need
- **`platform.h`:** none — upstream already does `#elif defined(__ANDROID__) →
  XE_PLATFORM_ANDROID` (+ `XE_PLATFORM_LINUX`). (aX360e disabled this for a custom flag; we
  keep upstream's, so zero edits here.)
- **GTK3/X11 Linux deps:** none — that block is `elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")`
  and the NDK sets `CMAKE_SYSTEM_NAME=Android`, so it self-skips. Circular-dep link grouping
  and needed libs are handled in our **app** `CMakeLists` (outside Canary).

<!--
Template for new entries:

### N. `path/inside/canary`
- **Why:** <the Android-specific reason this edit is needed>
- **What:** <concise description of the change; reference the `XENIA-ANDROID:` tag>
- **Upstreamable?:** <yes/no — could this be contributed back?>
- **Update risk:** <low/med/high — how likely upstream churn collides with this>
-->
