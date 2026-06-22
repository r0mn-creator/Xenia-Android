# Xenia Android

A native **Xbox 360 emulator for Android**, built on the [Xenia-Canary](https://github.com/xenia-canary/xenia-canary)
engine and its mature **`a64` ARM64 JIT backend**, with a custom Android UI/UX.

> **Build codename:** "Falcon"

## Status

Early work-in-progress / research. The emulator **boots and runs commercial titles**
(e.g. Halo 3, Need for Speed: Carbon): the ARM64 JIT, Vulkan GPU command processing, AAudio
output, HID input, and the HLE kernel are all wired up and stable enough to run for minutes
at a time. The current focus is getting games past boot to **on-screen 3D content** — the
running debugging notes live in [`docs/`](docs/).

This is an open, "fork-and-improve" project. Contributions welcome.

## Requirements

- **Android 11 (API level 30) or newer** — required for the thread APIs the engine relies on.
- **64-bit ARM (arm64-v8a) only** — the whole point of this project is the ARM64 (`a64`) JIT;
  there is no 32-bit or x86 build.
- **A Vulkan-capable GPU.** A recent, powerful 64-bit ARM device is recommended — Xbox 360
  emulation is demanding.

## What this is (and why)

This replaces an earlier attempt that used a hand-written ARM64 JIT backend, whose codegen
bugs surfaced as endless deadlocks / crashes / black screens. Xenia-Canary's `a64` backend is
the upstream, battle-tested ARM64 translator — so we build on that instead, and keep our own UI.

> The old hand-written-backend version is preserved on the `legacy-handwritten-backend` branch.

## Project structure

```
app/                          Android app module — our UI, JNI glue, native build
  src/main/java/jp/xenia/...     Activities, on-screen controller, input handling
  src/main/cpp/                  JNI entry, CMakeLists, low-memory JIT reservation
patches/                      Our engine changes vs pinned Canary, as one git patch
scripts/setup-engine.sh       Fetches the pinned engine and applies the patch
docs/                         Architecture, build, and per-patch documentation
third_party/xenia-canary/     The engine (fetched by the setup script — not committed)
NOTICE                        Open-source license attributions
```

## Building

The Xenia-Canary engine source is **not committed here** (it's large upstream BSD code).
It is pinned to a specific commit and fetched + patched by a script:

```bash
# 1. Fetch the pinned engine and apply the Xenia Android patches
./scripts/setup-engine.sh

# 2. Build the APK (Android Studio, or from the CLI)
./gradlew :app:assembleDebug

# 3. Install
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Toolchain: Android SDK 34, NDK r27, CMake 3.22+. More in [`docs/BUILD.md`](docs/BUILD.md).

## How it was created

Xenia Android is assembled from clean upstream open source, with a deliberately small,
fully-documented set of changes — so the engine stays close to upstream and easy to update.

1. **Foundation.** It takes [Xenia-Canary](https://github.com/xenia-canary/xenia-canary)
   pinned at commit `e7d0e45` and its `a64` ARM64 JIT backend (built on `xbyak_aarch64`) — the
   upstream, well-exercised ARM64 translator — as the emulation core. The bulk of the engine
   (~95% plain C++) is reused unchanged.

2. **Minimal, documented engine patches.** Rather than forking the engine, every change to
   Canary is kept as a tiny, reviewable patch and logged in
   [`docs/CANARY_PATCHES.md`](docs/CANARY_PATCHES.md) (currently ~20 patches), then re-applied
   by the setup script. The patches add Android plumbing and fix Android-specific issues:
   - **AAudio** audio driver (`src/xenia/apu/aaudio/`) — native low-latency output, with the
     correct 6-channel big-endian → stereo conversion.
   - **Android HID** input driver (`src/xenia/hid/android/`) — drives the on-screen controller.
   - **Android windowing / JNI** glue and a Vulkan presenter on the Android surface.
   - **Threading** fixes for bionic (no robust mutexes; signal-based suspend/APC delivery; a
     thread-exit use-after-free fix).
   - **JIT code-cache placement** — relocating the fixed-address indirection table around
     Android's ART managed heap so launches are reliable.
   - **Kernel / HLE** fixes — a default offline player profile, graceful object handling, etc.

3. **Android app layer (`app/`).** A custom UI, JNI entry points, an on-screen controller
   overlay, content/storage roots under app-private storage, and the CMake build that compiles
   the engine plus our drivers into a single `libxeniaapp.so`.

4. **Iterative, AI-assisted development.** The port was driven through long, hands-on debugging
   sessions — boot bring-up, threading races, JIT/codegen, Vulkan command processing, and the
   HLE kernel — with the help of Claude Code. The investigation notes live under `docs/`.

No code from other Android Xbox 360 ports was used; the working result was reached
independently from the BSD/permissive upstream sources above.

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — how it's structured and why.
- [`docs/BUILD.md`](docs/BUILD.md) — how to build.
- [`docs/CANARY_PATCHES.md`](docs/CANARY_PATCHES.md) — every edit we make to the Canary engine
  (kept tiny and documented so updates stay easy).
- [`docs/UPDATING_CANARY.md`](docs/UPDATING_CANARY.md) — how to move to a newer Canary.

## Licensing

Built entirely from BSD/permissive open source. See [`NOTICE`](NOTICE) and [`LICENSE`](LICENSE).
The Canary engine is BSD-3-Clause (Ben Vanik & contributors); `xbyak_aarch64` is Apache-2.0.

## Legal

For use with software you legally own. This project ships no Xbox 360 games, BIOS, or other
proprietary content.
