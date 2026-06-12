LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_RENDERSCRIPT_COMPATIBILITY :=
LOCAL_MODULE := xenia-cpu
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
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
    $(LOCAL_PATH)/../third_party/llvm/include
  LOCAL_C_INCLUDES := $(call PREMAKE_ANDROIDNDK_SHELL_ESCAPE,$(LOCAL_C_INCLUDES))
  LOCAL_MODULE_FILENAME := libxenia-cpu
  LOCAL_SRC_FILES := \
    ../src/xenia/cpu/backend/assembler.cc \
    ../src/xenia/cpu/backend/backend.cc \
    ../src/xenia/cpu/backend/null_backend.cc \
    ../src/xenia/cpu/breakpoint.cc \
    ../src/xenia/cpu/compiler/compiler.cc \
    ../src/xenia/cpu/compiler/compiler_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_pass.cc \
    ../src/xenia/cpu/compiler/passes/conditional_group_subpass.cc \
    ../src/xenia/cpu/compiler/passes/constant_propagation_pass.cc \
    ../src/xenia/cpu/compiler/passes/context_promotion_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/control_flow_simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/data_flow_analysis_pass.cc \
    ../src/xenia/cpu/compiler/passes/dead_code_elimination_pass.cc \
    ../src/xenia/cpu/compiler/passes/finalization_pass.cc \
    ../src/xenia/cpu/compiler/passes/memory_sequence_combination_pass.cc \
    ../src/xenia/cpu/compiler/passes/register_allocation_pass.cc \
    ../src/xenia/cpu/compiler/passes/simplification_pass.cc \
    ../src/xenia/cpu/compiler/passes/validation_pass.cc \
    ../src/xenia/cpu/compiler/passes/value_reduction_pass.cc \
    ../src/xenia/cpu/cpu_flags.cc \
    ../src/xenia/cpu/elf_module.cc \
    ../src/xenia/cpu/entry_table.cc \
    ../src/xenia/cpu/export_resolver.cc \
    ../src/xenia/cpu/function.cc \
    ../src/xenia/cpu/function_debug_info.cc \
    ../src/xenia/cpu/hir/block.cc \
    ../src/xenia/cpu/hir/hir_builder.cc \
    ../src/xenia/cpu/hir/instr.cc \
    ../src/xenia/cpu/hir/opcodes.cc \
    ../src/xenia/cpu/hir/value.cc \
    ../src/xenia/cpu/lzx.cc \
    ../src/xenia/cpu/mmio_handler.cc \
    ../src/xenia/cpu/module.cc \
    ../src/xenia/cpu/ppc/ppc_context.cc \
    ../src/xenia/cpu/ppc/ppc_emit_altivec.cc \
    ../src/xenia/cpu/ppc/ppc_emit_alu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_control.cc \
    ../src/xenia/cpu/ppc/ppc_emit_fpu.cc \
    ../src/xenia/cpu/ppc/ppc_emit_memory.cc \
    ../src/xenia/cpu/ppc/ppc_frontend.cc \
    ../src/xenia/cpu/ppc/ppc_hir_builder.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_disasm_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_info.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_lookup_gen.cc \
    ../src/xenia/cpu/ppc/ppc_opcode_table_gen.cc \
    ../src/xenia/cpu/ppc/ppc_scanner.cc \
    ../src/xenia/cpu/ppc/ppc_translator.cc \
    ../src/xenia/cpu/processor.cc \
    ../src/xenia/cpu/raw_module.cc \
    ../src/xenia/cpu/stack_walker_posix.cc \
    ../src/xenia/cpu/test_module.cc \
    ../src/xenia/cpu/thread.cc \
    ../src/xenia/cpu/thread_state.cc \
    ../src/xenia/cpu/xex_module.cc
  LOCAL_STATIC_LIBRARIES := \
    xenia-base \
    mspack
  include $(BUILD_STATIC_LIBRARY)
endif