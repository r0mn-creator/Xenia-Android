/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * AArch64 raw instruction encoder for the A64 JIT backend.
 *
 * DESIGN NOTES:
 *   This file is the foundation of the A64 backend. It provides:
 *   1. Named AArch64 register constants (X0..XZR, SP, W0..WZR)
 *   2. Inline encoding functions — one per instruction form
 *   3. A64Emitter class: wraps a growable byte buffer with Emit() and fixup
 *      support for resolving forward branch targets after all blocks are done.
 *
 * ENCODING CONVENTIONS (AArch64, ARMv8-A ISA):
 *   - All instructions are exactly 4 bytes (32-bit little-endian words).
 *   - Bit [31] is the MSB; bit [0] is the LSB.
 *   - Register 31 means XZR (zero register) in data instructions;
 *     it means SP (stack pointer) in address instructions.
 *   - Immediate offsets in load/store instructions are SCALED:
 *       64-bit (LDR X): imm12 * 8 = byte offset (max 32760)
 *       32-bit (LDR W): imm12 * 4 = byte offset (max 16380)
 *   - Branch offsets are SIGNED and PC-relative, in units of 4 bytes.
 *
 * TROUBLESHOOTING:
 *   - If code crashes immediately with SIGILL, an encoding is wrong.
 *   - Verify STP/LDP offset is a multiple of 8 (aligned for 64-bit pairs).
 *   - Verify branch fixups: if Assemble() succeeds but a branch jumps to
 *     garbage, check that block_offsets[] contains the correct byte offset
 *     for every block ordinal before ApplyFixups() is called.
 *   - For memory faults inside JIT code, check that x19 = PPCContext* and
 *     x20 = virtual_membase at all times; these are set by the thunk and
 *     restored in the function epilog.
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_A64_A64_EMITTER_H_
#define XENIA_CPU_BACKEND_A64_A64_EMITTER_H_

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace xe {
namespace cpu {
namespace backend {
namespace a64 {

// ---------------------------------------------------------------------------
// Register constants (integer, 64-bit Xn / 32-bit Wn)
// ---------------------------------------------------------------------------
// Callee-saved:  x19-x28  (saved/restored by called functions)
// Caller-saved:  x0-x15   (scratch; clobbered across calls)
// Special:       x16,x17  (intra-procedure-call scratch; avoid in JIT seqs)
//                x18      (platform register; reserved on some ABI targets)
// Fixed JIT use:
//   x19  = PPCContext*        (set by HostToGuestThunk, invariant)
//   x20  = virtual_membase    (loaded from context in thunk)
//   x25  = HIR slot base ptr  (set in every generated function's prolog)
//   x29  = frame pointer
//   x30  = link register (LR)
//   x31  = XZR or SP depending on context

static constexpr int X0  =  0, X1  =  1, X2  =  2, X3  =  3;
static constexpr int X4  =  4, X5  =  5, X6  =  6, X7  =  7;
static constexpr int X8  =  8, X9  =  9, X10 = 10, X11 = 11;
static constexpr int X12 = 12, X13 = 13, X14 = 14, X15 = 15;
static constexpr int X16 = 16, X17 = 17, X18 = 18;
static constexpr int X19 = 19, X20 = 20, X21 = 21, X22 = 22;
static constexpr int X23 = 23, X24 = 24, X25 = 25, X26 = 26;
static constexpr int X27 = 27, X28 = 28, X29 = 29, X30 = 30;
static constexpr int XZR = 31;  // Zero register (reads as 0, writes discarded)
static constexpr int SP  = 31;  // Stack pointer (same encoding, used for base)

// Condition codes (used by BCOND, CSET, etc.)
static constexpr int COND_EQ =  0;  // Equal / Zero
static constexpr int COND_NE =  1;  // Not Equal / Non-Zero
static constexpr int COND_HS =  2;  // Unsigned >= (Carry Set)
static constexpr int COND_LO =  3;  // Unsigned <  (Carry Clear)
static constexpr int COND_MI =  4;  // Minus / Negative
static constexpr int COND_PL =  5;  // Plus / Non-Negative
static constexpr int COND_VS =  6;  // Overflow Set
static constexpr int COND_VC =  7;  // Overflow Clear
static constexpr int COND_HI =  8;  // Unsigned >  (Higher)
static constexpr int COND_LS =  9;  // Unsigned <= (Lower or Same)
static constexpr int COND_GE = 10;  // Signed >=
static constexpr int COND_LT = 11;  // Signed <
static constexpr int COND_GT = 12;  // Signed >
static constexpr int COND_LE = 13;  // Signed <=
static constexpr int COND_AL = 14;  // Always

// ---------------------------------------------------------------------------
// Instruction encoding helpers (all inline)
// Each function returns a 32-bit instruction word.
// ---------------------------------------------------------------------------

// ADD Xd, Xn, Xm  (64-bit shifted register, shift=0)
inline uint32_t ADD64rr(int Rd, int Rn, int Rm) {
  return 0x8B000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// SUB Xd, Xn, Xm
inline uint32_t SUB64rr(int Rd, int Rn, int Rm) {
  return 0xCB000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// AND Xd, Xn, Xm
inline uint32_t AND64rr(int Rd, int Rn, int Rm) {
  return 0x8A000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// ORR Xd, Xn, Xm
inline uint32_t ORR64rr(int Rd, int Rn, int Rm) {
  return 0xAA000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// EOR Xd, Xn, Xm  (XOR)
inline uint32_t EOR64rr(int Rd, int Rn, int Rm) {
  return 0xCA000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// ORN Xd, Xn, Xm  (Xd = Xn | ~Xm)
inline uint32_t ORN64rr(int Rd, int Rn, int Rm) {
  return 0xAA200000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// BIC Xd, Xn, Xm  (Xd = Xn & ~Xm, Bit Clear = AND NOT)
inline uint32_t BIC64rr(int Rd, int Rn, int Rm) {
  return 0x8A200000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// ADC Xd, Xn, Xm  (Xd = Xn + Xm + Carry; Carry flag must be set first)
// Used for multi-word add.  See ADDS for flag-setting variant.
inline uint32_t ADC64rr(int Rd, int Rn, int Rm) {
  return 0x9A000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// ADDS Xd, Xn, Xm  — add and set flags (sets Carry, Overflow, etc.)
inline uint32_t ADDS64rr(int Rd, int Rn, int Rm) {
  return 0xAB000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}

// MOV Xd, Xm  (= ORR Xd, XZR, Xm)
inline uint32_t MOV64rr(int Rd, int Rm) {
  // XZR = 31 in the Rn field
  return 0xAA0003E0u | (uint32_t(Rm) << 16) | Rd;
}
// MOV Xd, Wm  (= UXTW Xd, Wm = zero-extend 32->64)
// Encoded as ORR W-form to ensure upper bits are cleared.
inline uint32_t UXTW(int Rd, int Rm) {
  // 32-bit ORR: Wd = WZR ORR Wm, then upper 32 bits of Xd are zero
  return 0x2A0003E0u | (uint32_t(Rm) << 16) | Rd;
}

// 32-bit arithmetic (results are zero-extended to 64 bits)
inline uint32_t ADD32rr(int Rd, int Rn, int Rm) {
  return 0x0B000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t SUB32rr(int Rd, int Rn, int Rm) {
  return 0x4B000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t AND32rr(int Rd, int Rn, int Rm) {
  return 0x0A000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t ORR32rr(int Rd, int Rn, int Rm) {
  return 0x2A000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t EOR32rr(int Rd, int Rn, int Rm) {
  return 0x4A000000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t MOV32rr(int Rd, int Rm) {
  return 0x2A0003E0u | (uint32_t(Rm) << 16) | Rd;
}

// ADD Xd, Xn, #imm12  (unsigned 12-bit immediate, no shift)
inline uint32_t ADD64ri(int Rd, int Rn, uint32_t imm12) {
  return 0x91000000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}
// SUB Xd, Xn, #imm12
inline uint32_t SUB64ri(int Rd, int Rn, uint32_t imm12) {
  return 0xD1000000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}
// ADD Xd, Xn, #imm12, LSL #12  (for large frame sizes)
inline uint32_t ADD64ri_shift(int Rd, int Rn, uint32_t imm12) {
  return 0x91400000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}
// SUB Xd, Xn, #imm12, LSL #12
inline uint32_t SUB64ri_shift(int Rd, int Rn, uint32_t imm12) {
  return 0xD1400000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}
// ADD Wd, Wn, #imm12
inline uint32_t ADD32ri(int Rd, int Rn, uint32_t imm12) {
  return 0x11000000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}
// SUB Wd, Wn, #imm12
inline uint32_t SUB32ri(int Rd, int Rn, uint32_t imm12) {
  return 0x51000000u | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5) | Rd;
}

// MUL Xd, Xn, Xm  (= MADD Xd, Xn, Xm, XZR)
inline uint32_t MUL64rr(int Rd, int Rn, int Rm) {
  return 0x9B007C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t MUL32rr(int Rd, int Rn, int Rm) {
  return 0x1B007C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// SMULH Xd, Xn, Xm  (signed multiply-high: result = (Xn*Xm) >> 64)
inline uint32_t SMULH(int Rd, int Rn, int Rm) {
  return 0x9B407C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// UMULH Xd, Xn, Xm  (unsigned multiply-high)
inline uint32_t UMULH(int Rd, int Rn, int Rm) {
  return 0x9BC07C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// SDIV Xd, Xn, Xm  (signed 64-bit divide)
inline uint32_t SDIV64(int Rd, int Rn, int Rm) {
  return 0x9AC00C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// UDIV Xd, Xn, Xm  (unsigned 64-bit divide)
inline uint32_t UDIV64(int Rd, int Rn, int Rm) {
  return 0x9AC00800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// SDIV Wd, Wn, Wm  (signed 32-bit divide)
inline uint32_t SDIV32(int Rd, int Rn, int Rm) {
  return 0x1AC00C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// UDIV Wd, Wn, Wm  (unsigned 32-bit divide)
inline uint32_t UDIV32(int Rd, int Rn, int Rm) {
  return 0x1AC00800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}

// Shift by register
// LSLV Xd, Xn, Xm  (logical shift left variable)
inline uint32_t LSL64rr(int Rd, int Rn, int Rm) {
  return 0x9AC02000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// LSRV Xd, Xn, Xm  (logical shift right variable)
inline uint32_t LSR64rr(int Rd, int Rn, int Rm) {
  return 0x9AC02400u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// ASRV Xd, Xn, Xm  (arithmetic shift right variable)
inline uint32_t ASR64rr(int Rd, int Rn, int Rm) {
  return 0x9AC02800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
// RORV Xd, Xn, Xm  (rotate right variable)
inline uint32_t ROR64rr(int Rd, int Rn, int Rm) {
  return 0x9AC02C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t LSL32rr(int Rd, int Rn, int Rm) {
  return 0x1AC02000u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t LSR32rr(int Rd, int Rn, int Rm) {
  return 0x1AC02400u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t ASR32rr(int Rd, int Rn, int Rm) {
  return 0x1AC02800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t ROR32rr(int Rd, int Rn, int Rm) {
  return 0x1AC02C00u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rd;
}

// Shift by immediate
// LSL Xd, Xn, #imm  (= UBFM Xd, Xn, #(64-imm), #(63-imm))
inline uint32_t LSL64ri(int Rd, int Rn, uint32_t imm) {
  uint32_t immr = (64 - imm) & 63;
  uint32_t imms = 63 - imm;
  return 0xD3400000u | (immr << 16) | (imms << 10) | (uint32_t(Rn) << 5) | Rd;
}
// LSR Xd, Xn, #imm  (= UBFM Xd, Xn, #imm, #63)
inline uint32_t LSR64ri(int Rd, int Rn, uint32_t imm) {
  return 0xD3400000u | (imm << 16) | (63u << 10) | (uint32_t(Rn) << 5) | Rd;
}
// ASR Xd, Xn, #imm  (= SBFM Xd, Xn, #imm, #63)
inline uint32_t ASR64ri(int Rd, int Rn, uint32_t imm) {
  return 0x9340FC00u | (imm << 16) | (uint32_t(Rn) << 5) | Rd;
}

// NEG Xd, Xm  (= SUB Xd, XZR, Xm)
inline uint32_t NEG64r(int Rd, int Rm) {
  return 0xCB0003E0u | (uint32_t(Rm) << 16) | Rd;
}
// NOT Xd, Xm  (= ORN Xd, XZR, Xm = MVN Xd, Xm)
inline uint32_t NOT64r(int Rd, int Rm) {
  return 0xAA2003E0u | (uint32_t(Rm) << 16) | Rd;
}

// Sign/zero extension
// SXTW Xd, Wn  (sign-extend 32-bit to 64-bit)
inline uint32_t SXTW(int Rd, int Rn) {
  return 0x93407C00u | (uint32_t(Rn) << 5) | Rd;
}
// SXTH Xd, Wn  (sign-extend 16-bit to 64-bit)
inline uint32_t SXTH64(int Rd, int Rn) {
  return 0x93403C00u | (uint32_t(Rn) << 5) | Rd;
}
// SXTB Xd, Wn  (sign-extend 8-bit to 64-bit)
inline uint32_t SXTB64(int Rd, int Rn) {
  return 0x93401C00u | (uint32_t(Rn) << 5) | Rd;
}

// ---------------------------------------------------------------------------
// Compare and test
// ---------------------------------------------------------------------------
// CMP Xn, Xm  (= SUBS XZR, Xn, Xm, sets NZCV flags)
inline uint32_t CMP64rr(int Rn, int Rm) {
  return 0xEB00001Fu | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5);
}
// CMP Wn, Wm
inline uint32_t CMP32rr(int Rn, int Rm) {
  return 0x6B00001Fu | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5);
}
// CMP Xn, #imm12
inline uint32_t CMP64ri(int Rn, uint32_t imm12) {
  return 0xF100001Fu | ((imm12 & 0xFFFu) << 10) | (uint32_t(Rn) << 5);
}
// TST Xn, Xm  (= ANDS XZR, Xn, Xm)
inline uint32_t TST64rr(int Rn, int Rm) {
  return 0xEA00001Fu | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5);
}

// CSET Xd, cond  (set Xd=1 if cond is true, else 0)
// Implemented as CSINC Xd, XZR, XZR, invert(cond)
// cond^1 inverts the condition (works for all AArch64 condition pairs)
inline uint32_t CSET64(int Rd, int cond) {
  int inv_cond = cond ^ 1;
  return 0x9A9F07E0u | (uint32_t(inv_cond & 0xF) << 12) | Rd;
}

// ---------------------------------------------------------------------------
// Load / Store (unsigned scaled immediate offset)
// imm12 in the encoding is byte_offset / element_size — must be aligned.
// Troubleshooting: crashes here usually mean the offset is unaligned or
//                  larger than the max (12 bits * element_size).
// ---------------------------------------------------------------------------
// LDR Xd, [Xn, #byte_offset]  (64-bit, max offset = 32760)
inline uint32_t LDR64(int Rt, int Rn, uint32_t byte_offset) {
  return 0xF9400000u | (((byte_offset / 8) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// STR Xt, [Xn, #byte_offset]
inline uint32_t STR64(int Rt, int Rn, uint32_t byte_offset) {
  return 0xF9000000u | (((byte_offset / 8) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDR Wd, [Xn, #byte_offset]  (32-bit, max offset = 16380)
inline uint32_t LDR32(int Rt, int Rn, uint32_t byte_offset) {
  return 0xB9400000u | (((byte_offset / 4) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// STR Wt, [Xn, #byte_offset]
inline uint32_t STR32(int Rt, int Rn, uint32_t byte_offset) {
  return 0xB9000000u | (((byte_offset / 4) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDRH Wd, [Xn, #byte_offset]  (16-bit, max offset = 8190)
inline uint32_t LDRH(int Rt, int Rn, uint32_t byte_offset) {
  return 0x79400000u | (((byte_offset / 2) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// STRH Wt, [Xn, #byte_offset]
inline uint32_t STRH(int Rt, int Rn, uint32_t byte_offset) {
  return 0x79000000u | (((byte_offset / 2) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDRB Wd, [Xn, #byte_offset]  (8-bit, max offset = 4095)
inline uint32_t LDRB(int Rt, int Rn, uint32_t byte_offset) {
  return 0x39400000u | ((byte_offset & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// STRB Wt, [Xn, #byte_offset]
inline uint32_t STRB(int Rt, int Rn, uint32_t byte_offset) {
  return 0x39000000u | ((byte_offset & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDRSB Xd, [Xn, #byte_offset]  (8-bit sign-extend to 64-bit)
inline uint32_t LDRSB64(int Rt, int Rn, uint32_t byte_offset) {
  return 0x39800000u | ((byte_offset & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDRSH Xd, [Xn, #byte_offset]  (16-bit sign-extend to 64-bit)
inline uint32_t LDRSH64(int Rt, int Rn, uint32_t byte_offset) {
  return 0x79800000u | (((byte_offset / 2) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}
// LDRSW Xd, [Xn, #byte_offset]  (32-bit sign-extend to 64-bit)
inline uint32_t LDRSW(int Rt, int Rn, uint32_t byte_offset) {
  return 0xB9800000u | (((byte_offset / 4) & 0xFFFu) << 10) |
         (uint32_t(Rn) << 5) | Rt;
}

// Load/store using register offset  LDR Xd, [Xn, Xm]
inline uint32_t LDR64_REG(int Rt, int Rn, int Rm) {
  return 0xF8606800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rt;
}
inline uint32_t STR64_REG(int Rt, int Rn, int Rm) {
  return 0xF8206800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rt;
}
inline uint32_t LDR32_REG(int Rt, int Rn, int Rm) {
  return 0xB8606800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rt;
}
inline uint32_t STR32_REG(int Rt, int Rn, int Rm) {
  return 0xB8206800u | (uint32_t(Rm) << 16) | (uint32_t(Rn) << 5) | Rt;
}

// ---------------------------------------------------------------------------
// Load / Store pair
//
// STP Xt1, Xt2, [Xn, #offset]!  — pre-indexed, sp += offset first
//   base 0xA9800000, imm7 = offset/8 (signed 7-bit, range -512..504)
// STP Xt1, Xt2, [Xn, #offset]   — signed offset, no writeback
//   base 0xA9000000
// LDP Xt1, Xt2, [Xn], #offset   — post-indexed, sp += offset after load
//   base 0xA8C00000
// LDP Xt1, Xt2, [Xn, #offset]   — signed offset, no writeback (load form)
//   base 0xA9400000
// ---------------------------------------------------------------------------
inline uint32_t STP64_PRE(int Rt1, int Rt2, int Rn, int byte_offset) {
  int imm7 = byte_offset / 8;
  return 0xA9800000u | (uint32_t(imm7 & 0x7F) << 15) |
         (uint32_t(Rt2) << 10) | (uint32_t(Rn) << 5) | Rt1;
}
inline uint32_t STP64_OFF(int Rt1, int Rt2, int Rn, int byte_offset) {
  int imm7 = byte_offset / 8;
  return 0xA9000000u | (uint32_t(imm7 & 0x7F) << 15) |
         (uint32_t(Rt2) << 10) | (uint32_t(Rn) << 5) | Rt1;
}
inline uint32_t LDP64_POST(int Rt1, int Rt2, int Rn, int byte_offset) {
  int imm7 = byte_offset / 8;
  return 0xA8C00000u | (uint32_t(imm7 & 0x7F) << 15) |
         (uint32_t(Rt2) << 10) | (uint32_t(Rn) << 5) | Rt1;
}
inline uint32_t LDP64_OFF(int Rt1, int Rt2, int Rn, int byte_offset) {
  int imm7 = byte_offset / 8;
  return 0xA9400000u | (uint32_t(imm7 & 0x7F) << 15) |
         (uint32_t(Rt2) << 10) | (uint32_t(Rn) << 5) | Rt1;
}

// ---------------------------------------------------------------------------
// Move wide immediates (for materializing arbitrary 64-bit constants)
// MOVZ Xd, #imm16, LSL #(hw*16)  — zero all bits then insert imm16
// MOVK Xd, #imm16, LSL #(hw*16)  — keep existing bits, insert imm16
// hw: 0=shift0, 1=shift16, 2=shift32, 3=shift48
// ---------------------------------------------------------------------------
inline uint32_t MOVZ64(int Rd, uint32_t imm16, int hw = 0) {
  return 0xD2800000u | (uint32_t(hw & 3) << 21) |
         ((imm16 & 0xFFFFu) << 5) | Rd;
}
inline uint32_t MOVK64(int Rd, uint32_t imm16, int hw = 0) {
  return 0xF2800000u | (uint32_t(hw & 3) << 21) |
         ((imm16 & 0xFFFFu) << 5) | Rd;
}
inline uint32_t MOVZ32(int Rd, uint32_t imm16) {
  return 0x52800000u | ((imm16 & 0xFFFFu) << 5) | Rd;
}

// ---------------------------------------------------------------------------
// Branches
// B #offset   — unconditional, 26-bit PC-relative (offset in bytes, /4)
// BL #offset  — branch and link (sets LR = next instruction)
// BR Xn       — branch to register
// BLR Xn      — branch with link to register
// RET         — return via X30 (LR)
// B.cond      — conditional branch, 19-bit PC-relative
// CBZ/CBNZ    — compare-and-branch, 19-bit PC-relative
//
// All offsets are in BYTES and must be multiples of 4.
// ---------------------------------------------------------------------------
inline uint32_t B_instr(int32_t byte_offset) {
  return 0x14000000u | (uint32_t((byte_offset / 4) & 0x3FFFFFFu));
}
inline uint32_t BL_instr(int32_t byte_offset) {
  return 0x94000000u | (uint32_t((byte_offset / 4) & 0x3FFFFFFu));
}
inline uint32_t BR(int Rn) {
  return 0xD61F0000u | (uint32_t(Rn) << 5);
}
inline uint32_t BLR(int Rn) {
  return 0xD63F0000u | (uint32_t(Rn) << 5);
}
// RET = RET X30  (branch to link register)
inline uint32_t RET_instr() { return 0xD65F03C0u; }
// NOP
inline uint32_t NOP_instr() { return 0xD503201Fu; }
// BRK #imm16  — software breakpoint (crashes with SIGTRAP)
// Use BRK 0xDEAD for unimplemented HIR ops so they're easy to spot in crashes.
inline uint32_t BRK(uint32_t imm16) {
  return 0xD4200000u | ((imm16 & 0xFFFFu) << 5);
}
// B.cond #offset  (19-bit PC-relative offset, imm19 = offset/4)
inline uint32_t BCOND(int cond, int32_t byte_offset) {
  return 0x54000000u | (uint32_t((byte_offset / 4) & 0x7FFFFu) << 5) |
         uint32_t(cond & 0xF);
}
// CBZ Xn, #offset  — branch if Xn == 0
inline uint32_t CBZ64(int Rn, int32_t byte_offset) {
  return 0xB4000000u | (uint32_t((byte_offset / 4) & 0x7FFFFu) << 5) | Rn;
}
// CBNZ Xn, #offset — branch if Xn != 0
inline uint32_t CBNZ64(int Rn, int32_t byte_offset) {
  return 0xB5000000u | (uint32_t((byte_offset / 4) & 0x7FFFFu) << 5) | Rn;
}
// CBZ Wn, #offset
inline uint32_t CBZ32(int Rn, int32_t byte_offset) {
  return 0x34000000u | (uint32_t((byte_offset / 4) & 0x7FFFFu) << 5) | Rn;
}
// CBNZ Wn, #offset
inline uint32_t CBNZ32(int Rn, int32_t byte_offset) {
  return 0x35000000u | (uint32_t((byte_offset / 4) & 0x7FFFFu) << 5) | Rn;
}

// ---------------------------------------------------------------------------
// Byte-swap (for LOAD_STORE_BYTE_SWAP flag on PPC big-endian loads)
// REV Xd, Xn  — reverse byte order of 64-bit register
// REV Wd, Wn  — reverse byte order of 32-bit register
// REV16 Wd, Wn — reverse bytes within each 16-bit halfword
// ---------------------------------------------------------------------------
inline uint32_t REV64(int Rd, int Rn) {
  return 0xDAC00C00u | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t REV32(int Rd, int Rn) {
  return 0x5AC00800u | (uint32_t(Rn) << 5) | Rd;
}
inline uint32_t REV16_32(int Rd, int Rn) {
  return 0x5AC00400u | (uint32_t(Rn) << 5) | Rd;
}

// ---------------------------------------------------------------------------
// A64Emitter  — code buffer with branch fixup support
//
// Usage:
//   A64Emitter e;
//   e.Emit(ADD64rr(X0, X1, X2));     // append an instruction
//   size_t patch_site = e.Size();
//   e.Emit(B_instr(0));               // placeholder branch
//   // ... emit more code ...
//   size_t target = e.Size();
//   e.PatchBranch(patch_site, target);// fix the branch
//
// For HIR block branches, record a fixup instead of patching immediately
// (the target block might not be emitted yet):
//   e.RecordBranchFixup(instr_offset, target_block_ordinal, BranchKind::B);
//   e.Emit(B_instr(0));  // placeholder — ApplyFixups() will overwrite it
//
// After all blocks are emitted, call e.ApplyFixups(block_offsets).
// ---------------------------------------------------------------------------
enum class BranchKind {
  B,       // unconditional B
  CBNZ_X0, // CBNZ X0, target  (branch if X0 != 0, used for BRANCH_TRUE)
  CBZ_X0,  // CBZ  X0, target  (branch if X0 == 0, used for BRANCH_FALSE)
};

struct BranchFixup {
  size_t instr_byte_offset;    // byte offset of the branch instruction in buffer
  int    target_block_ordinal; // HIR block ordinal to branch to
  BranchKind kind;
};

class A64Emitter {
 public:
  A64Emitter() { buffer_.reserve(4096); }

  // Emit one 32-bit instruction word
  void Emit(uint32_t instr) {
    buffer_.push_back(instr);
  }

  // Emit a NOP (used for alignment or padding)
  void EmitNOP() { buffer_.push_back(NOP_instr()); }

  // Materialize an arbitrary 64-bit constant into Xreg using MOVZ + MOVK.
  // Emits 1..4 instructions depending on how many non-zero 16-bit slices exist.
  void EmitMov64(int Xreg, uint64_t value) {
    bool first = true;
    for (int hw = 0; hw < 4; hw++) {
      uint32_t slice = (value >> (hw * 16)) & 0xFFFF;
      if (first) {
        // Always emit at least one MOVZ to zero the register
        Emit(MOVZ64(Xreg, slice, hw));
        first = false;
        if (value == uint64_t(slice) << (hw * 16)) return; // only 1 slice needed
      } else if (slice != 0) {
        Emit(MOVK64(Xreg, slice, hw));
      }
    }
  }

  // Emit a 64-bit constant into Xreg, optimizing for zero-upper-half case.
  // Handles the common case of 32-bit zero-extended constants with 2 instrs.
  void EmitMov64Fast(int Xreg, uint64_t value) {
    if (value == 0) {
      // MOV Xreg, XZR
      Emit(MOV64rr(Xreg, XZR));
      return;
    }
    if (value <= 0xFFFF) {
      Emit(MOVZ64(Xreg, uint32_t(value), 0));
      return;
    }
    if (value <= 0xFFFFFFFFu) {
      Emit(MOVZ64(Xreg, uint32_t(value) & 0xFFFF, 0));
      if ((value >> 16) != 0)
        Emit(MOVK64(Xreg, uint32_t(value >> 16) & 0xFFFF, 1));
      return;
    }
    EmitMov64(Xreg, value);
  }

  // Emit frame size adjustment, handling sizes > 4095 with 2 instructions.
  // ADD SP, SP, #N  or  SUB SP, SP, #N
  void EmitAddSP(int32_t amount) {
    if (amount == 0) return;
    if (amount > 0) {
      // ADD SP, SP, #amount
      if (uint32_t(amount) <= 4095) {
        Emit(ADD64ri(SP, SP, amount));
      } else {
        if (amount & 0xFFF)
          Emit(ADD64ri(SP, SP, amount & 0xFFF));
        if (amount >> 12)
          Emit(ADD64ri_shift(SP, SP, amount >> 12));
      }
    } else {
      // SUB SP, SP, #|amount|
      uint32_t n = uint32_t(-amount);
      if (n <= 4095) {
        Emit(SUB64ri(SP, SP, n));
      } else {
        if (n & 0xFFF)
          Emit(SUB64ri(SP, SP, n & 0xFFF));
        if (n >> 12)
          Emit(SUB64ri_shift(SP, SP, n >> 12));
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Branch fixup API
  // ---------------------------------------------------------------------------

  // Record a forward branch that needs its target resolved later.
  // The caller emits a placeholder instruction (e.g., B_instr(0)) BEFORE
  // calling this, so call_offset = Size() - 1 (the just-emitted word index)
  // This is recorded by SIZE in bytes (not word index).
  void RecordBranchFixup(size_t instr_word_index,
                         int target_block_ordinal,
                         BranchKind kind) {
    fixups_.push_back({instr_word_index * 4, target_block_ordinal, kind});
  }

  // After all blocks are emitted, call this with a map from block ordinal
  // to the byte offset of that block's first instruction in the buffer.
  // Returns false if any target block is not in the map.
  bool ApplyFixups(const std::unordered_map<int, size_t>& block_offsets) {
    for (auto& fix : fixups_) {
      auto it = block_offsets.find(fix.target_block_ordinal);
      if (it == block_offsets.end()) return false;

      size_t target_byte = it->second;
      size_t src_byte    = fix.instr_byte_offset;
      int32_t delta      = int32_t(int64_t(target_byte) - int64_t(src_byte));

      uint32_t* slot = &buffer_[src_byte / 4];
      switch (fix.kind) {
        case BranchKind::B:
          *slot = B_instr(delta);
          break;
        case BranchKind::CBNZ_X0:
          *slot = CBNZ64(X0, delta);
          break;
        case BranchKind::CBZ_X0:
          *slot = CBZ64(X0, delta);
          break;
      }
    }
    return true;
  }

  // Current size in bytes (number of instructions * 4)
  size_t Size() const { return buffer_.size() * 4; }

  // Raw instruction buffer (write view; use exec pointer for actual execution)
  const uint32_t* Data() const { return buffer_.data(); }

  // Current instruction count (for recording a fixup before emitting placeholder)
  size_t InstrCount() const { return buffer_.size(); }

  void Reset() {
    buffer_.clear();
    fixups_.clear();
  }

 private:
  std::vector<uint32_t> buffer_;
  std::vector<BranchFixup> fixups_;
};

}  // namespace a64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_CPU_BACKEND_A64_A64_EMITTER_H_
