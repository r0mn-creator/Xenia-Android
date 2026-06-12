LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-gpu-shader-compiler
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
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
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
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
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
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
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := xenia-gpu-shader-compiler
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/gpu/shader_compiler_main.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
endif