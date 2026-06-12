LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-cpu-tests
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-ARM64/Checked
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-ARM64/Debug
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-ARM64/Release
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-x86_64/Checked
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
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
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-x86_64/Debug
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
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
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  PREMAKE_ANDROIDNDK_TEMPORARY_1 := $(LOCAL_PATH)/bin/Android-x86_64/Release
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DXE_TEST_SUITE_NAME=\"xenia-cpu-tests\" \
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
    $(LOCAL_PATH)/../tools/build \
    $(LOCAL_PATH)/../tools/build/src \
    $(LOCAL_PATH)/../tools/build/third_party/catch/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
  LOCAL_LDLIBS := \
    -L"$(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_PRE_QUOTES,$(PREMAKE_ANDROIDNDK_TEMPORARY_1))" \
    -landroid \
    -ldl \
    -llog
  LOCAL_LDLIBS := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE_POST_QUOTES,$(LOCAL_LDLIBS))
  LOCAL_MODULE_FILENAME := xenia-cpu-tests
  LOCAL_SRC_FILES := \
    ../src/xenia/base/console_app_main_android.cc \
    ../src/xenia/cpu/testing/add_test.cc \
    ../src/xenia/cpu/testing/byte_swap_test.cc \
    ../src/xenia/cpu/testing/extract_test.cc \
    ../src/xenia/cpu/testing/insert_test.cc \
    ../src/xenia/cpu/testing/load_vector_shl_shr_test.cc \
    ../src/xenia/cpu/testing/pack_test.cc \
    ../src/xenia/cpu/testing/permute_test.cc \
    ../src/xenia/cpu/testing/sha_test.cc \
    ../src/xenia/cpu/testing/shl_test.cc \
    ../src/xenia/cpu/testing/shr_test.cc \
    ../src/xenia/cpu/testing/swizzle_test.cc \
    ../src/xenia/cpu/testing/unpack_test.cc \
    ../src/xenia/cpu/testing/vector_add_test.cc \
    ../src/xenia/cpu/testing/vector_max_test.cc \
    ../src/xenia/cpu/testing/vector_min_test.cc \
    ../src/xenia/cpu/testing/vector_rotate_left_test.cc \
    ../src/xenia/cpu/testing/vector_sha_test.cc \
    ../src/xenia/cpu/testing/vector_shl_test.cc \
    ../src/xenia/cpu/testing/vector_shr_test.cc \
    ../tools/build/src/test_suite_main.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-core \
    xenia-cpu \
    xenia-kernel \
    xenia-cpu-backend-x64
  LOCAL_WHOLE_STATIC_LIBRARIES := \
    xenia-ui
  include $(BUILD_EXECUTABLE)
endif