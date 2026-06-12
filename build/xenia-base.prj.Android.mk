LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-base
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
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
    $(LOCAL_PATH)/../third_party/glslang
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-base
  LOCAL_SRC_FILES := \
    ../src/xenia/base/arena.cc \
    ../src/xenia/base/bit_map.cc \
    ../src/xenia/base/bit_stream.cc \
    ../src/xenia/base/byte_stream.cc \
    ../src/xenia/base/clock.cc \
    ../src/xenia/base/clock_posix.cc \
    ../src/xenia/base/clock_x64.cc \
    ../src/xenia/base/console_posix.cc \
    ../src/xenia/base/cvar.cc \
    ../src/xenia/base/cvar_android.cc \
    ../src/xenia/base/debugging_posix.cc \
    ../src/xenia/base/exception_handler.cc \
    ../src/xenia/base/exception_handler_posix.cc \
    ../src/xenia/base/filesystem.cc \
    ../src/xenia/base/filesystem_android.cc \
    ../src/xenia/base/filesystem_posix.cc \
    ../src/xenia/base/filesystem_wildcard.cc \
    ../src/xenia/base/fuzzy.cc \
    ../src/xenia/base/host_thread_context.cc \
    ../src/xenia/base/logging.cc \
    ../src/xenia/base/main_android.cc \
    ../src/xenia/base/mapped_memory_posix.cc \
    ../src/xenia/base/memory.cc \
    ../src/xenia/base/memory_posix.cc \
    ../src/xenia/base/mutex.cc \
    ../src/xenia/base/profiling.cc \
    ../src/xenia/base/ring_buffer.cc \
    ../src/xenia/base/string.cc \
    ../src/xenia/base/string_buffer.cc \
    ../src/xenia/base/system_android.cc \
    ../src/xenia/base/threading.cc \
    ../src/xenia/base/threading_posix.cc \
    ../src/xenia/base/threading_timer_queue.cc \
    ../src/xenia/base/utf8.cc \
    ../src/xenia/base/vec128.cc
  LOCAL_STATIC_LIBRARIES := \
    fmt
  include $(BUILD_STATIC_LIBRARY)
endif