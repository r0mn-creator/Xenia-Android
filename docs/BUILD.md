# Building Xenia Android

## Prerequisites
- Android SDK with **NDK 27** (27.0.12077973) — required for `std::jthread` / newer libc++.
- CMake 3.22+ (the gradle CMake build uses it).
- JDK 11+. A device/emulator on **Android 11 (API 30)+**, arm64-v8a.
- `third_party/xenia-canary` submodule populated.

## Build
```
./gradlew :app:assembleDebug
```
Output: `app/build/outputs/apk/debug/app-debug.apk`.

## Notes
- arm64-v8a only (the a64 JIT is the point).
- The native build compiles the Canary engine via CMake (see app/src/main/cpp/CMakeLists.txt)
  and links it into libxenia-app.so.
- Engine edits are tracked in docs/CANARY_PATCHES.md.
- gradle's native up-to-date check is unreliable; if a change doesn't take, delete
  `app/.cxx` and `app/build` and rebuild.
