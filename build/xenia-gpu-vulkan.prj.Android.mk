LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-gpu-vulkan
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
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
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
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
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/Vulkan-Headers/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/vulkan/deferred_command_buffer.cc \
    ../src/xenia/gpu/vulkan/vulkan_command_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_graphics_system.cc \
    ../src/xenia/gpu/vulkan/vulkan_pipeline_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_primitive_processor.cc \
    ../src/xenia/gpu/vulkan/vulkan_render_target_cache.cc \
    ../src/xenia/gpu/vulkan/vulkan_shader.cc \
    ../src/xenia/gpu/vulkan/vulkan_shared_memory.cc \
    ../src/xenia/gpu/vulkan/vulkan_texture_cache.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt \
    glslang-spirv \
    xenia-base \
    xenia-gpu \
    xenia-ui-vulkan \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
endif