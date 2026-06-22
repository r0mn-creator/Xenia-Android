# Xenia Android — Architecture

App name: **Xenia Android**. Build codename: **"Falcon"**.

## What this project is

A native Xbox 360 emulator for Android, built on the **Xenia-Canary** engine using its
mature **`a64` ARM64 JIT backend**, with our own Android UI/UX on top.

It replaces an earlier attempt that used a hand-written `arm64` JIT backend; that backend
had codegen bugs that surfaced as endless deadlocks/crashes/black-screens. Canary's `a64`
backend is the upstream, battle-tested ARM64 translator and is the foundation here.

## Top-level layout

```
Xenia-Android/
├── app/                         # Android app module (OURS)
│   ├── build.gradle             # gradle build (CMake-based native build)
│   └── src/main/
│       ├── java/jp/xenia/...    # our UI (game grid, box art, emulator, controls)
│       ├── cpp/                 # OUR app-level native code:
│       │   ├── CMakeLists.txt   #   builds the JNI shared lib, links Canary
│       │   └── ...              #   windowed-app JNI glue + our additions
│       └── res/                 # our layouts/drawables
├── third_party/
│   └── xenia-canary/            # PRISTINE Canary engine (git submodule, pinned)
├── docs/
│   ├── ARCHITECTURE.md          # this file
│   ├── CANARY_PATCHES.md        # registry of every edit inside Canary (KEY for updates)
│   ├── BUILD.md                 # how to build
│   └── UPDATING_CANARY.md       # how to move to a newer Canary
├── NOTICE                       # license attributions
└── README.md
```

## The core design principle: isolate from Canary

Updating to the latest Canary should be a small chore, not a rewrite. We achieve that by
keeping **~all** of our work *outside* Canary's tree:

- **Our UI** → `app/src/main/java` + `res` (Canary never touches these).
- **Our native build + JNI glue** → `app/src/main/cpp` (a thin layer that *links* Canary's
  libraries; it does not modify them).
- **Unavoidable edits to Canary itself** (a handful: Android platform/CMake support) → kept
  minimal, tagged `XENIA-ANDROID:`, and logged in `CANARY_PATCHES.md`.

On update, only the small logged set of in-Canary edits can conflict. See
`docs/UPDATING_CANARY.md`.

## Why Canary doesn't "just build" for Android

Canary's CMake targets desktop only (Windows/macOS/Linux); its old premake→ndk-build Android
path is deprecated. So we add the missing Android build support ourselves:

1. Minimal patches to Canary so its CMake/platform layer understands Android/arm64 → `a64`.
2. An app-level `CMakeLists` (ours) that builds a **JNI shared library** linking Canary's
   static libraries plus the windowed-app Android glue our UI calls.
3. Host-side shader compilation (Xenia generates SPIR-V at build time).

## JNI contract (why our UI fits Canary unchanged)

Our UI is a fork of Xenia's official Android app. Canary still ships the same windowed-app
framework our UI drives — `Java_jp_xenia_emulator_WindowedAppActivity_*` (initialize, surface
changed, paint, motion event…). We add three of our own native hooks
(`onAndroidSuspend`, `onAndroidResume`, `injectControllerInputNative`) in our app-level glue.

## Licensing

All shipped code is BSD/permissive — see `NOTICE`. We do **not** use aX360e's authored glue
(mixed/unclear licensing); we reached the same result from clean sources. The Canary engine
is BSD (Ben Vanik); xbyak_aarch64 is Apache-2.0; libadrenotools (if added) is BSD-2-Clause.
