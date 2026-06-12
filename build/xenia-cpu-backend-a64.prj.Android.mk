# Android NDK makefile for the AArch64 JIT backend (xenia-cpu-backend-a64).
#
# This module is the AArch64 equivalent of xenia-cpu-backend-x64.
# It is built only for arm64-v8a targets.
#
# TROUBLESHOOTING:
#   - If you get "undefined reference to xe::cpu::backend::a64::A64Backend" in
#     the xenia-app link step, verify that xenia-cpu-backend-a64 appears in
#     LOCAL_STATIC_LIBRARIES inside xenia-app.prj.Android.mk.
#   - If you see "cannot open file 'libxenia-cpu-backend-a64.a'", the module
#     condition below (ABI=arm64-v8a) may not match your build configuration.
#     Check that TARGET_ARCH_ABI is arm64-v8a.
#   - The a64 backend source files use Linux-specific syscalls (memfd_create,
#     mmap with PROT_EXEC).  Do not build this module for x86_64 Android.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-cpu-backend-a64
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs

# --- Checked (debug with assertions) arm64-v8a ---
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-a64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/a64/a64_assembler.cc \
    ../src/xenia/cpu/backend/a64/a64_backend.cc \
    ../src/xenia/cpu/backend/a64/a64_code_cache.cc \
    ../src/xenia/cpu/backend/a64/a64_function.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)

# --- Debug arm64-v8a ---
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-a64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/a64/a64_assembler.cc \
    ../src/xenia/cpu/backend/a64/a64_backend.cc \
    ../src/xenia/cpu/backend/a64/a64_code_cache.cc \
    ../src/xenia/cpu/backend/a64/a64_function.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)

# --- Release arm64-v8a ---
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -O2 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-a64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/a64/a64_assembler.cc \
    ../src/xenia/cpu/backend/a64/a64_backend.cc \
    ../src/xenia/cpu/backend/a64/a64_code_cache.cc \
    ../src/xenia/cpu/backend/a64/a64_function.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
endif
