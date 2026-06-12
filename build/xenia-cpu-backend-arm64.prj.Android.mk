# Android NDK makefile for the new full ARM64 JIT backend (xenia-cpu-backend-arm64).
# Uses Dolphin's Arm64Emitter as the assembler layer.
# Only built for arm64-v8a — x86_64 uses xenia-cpu-backend-x64.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-cpu-backend-arm64
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs

# --- Checked arm64-v8a ---
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -fvisibility=hidden \
    -std=c++20
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/arm64emitter \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-arm64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/arm64/arm64_assembler.cc \
    ../src/xenia/cpu/backend/arm64/arm64_backend.cc \
    ../src/xenia/cpu/backend/arm64/arm64_code_cache.cc \
    ../src/xenia/cpu/backend/arm64/arm64_emitter.cc \
    ../src/xenia/cpu/backend/arm64/arm64_function.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part1.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part2.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part3.cc
  LOCAL_STATIC_LIBRARIES := \
    arm64emitter \
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
    -std=c++20
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/arm64emitter \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-arm64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/arm64/arm64_assembler.cc \
    ../src/xenia/cpu/backend/arm64/arm64_backend.cc \
    ../src/xenia/cpu/backend/arm64/arm64_code_cache.cc \
    ../src/xenia/cpu/backend/arm64/arm64_emitter.cc \
    ../src/xenia/cpu/backend/arm64/arm64_function.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part1.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part2.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part3.cc
  LOCAL_STATIC_LIBRARIES := \
    arm64emitter \
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
    -std=c++20
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/arm64emitter \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-arm64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/arm64/arm64_assembler.cc \
    ../src/xenia/cpu/backend/arm64/arm64_backend.cc \
    ../src/xenia/cpu/backend/arm64/arm64_code_cache.cc \
    ../src/xenia/cpu/backend/arm64/arm64_emitter.cc \
    ../src/xenia/cpu/backend/arm64/arm64_function.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part1.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part2.cc \
    ../src/xenia/cpu/backend/arm64/arm64_sequences_part3.cc
  LOCAL_STATIC_LIBRARIES := \
    arm64emitter \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
endif
