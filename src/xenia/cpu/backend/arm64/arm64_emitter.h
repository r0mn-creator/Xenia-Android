/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Developers. All rights reserved.                      *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_BACKEND_ARM64_ARM64_EMITTER_H_
#define XENIA_CPU_BACKEND_ARM64_ARM64_EMITTER_H_

#include "xenia/base/platform.h"
#if XE_ARCH_ARM64

#include <vector>

#include <unordered_map>

#include "xenia/base/arena.h"
#include "xenia/base/vec128.h"
#include "xenia/cpu/function.h"
#include "xenia/cpu/function_trace_data.h"
#include "xenia/cpu/hir/block.h"
#include "xenia/cpu/hir/hir_builder.h"
#include "xenia/cpu/hir/instr.h"
#include "xenia/cpu/hir/label.h"
#include "xenia/cpu/hir/value.h"
#include "xenia/memory.h"

// Dolphin's Arm64Emitter is a self-contained ARM64 code generation library
// with no GameCube-specific dependencies. We use it as our assembler backend
// the same way the x64 backend uses Xbyak.
// Source: dolphin-master/Source/Core/Common/Arm64Emitter.h
#include "third_party/arm64emitter/Arm64Emitter.h"

namespace xe {
namespace cpu {
class Processor;
}
}  // namespace xe

namespace xe {
namespace cpu {
namespace backend {
namespace arm64 {

class ARM64Backend;
class ARM64CodeCache;
struct EmitFunctionInfo;

// These typedefs mirror arm64_backend.h; duplicated here to avoid the
// circular include dependency (arm64_emitter.h ↔ arm64_backend.h).
typedef void* (*HostToGuestThunk)(void* target, void* arg0, void* arg1);
typedef void* (*GuestToHostThunk)(void* target, void* arg0, void* arg1);
typedef void (*ResolveFunctionThunk)();

// ---------------------------------------------------------------------------
// NEON constant pool IDs
//
// Pre-computed 128-bit constants for vector operations. These mirror the
// XmmConst enum in the x64 backend — the same logical constants are needed
// to implement the XIR vector opcodes, just stored in a different format.
// ---------------------------------------------------------------------------
enum NeonConst {
  NeonZero = 0,
  NeonOne,
  NeonNegativeOne,
  NeonByteSwapMask,      // For big->little endian swaps
  NeonSignMaskPS,        // Sign bit mask for 4 x f32
  NeonSignMaskPD,        // Sign bit mask for 2 x f64
  NeonAbsMaskPS,
  NeonAbsMaskPD,
  NeonPackD3DCOLOR,
  NeonUnpackD3DCOLOR,
  NeonPackFLOAT16_2,
  NeonUnpackFLOAT16_2,
  NeonPackFLOAT16_4,
  NeonUnpackFLOAT16_4,
  NeonPackSHORT_2,
  NeonPackSHORT_4,
  NeonUnpackSHORT_2,
  NeonUnpackSHORT_4,
  NeonPackUINT_2101010,
  NeonUnpackUINT_2101010,
  NeonOneOver255,
  NeonShiftByteMask,
  NeonSwapWordMask,
  NeonIntMin,
  NeonIntMax,
  NeonQNaN,
  NeonConst_MAX
};

// ---------------------------------------------------------------------------
// ARM64 register allocation
//
// We have 31 general-purpose registers and 32 NEON registers.
//
//  Dedicated (never allocated):
//    x0         = scratch / argument passing to helpers
//    x1         = scratch / argument passing to helpers
//    x18        = reserved by Android platform ABI (do not use)
//    x19        = PPC guest context pointer (GuestContext*)
//    x20        = host memory base pointer
//    x29 (fp)   = frame pointer
//    x30 (lr)   = link register (managed by prolog/epilog)
//    sp (x31)   = stack pointer
//
//  Allocatable GPRs (7 callee-saved, matches x64 backend count):
//    x21, x22, x23, x24, x25, x26, x27
//
//  Scratch GPRs (caller-saved, for use within single sequence emissions):
//    x9, x10, x11, x12, x13, x14, x15
//
//  Allocatable NEON (12 regs, matches x64 XMM count):
//    v4–v15  (v8–v15 are callee-saved; v4–v7 are caller-saved)
//    Note: the upper 64 bits of v8-v15 are callee-saved per AAPCS64.
//          We save/restore full 128 bits in our thunks to be safe.
//
//  Scratch NEON (caller-saved):
//    v0, v1, v2, v3   (also used for arg passing to helpers)
//    v16–v31
// ---------------------------------------------------------------------------
enum ARM64EmitterFeatureFlags {
  // FEAT_FP16: half-precision float arithmetic (most modern Android SoCs)
  kARM64EmitFP16 = 1 << 0,
  // FEAT_DOTPROD: UDOT/SDOT instructions
  kARM64EmitDotProd = 1 << 1,
  // FEAT_SVE: Scalable Vector Extension (rare on mobile, skip for now)
  kARM64EmitSVE = 1 << 2,
  // FEAT_LRCPC: load-acquire RCpc instructions (better atomics)
  kARM64EmitLRCPC = 1 << 3,
};

class ARM64Emitter : public Arm64Gen::ARM64CodeBlock {
 public:
  // GPR_COUNT and VEC_COUNT mirror x64 backend values so the register
  // allocator (which lives in the shared HIR layer) sees the same count.
  static constexpr int GPR_COUNT = 7;
  static constexpr int VEC_COUNT = 12;

  // Maps HIR register index [0..GPR_COUNT-1] to AArch64 register number.
  // x21=0, x22=1, x23=2, x24=3, x25=4, x26=5, x27=6
  static const uint32_t gpr_reg_map_[GPR_COUNT];

  // Maps HIR vector register index [0..VEC_COUNT-1] to NEON register number.
  // v4=0, v5=1, v6=2, v7=3, v8=4, v9=5, v10=6, v11=7, v12=8, v13=9,
  // v14=10, v15=11
  static const uint32_t vec_reg_map_[VEC_COUNT];

  ARM64Emitter(ARM64Backend* backend, size_t block_size = 32 * 1024 * 1024);
  virtual ~ARM64Emitter();

  Processor* processor() const { return processor_; }
  ARM64Backend* backend() const { return backend_; }

  bool Emit(GuestFunction* function, hir::HIRBuilder* builder,
            uint32_t debug_info_flags, FunctionDebugInfo* debug_info,
            void** out_code_address, size_t* out_code_size,
            std::vector<SourceMapEntry>* out_source_map);

  // ---- Context and memory base register access ----------------------------

  // Returns the Arm64Gen register for the PPC context pointer (x19).
  Arm64Gen::ARM64Reg GetContextReg() const;
  // Returns the Arm64Gen register for the memory base pointer (x20).
  Arm64Gen::ARM64Reg GetMembaseReg() const;

  // Reload context/membase from their home slots on the stack. Called after
  // any helper call that might have clobbered them (shouldn't happen since
  // they're callee-saved, but useful after exception-path rejoins).
  void ReloadContext();
  void ReloadMembase();

  // ---- Register setup helpers for sequence emission -----------------------

  // Translates a HIR Value's register index to the corresponding AArch64
  // general-purpose register at the appropriate width.
  static Arm64Gen::ARM64Reg GetGPR64(const hir::Value* v);
  static Arm64Gen::ARM64Reg GetGPR32(const hir::Value* v);
  static Arm64Gen::ARM64Reg GetGPR16(const hir::Value* v);
  static Arm64Gen::ARM64Reg GetGPR8(const hir::Value* v);
  // Translates a HIR Value's register index to a NEON register.
  static Arm64Gen::ARM64Reg GetVec(const hir::Value* v);

  // ---- Constant loading ---------------------------------------------------

  // Load a 64-bit immediate into a register using the minimum number of MOV/
  // MOVK instructions.
  void LoadConstantI64(Arm64Gen::ARM64Reg dest, uint64_t value);
  void LoadConstantI32(Arm64Gen::ARM64Reg dest, uint32_t value);
  // Load a float constant into a NEON register lane.
  void LoadConstantF32(Arm64Gen::ARM64Reg dest, float value);
  void LoadConstantF64(Arm64Gen::ARM64Reg dest, double value);
  // Load a vec128 constant into a NEON register using the constant pool.
  void LoadConstantV128(Arm64Gen::ARM64Reg dest, const vec128_t& value);

  // ---- NEON constant pool access ------------------------------------------
  // Returns a pointer to the pre-computed constant. Emits an LDR (literal)
  // or ADR + LDR pair to load it into the destination register.
  void LoadNeonConst(Arm64Gen::ARM64Reg dest, NeonConst id);

  // ---- Guest function call helpers ----------------------------------------
  void Call(const hir::Instr* instr, GuestFunction* function);
  void CallIndirect(const hir::Instr* instr, Arm64Gen::ARM64Reg reg);
  // DIAGNOSTIC: emit a call to XeStoreWatchRecord(ea-membase, value, fn) after a
  // 32-bit store so we can trace writes to a watched guest address range.
  void EmitStoreWatch(Arm64Gen::ARM64Reg ea, Arm64Gen::ARM64Reg value32);
  void EmitLoadTrace(Arm64Gen::ARM64Reg ea, Arm64Gen::ARM64Reg value32);
  void CallExtern(const hir::Instr* instr, const Function* function);
  // Call a host C++ helper function. Saves/restores caller-saved regs.
  void CallNative(void* fn);
  void CallNative(uint64_t (*fn)(void* raw_context));
  void CallNative(uint64_t (*fn)(void* raw_context, uint64_t arg0));
  void CallNative(uint64_t (*fn)(void* raw_context, uint64_t arg0),
                  uint64_t arg0);

  void SetReturnAddress(uint64_t value);

  // ---- Debug/trap ---------------------------------------------------------
  void DebugBreak();
  void Trap(uint16_t trap_type = 0);
  void UnimplementedInstr(const hir::Instr* i);
  void MarkSourceOffset(const hir::Instr* i);

  // ---- Epilog label access ------------------------------------------------
  // Emit an unconditional branch to the function epilog. The branch is
  // registered internally and patched when SetEpilogLabel() is called.
  // Returns the FixupBranch in case the caller needs it.
  Arm64Gen::FixupBranch GetEpilogFixup();
  // Register a conditional branch fixup to the epilog (e.g. CBNZ).
  void AddEpilogFixup(Arm64Gen::FixupBranch fb);
  void SetEpilogLabel();

  // ---- Label-aware branch helpers ----------------------------------------
  // Branch to a HIR label's owning block. Handles forward/backward resolution
  // via block_labels_ and pending_block_fixups_.
  void BranchLabel(hir::Label* label);
  void BranchLabelIfNZ(Arm64Gen::ARM64Reg cond, hir::Label* label);  // CBNZ
  void BranchLabelIfZ(Arm64Gen::ARM64Reg cond, hir::Label* label);   // CBZ

  // ---- Block label helpers (for inter-block branches in sequences) --------
  // Returns the code pointer for a block that has already been emitted, or
  // nullptr if the block hasn't been reached yet.
  const uint8_t* GetBlockAddress(hir::Block* block) const;
  // Register a forward-branch fixup for a block not yet emitted.
  // SetBlockLabel() (called when the block is reached) will resolve it.
  void AddBlockFixup(hir::Block* target, Arm64Gen::FixupBranch fb);

  bool IsFeatureEnabled(uint32_t flag) const {
    return (feature_flags_ & flag) != 0;
  }

  // Returns the current write offset from the start of this code block.
  // Dolphin's ARM64CodeBlock doesn't have GetCodeOffset() directly —
  // we compute it from GetCodePtr() minus the block start.
  size_t GetCodeOffset() const {
    return static_cast<size_t>(this->GetCodePtr() - region_start_);
  }

  // Called once at the start of each Emit() call to record the block base.
  void ResetCodePtr() {
    region_start_ = this->GetWritableCodePtr();
    // New Dolphin API: SetCodePtr(start, end). region + region_size = buffer end.
    this->SetCodePtr(region_start_, region + region_size);
  }

  FunctionDebugInfo* debug_info() const { return debug_info_; }
  size_t stack_size() const { return stack_size_; }

  // Scratch register helpers — x9/x10/x11 are caller-saved scratch.
  // These do NOT conflict with the allocatable set (x21-x27).
  static Arm64Gen::ARM64Reg ScratchReg(int index = 0);  // x9, x10, x11
  static Arm64Gen::ARM64Reg ScratchVec(int index = 0);  // v16, v17, v18

  // ---- Float/NEON forwarding wrappers --------------------------------------
  // These dispatch float/NEON instructions to fe_ (ARM64FloatEmitter).
  // Methods that exist on ARM64XEmitter (GPR) AND ARM64FloatEmitter (NEON)
  // with the same 3-arg signature get smart dispatch via IsGPR(Rd).

  // Smart-dispatch LDR/STR: GPR regs → base emitter, float/NEON → fe_.
  using ARM64XEmitter::LDR;
  using ARM64XEmitter::STR;
  void LDR(Arm64Gen::IndexType type, Arm64Gen::ARM64Reg Rt,
           Arm64Gen::ARM64Reg Rn, s32 imm) {
    if (Arm64Gen::IsGPR(Rt)) { ARM64XEmitter::LDR(type, Rt, Rn, imm); return; }
    uint8_t sz = Arm64Gen::IsSingle(Rt) ? 32 :
                 Arm64Gen::IsDouble(Rt)  ? 64 : 128;
    fe_.LDR(sz, type, Rt, Rn, imm);
  }
  void STR(Arm64Gen::IndexType type, Arm64Gen::ARM64Reg Rt,
           Arm64Gen::ARM64Reg Rn, s32 imm) {
    if (Arm64Gen::IsGPR(Rt)) { ARM64XEmitter::STR(type, Rt, Rn, imm); return; }
    uint8_t sz = Arm64Gen::IsSingle(Rt) ? 32 :
                 Arm64Gen::IsDouble(Rt)  ? 64 : 128;
    fe_.STR(sz, type, Rt, Rn, imm);
  }

  // Smart-dispatch AND/ORR/EOR/BIC/NOT: GPR → base, vector → fe_.
  using ARM64XEmitter::AND;
  using ARM64XEmitter::ORR;
  using ARM64XEmitter::EOR;
  using ARM64XEmitter::BIC;
  void AND(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsGPR(Rd))
      ARM64XEmitter::AND(Rd, Rn, Rm,
                         Arm64Gen::ArithOption(Rd, Arm64Gen::ShiftType::LSL, 0));
    else fe_.AND(Rd, Rn, Rm);
  }
  void AND(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn, uint64_t imm) {
    ANDI2R(Rd, Rn, imm, ScratchReg(2));
  }
  void ORR(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsGPR(Rd))
      ARM64XEmitter::ORR(Rd, Rn, Rm,
                         Arm64Gen::ArithOption(Rd, Arm64Gen::ShiftType::LSL, 0));
    else fe_.ORR(Rd, Rn, Rm);
  }
  void EOR(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsGPR(Rd))
      ARM64XEmitter::EOR(Rd, Rn, Rm,
                         Arm64Gen::ArithOption(Rd, Arm64Gen::ShiftType::LSL, 0));
    else fe_.EOR(Rd, Rn, Rm);
  }
  void EOR(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn, uint64_t imm) {
    EORI2R(Rd, Rn, imm, ScratchReg(2));
  }
  void BIC(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsGPR(Rd))
      ARM64XEmitter::BIC(Rd, Rn, Rm,
                         Arm64Gen::ArithOption(Rd, Arm64Gen::ShiftType::LSL, 0));
    else fe_.BIC(Rd, Rn, Rm);
  }
  void NOT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) { fe_.NOT(Rd, Rn); }
  void BSL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) { fe_.BSL(Rd, Rn, Rm); }

  // Smart-dispatch REV16/REV32/REV64: vector → fe_(size), GPR → base.
  using ARM64XEmitter::REV16;
  using ARM64XEmitter::REV32;
  using ARM64XEmitter::REV64;
  void REV16(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsGPR(Rd)) ARM64XEmitter::REV16(Rd, Rn);
    else fe_.REV16(8, Rd, Rn);
  }
  void REV32(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsGPR(Rd)) ARM64XEmitter::REV32(Rd, Rn);
    else fe_.REV32(8, Rd, Rn);
  }
  void REV64(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsGPR(Rd)) ARM64XEmitter::REV64(Rd, Rn);
    else fe_.REV64(8, Rd, Rn);
  }
  // REV — byte-reverse within a GPR word (32-bit) or doubleword (64-bit).
  void REV(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::Is64Bit(Rd))
      Write32(0xDAC00C00u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
    else
      Write32(0x5AC00800u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

  // FMOV: scalar and GPR↔float moves (all via fe_).
  void FMOV(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            bool top = false) { fe_.FMOV(Rd, Rn, top); }
  void FMOV(Arm64Gen::ARM64Reg Rd, uint8_t imm8) { fe_.FMOV(Rd, imm8); }
  void FMOV(Arm64Gen::ARM64Reg Rd, double imm) {
    uint64_t bits; std::memcpy(&bits, &imm, 8);
    MOVI2R(ScratchReg(1), bits);
    fe_.FMOV(Arm64Gen::EncodeRegToDouble(Rd), ScratchReg(1));
  }

  // Scalar float: 1-source (dispatch on S vs D via fe_; vector via raw encode)
  void FABS(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsVector(Rd)) fe_.FABS(32, Rd, Rn); else fe_.FABS(Rd, Rn);
  }
  void FNEG(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsVector(Rd)) fe_.FNEG(32, Rd, Rn); else fe_.FNEG(Rd, Rn);
  }
  void FSQRT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsVector(Rd))
      Write32(0x6EA1F800u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
    else fe_.FSQRT(Rd, Rn);
  }
  void FRINTI(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) { fe_.FRINTI(Rd,Rn); }
  void FRECPE(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsVector(Rd)) fe_.FRECPE(32, Rd, Rn); else fe_.FRECPE(Rd, Rn);
  }
  void FRSQRTE(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    if (Arm64Gen::IsVector(Rd)) fe_.FRSQRTE(32,Rd,Rn); else fe_.FRSQRTE(Rd,Rn);
  }

  // Scalar float rounding (absent from Dolphin FloatEmitter — raw encoded).
  // Encoding: (0x1E200000 | type<<22 | opcode<<15 | 1<<14 | Rn<<5 | Rd)
  // opcode: N=8, P=9, M=10, Z=11, I=15
  void FRINTN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x1E200000u|(Arm64Gen::IsDouble(Rd)?0x400000u:0u)|
            ( 8u<<15)|(1u<<14)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void FRINTZ(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x1E200000u|(Arm64Gen::IsDouble(Rd)?0x400000u:0u)|
            (11u<<15)|(1u<<14)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void FRINTM(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x1E200000u|(Arm64Gen::IsDouble(Rd)?0x400000u:0u)|
            (10u<<15)|(1u<<14)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void FRINTP(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x1E200000u|(Arm64Gen::IsDouble(Rd)?0x400000u:0u)|
            ( 9u<<15)|(1u<<14)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

  // Scalar float 2-source (dispatch S/D → scalar; Q → vector form).
  void FADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FADD(32,Rd,Rn,Rm); else fe_.FADD(Rd,Rn,Rm);
  }
  void FSUB(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FSUB(32,Rd,Rn,Rm); else fe_.FSUB(Rd,Rn,Rm);
  }
  void FMUL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FMUL(32,Rd,Rn,Rm); else fe_.FMUL(Rd,Rn,Rm);
  }
  void FDIV(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FDIV(32,Rd,Rn,Rm); else fe_.FDIV(Rd,Rn,Rm);
  }
  void FMAX(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FMAX(32,Rd,Rn,Rm); else fe_.FMAX(Rd,Rn,Rm);
  }
  void FMIN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    if (Arm64Gen::IsVector(Rd)) fe_.FMIN(32,Rd,Rn,Rm); else fe_.FMIN(Rd,Rn,Rm);
  }
  // FMADD: scalar 3-source only.
  void FMADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm, Arm64Gen::ARM64Reg Ra) {
    fe_.FMADD(Rd, Rn, Rm, Ra);
  }
  // FMLA: vector multiply-accumulate (size=32, 4xF32).
  void FMLA(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.FMLA(32, Rd, Rn, Rm); }

  // FRECPS / FRSQRTS: reciprocal step (absent from Dolphin — raw encoded).
  // Scalar: 0x1E20FC00 (single) / 0x1E60FC00 (double); Vector 4S: 0x4E20FC00 / 0x6E20FC00.
  void FRECPS(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
              Arm64Gen::ARM64Reg Rm) {
    uint32_t base = Arm64Gen::IsVector(Rd) ? 0x4E20FC00u :
                    (Arm64Gen::IsDouble(Rd) ? 0x1E60FC00u : 0x1E20FC00u);
    Write32(base|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void FRSQRTS(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
               Arm64Gen::ARM64Reg Rm) {
    uint32_t base = Arm64Gen::IsVector(Rd) ? 0x6E20FC00u :
                    (Arm64Gen::IsDouble(Rd) ? 0x5EE0FC00u : 0x5EA0FC00u);
    Write32(base|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

  // Float conversion: SCVTF (int→float), FCVTZS (float→int truncate), FCVT.
  void SCVTF(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) { fe_.SCVTF(Rd,Rn); }
  void FCVTZS(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.FCVTS(Rd, Rn, Arm64Gen::RoundingMode::Z);
  }
  // Vector (4×F32) packed int↔float conversions.
  void SCVTF_4S(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.SCVTF(32, Rd, Rn);
  }
  void UCVTF_4S(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.UCVTF(32, Rd, Rn);
  }
  void FCVTZS_4S(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.FCVTZS(32, Rd, Rn);
  }
  void FCVTZU_4S(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.FCVTZU(32, Rd, Rn);
  }
  void FCVT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    uint8_t sz_to   = Arm64Gen::IsSingle(Rd) ? 32 : 64;
    uint8_t sz_from = Arm64Gen::IsSingle(Rn) ? 32 : 64;
    fe_.FCVT(sz_to, sz_from, Rd, Rn);
  }

  // Float compare / conditional select.
  void FCMP(Arm64Gen::ARM64Reg Rn) { fe_.FCMP(Rn); }
  void FCMP(Arm64Gen::ARM64Reg Rn, Arm64Gen::ARM64Reg Rm) { fe_.FCMP(Rn, Rm); }
  void FCSEL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm, CCFlags cond) {
    fe_.FCSEL(Rd, Rn, Rm, cond);
  }

  // Vector integer compare (default element size = 32).
  void CMEQ(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.CMEQ(32,Rd,Rn,Rm); }
  void CMGT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.CMGT(32,Rd,Rn,Rm); }
  void CMGE(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.CMGE(32,Rd,Rn,Rm); }
  void CMHI(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.CMHI(32,Rd,Rn,Rm); }
  void CMHS(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) { fe_.CMHS(32,Rd,Rn,Rm); }
  void FCMEQ(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm) { fe_.FCMEQ(32,Rd,Rn,Rm); }
  void FCMGT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm) { fe_.FCMGT(32,Rd,Rn,Rm); }
  void FCMGE(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm) { fe_.FCMGE(32,Rd,Rn,Rm); }

  // Move/shuffle: INS, UMOV, DUP, EXT, MOVI.
  // 3-arg: GPR → element  (INS Vd.S[i], Wn)
  void INS(Arm64Gen::ARM64Reg Rd, uint8_t index, Arm64Gen::ARM64Reg Rn) {
    fe_.INS(32, Rd, index, Rn);
  }
  // 4-arg: element → element  (INS Vd.S[i], Vn.S[j])
  void INS(Arm64Gen::ARM64Reg Rd, uint8_t dst_idx,
           Arm64Gen::ARM64Reg Rn, uint8_t src_idx) {
    using namespace Arm64Gen;
    ARM64Reg qd = IsQuad(Rd) ? Rd : EncodeRegToQuad(Rd);
    ARM64Reg qn = IsQuad(Rn) ? Rn : EncodeRegToQuad(Rn);
    fe_.INS(32, qd, dst_idx, qn, src_idx);
  }

  // 4-arg MOV Vd.S[i], Vn.S[j] — alias for 4-arg INS.
  using ARM64XEmitter::MOV;
  void MOV(Arm64Gen::ARM64Reg Rd, uint8_t dst_idx,
           Arm64Gen::ARM64Reg Rn, uint8_t src_idx) {
    INS(Rd, dst_idx, Rn, src_idx);
  }

  // PRFM Rt, [Xn] — base register, offset 0 (unsigned offset form).
  using ARM64XEmitter::PRFM;
  void PRFM(Arm64Gen::ARM64Reg Rt, Arm64Gen::ARM64Reg Rn) {
    Write32(0xF9800000u | (Arm64Gen::DecodeReg(Rn) << 5) |
            (static_cast<uint32_t>(Rt) & 0x1Fu));
  }
  void UMOV(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn, uint8_t index) {
    fe_.UMOV(32, Rd, Rn, index);
  }
  void DUP(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.DUP(32, Rd, Rn);
  }
  void DUP(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn, uint8_t index) {
    Arm64Gen::ARM64Reg vn = Arm64Gen::IsGPR(Rn) ? Rn
                                                 : Arm64Gen::EncodeRegToQuad(Rn);
    fe_.DUP(32, Rd, vn, index);
  }
  void EXT(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm, uint32_t index) { fe_.EXT(Rd, Rn, Rm, index); }
  void MOVI(Arm64Gen::ARM64Reg Rd, uint64_t imm, uint8_t shift = 0) {
    fe_.MOVI(8, Rd, imm, shift);
  }

  // Narrowing: SQXTN/SQXTN2/UQXTN/UQXTN2 (Dolphin, default dest_size=16).
  // SQXTUN/SQXTUN2 absent from Dolphin — raw encoded (dest_size=8).
  void SQXTN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.SQXTN(16, Arm64Gen::IsVector(Rd) ? Arm64Gen::EncodeRegToDouble(Rd) : Rd, Rn);
  }
  void SQXTN2(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.SQXTN2(16, Rd, Rn);
  }
  void UQXTN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.UQXTN(16, Arm64Gen::IsVector(Rd) ? Arm64Gen::EncodeRegToDouble(Rd) : Rd, Rn);
  }
  void UQXTN2(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    fe_.UQXTN2(16, Rd, Rn);
  }
  void SQXTUN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x2E212800u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void SQXTUN2(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x6E212800u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

  // TBL: table vector lookup (absent from Dolphin — raw encoded).
  // 1-reg:  TBL Vd.16B, {Vn.16B}, Vm.16B = 0x4E000000
  // 2-reg:  TBL Vd.16B, {Vn.16B,Vm.16B}, Vk.16B = 0x4E002000
  void TBL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rm) {
    Write32(0x4E000000u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void TBL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
           Arm64Gen::ARM64Reg Rn2, Arm64Gen::ARM64Reg Rm) {
    (void)Rn2;  // Must equal Rn+1; encoded implicitly.
    Write32(0x4E002000u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

  // Missing NEON ops (absent from Dolphin — raw encoded via EmitThreeSame pattern).
  // Pattern: (Q<<30)|(U<<29)|0x0E200000|(size<<22)|(Rm<<16)|(opcode<<11)|(1<<10)|(Rn<<5)|Rd
  // All default to size=2 (32-bit elements, 4xI32 / 4xF32).
  void SSHL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x4EA04400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void USHL(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x6EA04400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void SMAX(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x4EA06400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void UMAX(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x6EA06400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void SMIN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x4EA06C00u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void UMIN(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
            Arm64Gen::ARM64Reg Rm) {
    Write32(0x6EA06C00u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void SQADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm) {
    Write32(0x4EA00C00u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void UQADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
             Arm64Gen::ARM64Reg Rm) {
    Write32(0x6EA00C00u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void SRHADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
              Arm64Gen::ARM64Reg Rm) {
    Write32(0x4EA01400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  void URHADD(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn,
              Arm64Gen::ARM64Reg Rm) {
    Write32(0x6EA01400u|(Arm64Gen::DecodeReg(Rm)<<16)|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }
  // UMAXV: unsigned max across all 32-bit lanes → result in lane 0.
  // Emit2RegMisc(Q=1, U=1, size=2, opcode=0x2A): 0x6EA2A800
  void UMAXV(Arm64Gen::ARM64Reg Rd, Arm64Gen::ARM64Reg Rn) {
    Write32(0x6EA2A800u|(Arm64Gen::DecodeReg(Rn)<<5)|Arm64Gen::DecodeReg(Rd));
  }

 protected:
  void* Emplace(const EmitFunctionInfo& func_info,
                GuestFunction* function = nullptr);
  bool EmitFunction(hir::HIRBuilder* builder, EmitFunctionInfo& func_info);

  // Emit function prolog: save callee-saved regs, set up frame pointer,
  // store context/membase to their home slots.
  void EmitFunctionProlog(const EmitFunctionInfo& func_info);
  // Allocate / free the current guest frame (stack_size_), saving/restoring
  // fp/lr. Handle frames larger than the pre-index STP/LDP immediate range.
  void EmitAllocFrame();
  void EmitFreeFrame();
  // Emit function epilog: restore callee-saved regs, return.
  void EmitFunctionEpilog();
  // Restore the allocatable callee-saved registers (x21-x27, v4-v15) saved in
  // the prolog. Must be emitted while the frame is still allocated (before the
  // LDP that deallocates it). Shared by the epilog and tail-call teardowns.
  void EmitRestoreCalleeSaved();

  // Emit the thunks that allow crossing the host/guest boundary.
  // Called once during backend initialization.
  bool EmitHostToGuestThunk();
  bool EmitGuestToHostThunk();
  bool EmitResolveFunctionThunk();

  // Build the NEON constant pool (called during backend init).
  static uintptr_t PlaceConstData();
  static void FreeConstData(uintptr_t data);
  friend class ARM64Backend;

 protected:
  // Float/NEON emitter — wraps this emitter for all NEON/float instructions.
  Arm64Gen::ARM64FloatEmitter fe_;

  // Pointer to the start of the current code block (set in ResetCodePtr).
  // Used to compute GetCodeOffset() as GetCodePtr() - region_start_.
  uint8_t* region_start_ = nullptr;

  Processor* processor_ = nullptr;
  ARM64Backend* backend_ = nullptr;
  ARM64CodeCache* code_cache_ = nullptr;
  uint32_t feature_flags_ = 0;

  // Epilog fixups — B() instructions emitted by RETURN sequences.
  // All resolved in SetEpilogLabel().
  std::vector<Arm64Gen::FixupBranch> epilog_fixups_;
  size_t epilog_offset_ = 0;

  // Block address map for the current function being compiled.
  // Keyed by hir::Block*; value is the code pointer at block start.
  std::unordered_map<hir::Block*, const uint8_t*> block_labels_;
  // Pending forward-branch fixups: block* → list of B() fixups targeting it.
  std::unordered_map<hir::Block*, std::vector<Arm64Gen::FixupBranch>>
      pending_block_fixups_;

  hir::Instr* current_instr_ = nullptr;
  // Guest PC of the current guest instruction (updated by MarkSourceOffset);
  // used by EmitStoreWatch to record the exact storing instruction.
  uint32_t current_guest_address_ = 0;

  FunctionDebugInfo* debug_info_ = nullptr;
  uint32_t debug_info_flags_ = 0;
  FunctionTraceData* trace_data_ = nullptr;
  Arena source_map_arena_;

  size_t stack_size_ = 0;

  // Pointer to the pre-allocated NEON constant pool (read-only data section).
  uintptr_t const_data_ = 0;

  // Thunk function pointers filled in by EmitHostToGuestThunk(),
  // EmitGuestToHostThunk(), EmitResolveFunctionThunk().
  // Read by ARM64Backend after thunk emission is complete.
  HostToGuestThunk   host_to_guest_thunk_ptr_   = nullptr;
  GuestToHostThunk   guest_to_host_thunk_ptr_   = nullptr;
  ResolveFunctionThunk resolve_function_thunk_ptr_ = nullptr;
};

}  // namespace arm64
}  // namespace backend
}  // namespace cpu
}  // namespace xe

#endif  // XE_ARCH_ARM64
#endif  // XENIA_CPU_BACKEND_ARM64_ARM64_EMITTER_H_
