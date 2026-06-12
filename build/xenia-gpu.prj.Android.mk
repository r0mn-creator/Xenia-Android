LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-gpu
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
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
  LOCAL_MODULE_FILENAME := libxenia-gpu
  LOCAL_SRC_FILES := \
    ../src/xenia/gpu/command_processor.cc \
    ../src/xenia/gpu/draw_extent_estimator.cc \
    ../src/xenia/gpu/draw_util.cc \
    ../src/xenia/gpu/dxbc_shader.cc \
    ../src/xenia/gpu/dxbc_shader_translator.cc \
    ../src/xenia/gpu/dxbc_shader_translator_alu.cc \
    ../src/xenia/gpu/dxbc_shader_translator_fetch.cc \
    ../src/xenia/gpu/dxbc_shader_translator_memexport.cc \
    ../src/xenia/gpu/dxbc_shader_translator_om.cc \
    ../src/xenia/gpu/gpu_flags.cc \
    ../src/xenia/gpu/graphics_system.cc \
    ../src/xenia/gpu/packet_disassembler.cc \
    ../src/xenia/gpu/primitive_processor.cc \
    ../src/xenia/gpu/register_file.cc \
    ../src/xenia/gpu/registers.cc \
    ../src/xenia/gpu/render_target_cache.cc \
    ../src/xenia/gpu/sampler_info.cc \
    ../src/xenia/gpu/shader.cc \
    ../src/xenia/gpu/shader_interpreter.cc \
    ../src/xenia/gpu/shader_translator.cc \
    ../src/xenia/gpu/shader_translator_disasm.cc \
    ../src/xenia/gpu/shared_memory.cc \
    ../src/xenia/gpu/spirv_builder.cc \
    ../src/xenia/gpu/spirv_shader.cc \
    ../src/xenia/gpu/spirv_shader_translator.cc \
    ../src/xenia/gpu/spirv_shader_translator_alu.cc \
    ../src/xenia/gpu/spirv_shader_translator_fetch.cc \
    ../src/xenia/gpu/spirv_shader_translator_memexport.cc \
    ../src/xenia/gpu/spirv_shader_translator_rb.cc \
    ../src/xenia/gpu/texture_cache.cc \
    ../src/xenia/gpu/texture_dump.cc \
    ../src/xenia/gpu/texture_extent.cc \
    ../src/xenia/gpu/texture_info.cc \
    ../src/xenia/gpu/texture_info_formats.cc \
    ../src/xenia/gpu/texture_util.cc \
    ../src/xenia/gpu/trace_dump.cc \
    ../src/xenia/gpu/trace_player.cc \
    ../src/xenia/gpu/trace_reader.cc \
    ../src/xenia/gpu/trace_viewer.cc \
    ../src/xenia/gpu/trace_writer.cc \
    ../src/xenia/gpu/ucode.cc \
    ../src/xenia/gpu/xenos.cc
  LOCAL_STATIC_LIBRARIES := \
    dxbc \
    fmt \
    glslang-spirv \
    snappy \
    xenia-base \
    xxhash
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_STATIC_LIBRARY)
endif