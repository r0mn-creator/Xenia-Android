/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_STACK_LAYOUT_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_STACK_LAYOUT_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include "xenia/base/vec128.h"

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

// ---------------------------------------------------------------------------
// AArch64 Calling Convention (AAPCS64) Summary
// ---------------------------------------------------------------------------
// Argument registers:    x0–x7   (integer), v0–v7  (float/vector)
// Return registers:      x0–x1   (integer), v0–v1  (float/vector)
// Caller-saved scratch:  x9–x15, v8–v15 (lower 64 bits only caller-saved;
//                        upper 64 bits of v8–v15 are callee-saved!)
// Callee-saved:          x19–x28, x29 (fp), x30 (lr), v8–v15 (full 128b)
// Stack pointer:         sp (must be 16-byte aligned at all call boundaries)
// Frame pointer:         x29 (optional but used here for stack walking)
// Link register:         x30 (return address)
//
// Xenia register assignments for JIT'd guest code:
//   x19  = PPC context pointer  (callee-saved -> survives calls to helpers)
//   x20  = memory base pointer  (callee-saved -> survives calls to helpers)
//   x21–x27 = JIT allocatable GPRs (callee-saved)
//   x9–x15   = scratch / temp GPRs (caller-saved, not preserved across calls)
//   v8–v15   = JIT allocatable NEON vector regs (callee-saved)
//   v16–v31  = scratch NEON regs (caller-saved)
// ---------------------------------------------------------------------------

class StackLayout {
 public:
  // -------------------------------------------------------------------------
  // Thunk Stack Frame
  //
  // Used by HostToGuestThunk and GuestToHostThunk. This is the frame pushed
  // when host C++ code enters JIT'd guest code (or vice versa).
  //
  // AArch64 stack grows downward. sp must be 16-byte aligned before any BL.
  //
  // Layout (offsets from sp after frame is set up):
  //
  //  sp + 0x000  x29 (frame pointer)   \  stp x29, x30
  //  sp + 0x008  x30 (link register)   /
  //  sp + 0x010  x19 (context ptr)     \  stp x19, x20
  //  sp + 0x018  x20 (membase ptr)     /
  //  sp + 0x020  x21                   \  stp x21, x22
  //  sp + 0x028  x22                   /
  //  sp + 0x030  x23                   \  stp x23, x24
  //  sp + 0x038  x24                   /
  //  sp + 0x040  x25                   \  stp x25, x26
  //  sp + 0x048  x26                   /
  //  sp + 0x050  x27                   \  stp x27, x28
  //  sp + 0x058  x28                   /
  //  sp + 0x060  v8  (lower 8 bytes)   \  stp d8, d9
  //  sp + 0x068  v9  (lower 8 bytes)   /
  //  sp + 0x070  v10                   \  stp d10, d11
  //  sp + 0x078  v11                   /
  //  sp + 0x080  v12                   \  stp d12, d13
  //  sp + 0x088  v13                   /
  //  sp + 0x090  v14                   \  stp d14, d15
  //  sp + 0x098  v15                   /
  //  sp + 0x0A0  arg_temp[0]           \  3 x 8-byte argument spill slots
  //  sp + 0x0A8  arg_temp[1]           |
  //  sp + 0x0B0  arg_temp[2]           /
  //  sp + 0x0B8  padding (8 bytes)       <- keeps total size % 16 == 0
  //  --- total: 0xC0 = 192 bytes ---
  // -------------------------------------------------------------------------
  XEPACKEDSTRUCT(Thunk, {
    uint64_t fp;           // x29
    uint64_t lr;           // x30
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t d8;
    uint64_t d9;
    uint64_t d10;
    uint64_t d11;
    uint64_t d12;
    uint64_t d13;
    uint64_t d14;
    uint64_t d15;
    uint64_t arg_temp[3];
    uint64_t padding;
  });
  static_assert(sizeof(Thunk) % 16 == 0,
                "sizeof(Thunk) must be a multiple of 16!");
  static const size_t THUNK_STACK_SIZE = sizeof(Thunk);

  // -------------------------------------------------------------------------
  // Guest Function Stack Frame
  //
  // Every JIT-compiled guest function sets up this frame on entry.
  //
  //  sp + 0x00  x29 (frame pointer)   \  stp x29, x30
  //  sp + 0x08  x30 (link register)   /
  //  sp + 0x10  context ptr (x19)       <- spilled for helpers to reload
  //  sp + 0x18  guest return address    <- PPC return address (not host lr)
  //  sp + 0x20  call return address     <- host address to return to after call
  //  sp + 0x28  scratch[0]             \
  //  sp + 0x30  scratch[1]              |  6 x 8-byte local scratch slots
  //  sp + 0x38  scratch[2]              |  (used by sequences for temporaries)
  //  sp + 0x40  scratch[3]              |
  //  sp + 0x48  scratch[4]              |
  //  sp + 0x50  scratch[5]             /
  //  sp + 0x58  saved x21..x27          \  7 allocatable callee-saved GPRs the
  //  ...        (7 x 8 = 56 bytes)       /  register allocator hands to guest regs
  //  sp + 0x90  saved v4..v15           \  12 allocatable vector regs (full 128b);
  //  ...        (12 x 16 = 192 bytes)    /  must survive guest->guest calls
  //  --- total: 0x150 = 336 bytes (16-aligned) ---
  //
  // IMPORTANT: the shared register allocator treats the 7 GPRs (x21-x27) and 12
  // vector regs (v4-v15) as preserved across guest-to-guest calls, so it does
  // NOT spill them around a Call. That contract only holds if every guest
  // function saves/restores them in its prolog/epilog (like a normal AAPCS64
  // callee). Without this, a register-heavy caller's guest values are silently
  // clobbered by the callee — manifesting as e.g. a stored value of 0 where the
  // guest register actually held a valid pointer.
  // -------------------------------------------------------------------------
  static const size_t GUEST_STACK_SIZE    = 0x150;  // 336
  static const size_t GUEST_CTX_HOME     = 0x10;
  static const size_t GUEST_RET_ADDR     = 0x18;
  static const size_t GUEST_CALL_RET_ADDR = 0x20;
  static const size_t GUEST_SCRATCH_BASE = 0x28;
  // Save area for the allocatable callee-saved registers.
  static const size_t GUEST_SAVED_GPR_BASE = 0x58;   // x21..x27 (7 x 8)
  static const size_t GUEST_SAVED_VEC_BASE = 0x90;   // v4..v15  (12 x 16)
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_STACK_LAYOUT_H_
