/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/backend/arm64/arm64_backend.h"

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <set>
#include <string>

#include "xenia/base/exception_handler.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/backend/arm64/arm64_assembler.h"
#include "xenia/cpu/backend/arm64/arm64_code_cache.h"
#include "xenia/cpu/backend/arm64/arm64_emitter.h"
#include "xenia/cpu/backend/arm64/arm64_function.h"
#include "xenia/cpu/backend/arm64/arm64_sequences.h"
#include "xenia/cpu/processor.h"

// For sys/auxv / getauxval on Android to detect CPU features
#if XE_PLATFORM_ANDROID
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

ARM64Backend::ARM64Backend() : Backend() {}

ARM64Backend::~ARM64Backend() {
  // ARM64Emitter const data was allocated during Initialize; free it here.
  // (Mirrors X64Emitter::FreeConstData pattern.)
}

bool ARM64Backend::Initialize(Processor* processor) {
  if (!Backend::Initialize(processor)) {
    return false;
  }

  // Detect optional CPU features via HWCAP on Android/Linux.
  // These control which optional NEON/ARMv8 extensions we can use.
  // The emitter reads feature flags from the backend at emit time.
#if XE_PLATFORM_ANDROID || XE_PLATFORM_GNU_LINUX
  unsigned long hwcap = getauxval(AT_HWCAP);
  unsigned long hwcap2 = getauxval(AT_HWCAP2);
  (void)hwcap;
  (void)hwcap2;
  // HWCAP_FPHP  = FP16 arithmetic
  // HWCAP_ASIMDHP = Advanced SIMD FP16
  // HWCAP2_SVEI = SVE (unlikely on mobile, skip)
  // Feature flags are passed to the emitter via machine_info_.
#endif

  // Describe the register file to the shared register allocator.
  // This tells the HIR compiler how many registers are available.
  auto& gprs = machine_info_.register_sets[0];
  gprs.id = 0;
  std::strcpy(gprs.name, "gpr");
  gprs.types = MachineInfo::RegisterSet::INT_TYPES;
  gprs.count = ARM64Emitter::GPR_COUNT;

  auto& vecs = machine_info_.register_sets[1];
  vecs.id = 1;
  std::strcpy(vecs.name, "vec");
  vecs.types = MachineInfo::RegisterSet::FLOAT_TYPES |
               MachineInfo::RegisterSet::VEC_TYPES;
  vecs.count = ARM64Emitter::VEC_COUNT;

  // Extended load/store: ARM64 has native load/store with byte-reverse
  // (LDRB/STRB with explicit byte-swap via REV). We always support it.
  machine_info_.supports_extended_load_store = true;

  // Create the code cache (manages the JIT executable memory region).
  code_cache_ = ARM64CodeCache::Create();
  Backend::code_cache_ = code_cache_.get();
  if (!code_cache_->Initialize()) {
    XELOGE("ARM64Backend: failed to initialize code cache");
    return false;
  }

  // Emit the host<->guest crossing thunks. These are tiny trampolines that
  // handle the ABI boundary and live at fixed addresses in the code cache.
  if (!EmitThunks()) {
    XELOGE("ARM64Backend: failed to emit crossing thunks");
    return false;
  }

  XELOGI("ARM64Backend: initialized (GPR={}, VEC={})", ARM64Emitter::GPR_COUNT,
         ARM64Emitter::VEC_COUNT);
  return true;
}

bool ARM64Backend::EmitThunks() {
  // The thunks are emitted into this emitter's executable buffer and called
  // directly (not copied into the code cache), so the emitter must outlive the
  // backend — it's stored as a member, not a local. The thunks are small
  // (< 1KB combined) so a tiny block suffices.
  thunk_emitter_ = std::make_unique<ARM64Emitter>(this, /*block_size=*/4096);
  ARM64Emitter* emitter = thunk_emitter_.get();

  // HostToGuestThunk: C++ → JIT'd guest code
  //   Signature: void* thunk(void* target, void* arg0, void* arg1)
  //   x0 = target (compiled guest function pointer)
  //   x1 = arg0   (ThreadState*)
  //   x2 = arg1   (return address)
  {
    void* code = nullptr;
    size_t code_size = 0;
    if (!emitter->EmitHostToGuestThunk()) {
      return false;
    }
    // The emitter writes to its buffer; we retrieve the address below.
    // (Actual implementation in arm64_emitter.cc)
  }

  // GuestToHostThunk: JIT'd guest code → C++ helper
  {
    if (!emitter->EmitGuestToHostThunk()) {
      return false;
    }
  }

  // ResolveFunctionThunk: lazy JIT compilation on first call
  {
    if (!emitter->EmitResolveFunctionThunk()) {
      return false;
    }
  }

  // Flush the instruction cache for the freshly emitted thunks. They are
  // executed in-place from this emitter's buffer (not copied through the code
  // cache, which would otherwise clear the cache), so the I-cache must be made
  // coherent with the D-cache writes before the thunks are first called.
  emitter->FlushIcache();

  // Retrieve thunk addresses from emitter after emission.
  // (Set by emitter during PlaceHostCode calls)
  // These are set as side effects of the Emit*Thunk calls above.
  host_to_guest_thunk_ = emitter->host_to_guest_thunk_ptr_;
  guest_to_host_thunk_ = emitter->guest_to_host_thunk_ptr_;
  resolve_function_thunk_ = emitter->resolve_function_thunk_ptr_;

  // Diagnostic: dump which opcode numbers are registered in the sequence table
  // and how many total entries exist. This distinguishes a registration bug
  // from genuinely missing per-type variants.
  {
    std::set<int> ops;
    for (const auto& kv : sequence_table()) {
      ops.insert(static_cast<int>(kv.first & 0xFF));
    }
    std::string op_list;
    for (int op : ops) op_list += std::to_string(op) + ",";
    XELOGI("ARM64 sequence_table: {} entries, {} distinct opcodes",
           sequence_table().size(), ops.size());
  }

  return true;
}

void ARM64Backend::CommitExecutableRange(uint32_t guest_low,
                                         uint32_t guest_high) {
  code_cache_->CommitExecutableRange(guest_low, guest_high);
}

std::unique_ptr<Assembler> ARM64Backend::CreateAssembler() {
  return std::make_unique<ARM64Assembler>(this);
}

std::unique_ptr<GuestFunction> ARM64Backend::CreateGuestFunction(
    Module* module, uint32_t address) {
  return std::make_unique<ARM64Function>(module, address);
}

uint64_t ARM64Backend::CalculateNextHostInstruction(
    ThreadDebugInfo* thread_info, uint64_t current_pc) {
  // Walk forward one ARM64 instruction (always 4 bytes, fixed-width).
  // For branches, decode the target. This is used by the debugger.
  // Minimal implementation: assume linear execution (no branch decoding).
  // A full implementation would decode B, BL, CBZ, CBNZ, TBZ, TBNZ, BR, BLR.
  return current_pc + 4;
}

void ARM64Backend::InstallBreakpoint(Breakpoint* breakpoint) {
  // Insert a BRK #0 (software breakpoint) at the given host PC.
  // ARM64 BRK encoding: 0xD4200000 | (imm16 << 5)
  // BRK #0 = 0xD4200000
  // TODO: implement when debug infrastructure is wired up.
}

void ARM64Backend::InstallBreakpoint(Breakpoint* breakpoint, Function* fn) {
  // TODO: implement.
}

void ARM64Backend::UninstallBreakpoint(Breakpoint* breakpoint) {
  // Restore the original instruction that was overwritten by BRK.
  // TODO: implement.
}

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
