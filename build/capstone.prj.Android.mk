LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := capstone
LOCAL_CPP_EXTENSION := .cc .cp .cxx .cpp .CPP .c++ .C
LOCAL_RS_EXTENSION := .rs .fs
ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Checked PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Debug PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=arm64-v8a CONFIGURATION=Release PLATFORM=Android-ARM64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
  LOCAL_CPP_FEATURES := exceptions rtti
  LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/.. \
    $(LOCAL_PATH)/../src \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/glslang \
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Checked PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
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
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Debug PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
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
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
else ifeq ($(filter-out ABI=$(TARGET_ARCH_ABI) $(PREMAKE_ANDROIDNDK_CONFIGURATIONS_PREFIXED) $(PREMAKE_ANDROIDNDK_PLATFORMS_PREFIXED),ABI=x86_64 CONFIGURATION=Release PLATFORM=Android-x86_64),)
  LOCAL_ARM_NEON := true
  LOCAL_CONLYFLAGS := \
    -D_UNICODE \
    -DUNICODE \
    -DNDEBUG \
    -D_NO_DEBUG_HEAP=1 \
    -DCAPSTONE_X86_ATT_DISABLE \
    -DCAPSTONE_DIET_NO \
    -DCAPSTONE_X86_REDUCE_NO \
    -DCAPSTONE_HAS_X86 \
    -DCAPSTONE_USE_SYS_DYN_MEM \
    -D_LIB \
    -fvisibility=hidden
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
    $(LOCAL_PATH)/../third_party/capstone \
    $(LOCAL_PATH)/../third_party/capstone/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libcapstone
  LOCAL_SRC_FILES := \
    ../third_party/capstone/MCInst.c \
    ../third_party/capstone/MCInstrDesc.c \
    ../third_party/capstone/MCRegisterInfo.c \
    ../third_party/capstone/SStream.c \
    ../third_party/capstone/arch/X86/X86ATTInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Disassembler.c \
    ../third_party/capstone/arch/X86/X86DisassemblerDecoder.c \
    ../third_party/capstone/arch/X86/X86IntelInstPrinter.c \
    ../third_party/capstone/arch/X86/X86Mapping.c \
    ../third_party/capstone/arch/X86/X86Module.c \
    ../third_party/capstone/cs.c \
    ../third_party/capstone/utils.c
  include $(BUILD_STATIC_LIBRARY)
endif