LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-ui-vulkan
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
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
  LOCAL_MODULE_FILENAME := libxenia-ui-vulkan
  LOCAL_SRC_FILES := \
    ../src/xenia/ui/vulkan/linked_type_descriptor_set_allocator.cc \
    ../src/xenia/ui/vulkan/single_layout_descriptor_set_pool.cc \
    ../src/xenia/ui/vulkan/spirv_tools_context.cc \
    ../src/xenia/ui/vulkan/ui_samplers.cc \
    ../src/xenia/ui/vulkan/vulkan_device.cc \
    ../src/xenia/ui/vulkan/vulkan_gpu_completion_timeline.cc \
    ../src/xenia/ui/vulkan/vulkan_immediate_drawer.cc \
    ../src/xenia/ui/vulkan/vulkan_instance.cc \
    ../src/xenia/ui/vulkan/vulkan_mem_alloc.cc \
    ../src/xenia/ui/vulkan/vulkan_presenter.cc \
    ../src/xenia/ui/vulkan/vulkan_provider.cc \
    ../src/xenia/ui/vulkan/vulkan_upload_buffer_pool.cc \
    ../src/xenia/ui/vulkan/vulkan_util.cc \
    ../src/xenia/ui/vulkan/vulkan_window_demo.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
endif