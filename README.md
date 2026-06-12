# Xenia Android

Xbox 360 emulator ported to Android. Based on the [Xenia](https://github.com/xenia-project/xenia) emulator with a custom ARM64 JIT backend to run directly on Android devices.

---

## What Works

| Component | Status |
|-----------|--------|
| CPU (PowerPC → ARM64 JIT) | Working |
| Memory / virtual address space | Working |
| Kernel (threads, mutexes, events) | Working |
| Audio (AAudio driver) | Working |
| Vulkan GPU | Partial — presents frames (black screen TBD) |
| On-screen gamepad overlay | Working |
| Physical controller support | Working |
| File I/O (ISO / XEX) | Working |

---

## Requirements

- Android 8.0+ (API 26)
- Device with ARM64 processor (Snapdragon, Dimensity, Exynos — tested on Snapdragon 8 Gen 2)
- Vulkan 1.1+ GPU driver
- ~2 GB RAM free

---

## Build

### Prerequisites

- Android NDK r25c or newer
- Android Studio (for the Java/Kotlin UI layer)
- CMake 3.20+

### Steps

```bash
# Clone the repo
git clone https://github.com/r0mn-creator/Xenia-Android.git
cd Xenia-Android

# Build the native library
cd android/android_studio_project
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=app/jni/Android.mk

# Open in Android Studio and build the APK
# Or from the command line:
./gradlew assembleGithubDebug
```

The APK will be at:
```
app/build/outputs/apk/github/debug/app-github-debug.apk
```

### Install and Run

```bash
adb install app/build/outputs/apk/github/debug/app-github-debug.apk

# Launch with a game (ISO must be on the device)
adb shell am start \
  -n jp.xenia.emulator.github.debug/jp.xenia.emulator.EmulatorActivity \
  --es jp.xenia.emulator.EmulatorActivity.GAME_PATH "/sdcard/Download/game.iso"
```

---

## Project Structure

```
android/android_studio_project/   Android UI (Java + Gradle)
src/xenia/
  cpu/backend/arm64/              ARM64 JIT backend (core of the port)
  apu/aaudio/                     Android audio driver (AAudio)
  hid/android/                    Android HID input driver
  kernel/xboxkrnl/                Xbox 360 kernel emulation
  gpu/vulkan/                     Vulkan renderer
```

---

## How It Works

The emulator translates Xbox 360 PowerPC instructions into ARM64 machine code at runtime (JIT compilation). Each guest function is compiled once and cached. The ARM64 backend was written from scratch for this port — the original Xenia only supported x86-64.

Key pieces:
- **JIT** — `src/xenia/cpu/backend/arm64/` translates PPC HIR → ARM64
- **Kernel** — `src/xenia/kernel/` implements Xbox 360 system calls in C++
- **GPU** — Vulkan command processor + texture cache + SPIR-V shader translation
- **Audio** — AAudio driver with Xbox 360 surround → stereo downmix

---

## Known Issues

- **Black screen** — GPU command processor produces frames but nothing renders yet
- Some games crash due to unimplemented kernel calls
- Not all PowerPC instructions are implemented

---

## Based On

- [Xenia](https://github.com/xenia-project/xenia) by Ben Vanik and contributors (BSD license)
- ARM64 JIT, Android UI, audio, and input layers added in this fork
