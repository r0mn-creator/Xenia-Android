LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-cpu-backend-x64
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -DXBYAK_NO_OP_NAMES \
    -DXBYAK_ENABLE_OMITTED_OPERAND \
    -fvisibility=hidden \
    -std=c++17
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
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
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
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
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CPPFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -D_LIB \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
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
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu-backend-x64
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/x64/x64_assembler.cc \
    ../src/xenia/cpu/backend/x64/x64_backend.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache.cc \
    ../src/xenia/cpu/backend/x64/x64_code_cache_posix.cc \
    ../src/xenia/cpu/backend/x64/x64_emitter.cc \
    ../src/xenia/cpu/backend/x64/x64_function.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_control.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_memory.cc \
    ../src/xenia/cpu/backend/x64/x64_seq_vector.cc \
    ../src/xenia/cpu/backend/x64/x64_sequences.cc \
    ../src/xenia/cpu/backend/x64/x64_tracers.cc
  LOCAL_STATIC_LIBRARIES := \
    capstone \
    fmt \
    xenia-base \
    xenia-cpu
  include $(BUILD_STATIC_LIBRARY)
endif