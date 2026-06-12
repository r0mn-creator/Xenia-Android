/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_assembler.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <climits>
#include <atomic>
#include <string>

#include "xenia/base/logging.h"
#include "xenia/base/string_buffer.h"

#include "xenia/base/profiling.h"
#include "xenia/base/reset_scope.h"
#include "xenia/base/string.h"
#include "xenia/cpu/backend/arm64/arm64_backend.h"
#include "xenia/cpu/backend/arm64/arm64_code_cache.h"
#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/backend/arm64/arm64_function.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/cpu/hir/hir_builder.h"
#include "xenia/cpu/hir/label.h"
#include "xenia/cpu/processor.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

using xe::cpu::hir::HIRBuilder;

ARM64Assembler::ARM64Assembler(ARM64Backend* backend)
    : Assembler(backend), arm64_backend_(backend), capstone_handle_(0) {}

ARM64Assembler::~ARM64Assembler() {
  emitter_.reset();
}

bool ARM64Assembler::Initialize() {
  if (!Assembler::Initialize()) {
    return false;
  }
  emitter_ = std::make_unique<ARM64Emitter>(arm64_backend_);
  return true;
}

void ARM64Assembler::Reset() {
  string_buffer_.Reset();
  Assembler::Reset();
}

bool ARM64Assembler::Assemble(GuestFunction* function, HIRBuilder* builder,
                               uint32_t debug_info_flags,
                               std::unique_ptr<FunctionDebugInfo> debug_info) {
  SCOPE_profile_cpu_f("cpu");

  xe::make_reset_scope(this);

  // DIAGNOSTIC: dump the optimized HIR for the entry function so we can see the
  // spin loop and what it polls. Logged line-by-line because logcat truncates
  // long messages.
  // DIAGNOSTIC: dump optimized HIR for specific functions of interest (Error
  // level so it survives logd "chatty" throttling). Set the address(es) below.
  uint32_t fa = function->address();
  bool of_interest = false;
  if (of_interest) {
    StringBuffer sb;
    builder->Dump(&sb);
    XELOGE("==== HIR DUMP {:08X} BEGIN ====", function->address());
    const char* p = sb.buffer();
    std::string line;
    for (; *p; ++p) {
      if (*p == '\n') {
        XELOGE("HIR| {}", line);
        line.clear();
      } else {
        line.push_back(*p);
      }
    }
    if (!line.empty()) XELOGE("HIR| {}", line);
    XELOGE("==== HIR DUMP {:08X} END ====", function->address());
  }

  void* machine_code = nullptr;
  size_t code_size = 0;
  if (!emitter_->Emit(function, builder, debug_info_flags, debug_info.get(),
                      &machine_code, &code_size, &function->source_map())) {
    return false;
  }

  if (debug_info_flags & DebugInfoFlags::kDebugInfoDisasmMachineCode) {
    DumpMachineCode(machine_code, code_size, function->source_map(),
                    &string_buffer_);
    debug_info->set_machine_code_disasm(xe_strdup(string_buffer_.buffer()));
    string_buffer_.Reset();
  }

  function->set_debug_info(std::move(debug_info));

  static_cast<ARM64Function*>(function)->Setup(
      reinterpret_cast<uint8_t*>(machine_code), code_size);

  uint64_t host_address = reinterpret_cast<uint64_t>(machine_code);
  uint32_t host_offset = static_cast<uint32_t>(
      host_address - arm64_backend_->code_cache()->execute_base_address());
  reinterpret_cast<ARM64CodeCache*>(backend_->code_cache())
      ->AddIndirection(function->address(), host_offset);

  return true;
}

void ARM64Assembler::DumpMachineCode(
    void* machine_code, size_t code_size,
    const std::vector<SourceMapEntry>& source_map, StringBuffer* str) {
  // Disassembly not implemented (capstone not available in this build config).
  (void)machine_code;
  (void)code_size;
  (void)source_map;
  (void)str;
}

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
