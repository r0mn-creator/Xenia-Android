LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := mspack
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DDEBUG \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DDEBUG \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DDEBUG \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DDEBUG \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden
  LOCAL_CPPFLAGS := \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -D_LIB \
    -DHAVE_CONFIG_H \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CONLYFLAGS += \
    -mavx
  LOCAL_CPPFLAGS += \
    -mavx
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/mspack
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libmspack
  LOCAL_SRC_FILES := \
    ../third_party/mspack/logging.cc \
    ../third_party/mspack/lzxd.c \
    ../third_party/mspack/system.c
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  include $(BUILD_STATIC_LIBRARY)
endif