LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-app
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    arm64emitter
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-cpu-backend-arm64 \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    arm64emitter
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-cpu-backend-arm64 \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    arm64emitter
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-cpu-backend-arm64 \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -landroid \
    -ldl \
    -llog
  LOCAL_MODULE_FILENAME := libxenia-app
  LOCAL_SRC_FILES := \
    ../src/xenia/app/discord/discord_presence.cc \
    ../src/xenia/app/emulator_window.cc \
    ../src/xenia/app/xenia_main.cc \
    ../src/xenia/base/main_init_android.cc \
    ../src/xenia/ui/windowed_app_main_android.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-apu \
    xenia-apu-nop \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-gpu \
    xenia-gpu-null \
    xenia-gpu-vulkan \
    xenia-hid \
    xenia-hid-android \
    xenia-hid-nop \
    xenia-kernel \
    xenia-ui-vulkan \
    xenia-vfs \
    aes_128 \
    capstone \
    fmt \
    dxbc \
    discord-rpc \
    glslang-spirv \
    imgui \
    libavcodec \
    libavutil \
    mspack \
    snappy \
    xxhash \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui \
    xenia-gpu-vulkan-trace-viewer \
    xenia-hid-demo \
    xenia-ui-window-vulkan-demo
  include $(BUILD_SHARED_LIBRARY)
endif