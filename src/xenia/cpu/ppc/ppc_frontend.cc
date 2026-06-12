/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/ppc/ppc_frontend.h"

#include <atomic>

#include "xenia/base/atomic.h"
#include "xenia/base/logging.h"
#include "xenia/cpu/ppc/ppc_context.h"
#include "xenia/cpu/ppc/ppc_emit.h"
#include "xenia/cpu/ppc/ppc_opcode_info.h"
#include "xenia/cpu/ppc/ppc_translator.h"
#include "xenia/cpu/processor.h"

namespace xe {
namespace cpu {
namespace ppc {

void InitializeIfNeeded();
void CleanupOnShutdown();

void InitializeIfNeeded() {
  static bool has_initialized = false;
  if (has_initialized) {
    return;
  }
  has_initialized = true;

  RegisterEmitCategoryAltivec();
  RegisterEmitCategoryALU();
  RegisterEmitCategoryControl();
  RegisterEmitCategoryFPU();
  RegisterEmitCategoryMemory();

  atexit(CleanupOnShutdown);
}

void CleanupOnShutdown() {}

PPCFrontend::PPCFrontend(Processor* processor) : processor_(processor) {
  InitializeIfNeeded();
}

PPCFrontend::~PPCFrontend() {
  // Force cleanup now before we deinit.
  translator_pool_.Reset();
}

Memory* PPCFrontend::memory() const { return processor_->memory(); }

// DIAGNOSTIC: expected builtin args, captured at PPCFrontend::Initialize.
// An early-boot SEGV_ACCERR was seen INSIDE CheckGlobalLock dereferencing a
// garbage arg1 (heap-tagged ptr) — but every JIT call site materializes
// arg0/arg1 as immediates from the BuiltinFunction, so they "can't" be wrong.
// This guard tells us definitively whether the handlers are being entered with
// corrupted args (JIT call-path bug) or correct args (memory-perms problem),
// and survives instead of crashing/corrupting.
static void* g_expected_lock_arg0 = nullptr;
static void* g_expected_lock_arg1 = nullptr;

static bool CheckLockArgs(const char* who, PPCContext* ppc_context, void* arg0,
                          void* arg1) {
  if (arg0 == g_expected_lock_arg0 && arg1 == g_expected_lock_arg1) {
    return true;
  }
  static std::atomic<int> logs{0};
  if (logs.fetch_add(1, std::memory_order_relaxed) < 20) {
    XELOGE(
        "{} BAD ARGS arg0={} (want {}) arg1={} (want {}) ctx={} lr={:08X} "
        "r13={:08X}",
        who, arg0, g_expected_lock_arg0, arg1, g_expected_lock_arg1,
        reinterpret_cast<void*>(ppc_context),
        ppc_context ? (uint32_t)ppc_context->lr : 0u,
        ppc_context ? (uint32_t)ppc_context->r[13] : 0u);
  }
  return false;
}

// Checks the state of the global lock and sets scratch to the current MSR
// value.
void CheckGlobalLock(PPCContext* ppc_context, void* arg0, void* arg1) {
  if (!CheckLockArgs("CheckGlobalLock", ppc_context, arg0, arg1)) {
    ppc_context->scratch = 0x8000;  // pretend unlocked; don't touch garbage
    return;
  }
  auto global_mutex = reinterpret_cast<std::recursive_mutex*>(arg0);
  auto global_lock_count = reinterpret_cast<int32_t*>(arg1);
  std::lock_guard<std::recursive_mutex> lock(*global_mutex);
  ppc_context->scratch = *global_lock_count ? 0 : 0x8000;
}

// Enters the global lock. Safe to recursion.
void EnterGlobalLock(PPCContext* ppc_context, void* arg0, void* arg1) {
  if (!CheckLockArgs("EnterGlobalLock", ppc_context, arg0, arg1)) {
    return;
  }
  auto global_mutex = reinterpret_cast<std::recursive_mutex*>(arg0);
  auto global_lock_count = reinterpret_cast<int32_t*>(arg1);
  global_mutex->lock();
  xe::atomic_inc(global_lock_count);
}

// Leaves the global lock. Safe to recursion.
void LeaveGlobalLock(PPCContext* ppc_context, void* arg0, void* arg1) {
  if (!CheckLockArgs("LeaveGlobalLock", ppc_context, arg0, arg1)) {
    return;
  }
  auto global_mutex = reinterpret_cast<std::recursive_mutex*>(arg0);
  auto global_lock_count = reinterpret_cast<int32_t*>(arg1);
  auto new_lock_count = xe::atomic_dec(global_lock_count);
  assert_true(new_lock_count >= 0);
  global_mutex->unlock();
}

void SyscallHandler(PPCContext* ppc_context, void* arg0, void* arg1) {
  uint64_t syscall_number = ppc_context->r[0];
  switch (syscall_number) {
    default:
      assert_unhandled_case(syscall_number);
      XELOGE("Unhandled syscall {}!", syscall_number);
      break;
#pragma warning(suppress : 4065)
  }
}

bool PPCFrontend::Initialize() {
  void* arg0 = reinterpret_cast<void*>(&xe::global_critical_region::mutex());
  void* arg1 = reinterpret_cast<void*>(&builtins_.global_lock_count);
  g_expected_lock_arg0 = arg0;
  g_expected_lock_arg1 = arg1;
  XELOGI("PPCFrontend builtins: arg0={} arg1={}", arg0, arg1);
  builtins_.check_global_lock =
      processor_->DefineBuiltin("CheckGlobalLock", CheckGlobalLock, arg0, arg1);
  builtins_.enter_global_lock =
      processor_->DefineBuiltin("EnterGlobalLock", EnterGlobalLock, arg0, arg1);
  builtins_.leave_global_lock =
      processor_->DefineBuiltin("LeaveGlobalLock", LeaveGlobalLock, arg0, arg1);
  builtins_.syscall_handler = processor_->DefineBuiltin(
      "SyscallHandler", SyscallHandler, nullptr, nullptr);
  return true;
}

bool PPCFrontend::DeclareFunction(GuestFunction* function) {
  // Could scan or something here.
  // Could also check to see if it's a well-known function type and classify
  // for later.
  // Could also kick off a precompiler, since we know it's likely the function
  // will be demanded soon.
  return true;
}

bool PPCFrontend::DefineFunction(GuestFunction* function,
                                 uint32_t debug_info_flags) {
  auto translator = translator_pool_.Allocate(this);
  bool result = translator->Translate(function, debug_info_flags);
  translator_pool_.Release(translator);
  return result;
}

}  // namespace ppc
}  // namespace cpu
}  // namespace xe
