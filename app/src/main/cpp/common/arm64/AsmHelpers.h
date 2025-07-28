// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"

#include "vixl/aarch64/constants-aarch64.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <unordered_map>

namespace a64 = vixl::aarch64;

#define RWARG1 a64::w0
#define RWARG2 a64::w1
#define RWARG3 a64::w2
#define RWARG4 a64::w3
#define RXARG1 a64::x0
#define RXARG2 a64::x1
#define RXARG3 a64::x2
#define RXARG4 a64::x3

#define RXVIXLSCRATCH a64::x16
#define RWVIXLSCRATCH a64::w16
#define RSCRATCHADDR a64::x17

#define RQSCRATCH a64::q30
#define RDSCRATCH a64::d30
#define RSSCRATCH a64::s30
#define RQSCRATCH2 a64::q31
#define RDSCRATCH2 a64::d31
#define RSSCRATCH2 a64::s31
#define RQSCRATCH3 a64::q29
#define RDSCRATCH3 a64::d29
#define RSSCRATCH3 a64::s29

#define EAX a64::w0
#define ECX a64::w1
#define EDX a64::w2
#define EBX a64::w3
#define EEX a64::w4

#define RAX a64::x0
#define RCX a64::x1
#define RDX a64::x2
#define RBX a64::x3
#define REX a64::x4

#define RSTATE_x19 a64::x19
#define RSTATE_x20 a64::x20
#define RSTATE_x21 a64::x21
#define RSTATE_x22 a64::x22
#define RSTATE_x23 a64::x23
#define RSTATE_x24 a64::x24

#define RFASTMEMBASE a64::x25

// microVU
#define RSTATE_MVU a64::x27
#define RSTATE_VU1 a64::x28
#define RSTATE_VUR a64::x26
#define PTR_MVU(field) a64::MemOperand(RSTATE_MVU, offsetof(microVU, field))
#define PTR_VU1(field) a64::MemOperand(RSTATE_VU1, offsetof(VURegs, field))
#define PTR_VUR(field) a64::MemOperand(RSTATE_VUR, offsetof(VURegs, field))

// CPU(iR5900), PSX(iR3000A), FPU(iFPU, iFPUd)
#define RSTATE_FPU a64::x27
#define RSTATE_PSX a64::x28
#define RSTATE_CPU a64::x29
#define PTR_FPU(field) a64::MemOperand(RSTATE_FPU, offsetof(fpuRegisters, field))
#define PTR_PSX(field) a64::MemOperand(RSTATE_PSX, offsetof(psxRegisters, field))
#define PTR_CPU(field) a64::MemOperand(RSTATE_CPU, offsetof(cpuRegisters, field))


static inline s64 GetPCDisplacement(const void* current, const void* target)
{
    return static_cast<s64>((reinterpret_cast<ptrdiff_t>(target) - reinterpret_cast<ptrdiff_t>(current)) >> 2);
}

const a64::Register& armWRegister(int n);
const a64::Register& armXRegister(int n);
const a64::VRegister& armSRegister(int n);
const a64::VRegister& armDRegister(int n);
const a64::VRegister& armQRegister(int n);

class ArmConstantPool;

static const u32 SP_SCRATCH_OFFSET = 0;

extern thread_local a64::MacroAssembler* armAsm;
extern thread_local u8* armAsmPtr;
extern thread_local size_t armAsmCapacity;
extern thread_local ArmConstantPool* armConstantPool;

static __fi bool armHasBlock()
{
    return (armAsm != nullptr);
}

static __fi u8* armGetCurrentCodePointer()
{
    return static_cast<u8*>(armAsmPtr) + armAsm->GetCursorOffset();
}

__fi static u8* armGetAsmPtr()
{
    return armAsmPtr;
}

void armSetAsmPtr(void* ptr, size_t capacity, ArmConstantPool* pool);
void armAlignAsmPtr();
u8* armStartBlock();
u8* armEndBlock();

void armDisassembleAndDumpCode(const void* ptr, size_t size);
void armEmitJmp(const void* ptr, bool force_inline = false);
void armEmitCall(const void* ptr, bool force_inline = false);
void armEmitCbnz(const a64::Register& reg, const void* ptr);
void armEmitCondBranch(a64::Condition cond, const void* ptr);
void armMoveAddressToReg(const a64::Register& reg, const void* addr);
void armLoadPtr(const a64::CPURegister& reg, const void* addr);
void armStorePtr(const a64::CPURegister& reg, const void* addr);
void armBeginStackFrame(bool save_fpr=true);
void armEndStackFrame(bool save_fpr=true);
bool armIsCalleeSavedRegister(int reg);

bool armIsCallerSaved(int id);
bool armIsCallerSavedXmm(int id);

a64::MemOperand armOffsetMemOperand(const a64::MemOperand& op, s64 offset);
void armGetMemOperandInRegister(const a64::Register& addr_reg,
                                const a64::MemOperand& op, s64 extra_offset = 0);

void armLoadConstant128(const a64::VRegister& reg, const void* ptr);

// may clobber RSCRATCH/RSCRATCH2. they shouldn't be inputs.
void armEmitVTBL(const a64::VRegister& dst, const a64::VRegister& src1,
                 const a64::VRegister& src2, const a64::VRegister& tbl);

//////////////////////////////////////////////////////////////////////////

class ArmConstantPool
{
public:
    void Init(void* ptr, u32 capacity);
    void Destroy();
    void Reset();

    u8* GetJumpTrampoline(const void* target);
    u8* GetLiteral(u64 value);
    u8* GetLiteral(const u128& value);
    u8* GetLiteral(const u8* bytes, size_t len);

    void EmitLoadLiteral(const a64::CPURegister& reg, const u8* literal) const;

private:
    __fi u32 GetRemainingCapacity() const { return m_capacity - m_used; }

    struct u128_hash
    {
        std::size_t operator()(const u128& v) const
        {
            std::size_t s = 0;
            HashCombine(s, v.lo, v.hi);
            return s;
        }
    };

    std::unordered_map<const void*, u32> m_jump_targets;
    std::unordered_map<u128, u32, u128_hash> m_literals;

    u8* m_base_ptr = nullptr;
    u32 m_capacity = 0;
    u32 m_used = 0;
};

//////////////////////////////////////////////////////////////////////////

void armBind(a64::Label* p_label);
u32 armEmitJmpPtr(void* code, const void* dst, bool flush_icache=true);

a64::Register armLoadPtr(const void* addr);
a64::Register armLoadPtr64(const void* addr);
a64::Register armLdrh(const void* addr);
a64::Register armLdrsh(const void* addr);
a64::Register armLoadPtr(const a64::MemOperand offset);
void armLoadPtr(const a64::CPURegister& reg, const void* addr, int64_t offset);
a64::Register armLoadPtr(a64::Register regRs, int64_t offset);
void armLoadPtr(const a64::CPURegister& regRt, a64::Register regRs, int64_t offset);
void armLoadPtr(uint64_t imm, const void* addr, const a64::Register& reg=EEX);
void armLoadPtr(uint64_t imm, a64::Register regRs, int64_t offset, const a64::Register& regRt=EEX);
a64::VRegister armLoadPtrV(const void* addr);
a64::VRegister armLoadPtrM(a64::Register regRs, int64_t offset=0);
void armStorePtr(const a64::CPURegister& reg, const void* addr, int64_t offset);
void armStorePtr(const a64::CPURegister& regRt, a64::Register regRs, int64_t offset);
void armStorePtr(uint64_t imm, const void* addr, const a64::Register& reg=EEX);
void armStorePtr(uint64_t imm, a64::MemOperand offset, const a64::Register& reg=EEX);
void armStorePtr(uint64_t imm, a64::Register regRs, int64_t offset, const a64::Register& regRt=EEX);
a64::MemOperand armMemOperandPtr(const void* addr);

void armLoad(const a64::Register& regRt, a64::MemOperand offset);
void armLoadh(const a64::Register& regRt, a64::MemOperand offset);
void armLoadsh(const a64::Register& regRt, a64::MemOperand offset);
void armLoadsw(const a64::Register& regRt, a64::MemOperand offset);
void armLoad(const a64::VRegister& regRt, a64::MemOperand offset);
a64::Register armLoad(a64::MemOperand offset);
a64::Register armLoad64(a64::MemOperand offset);
a64::VRegister armLoadPtrV(a64::MemOperand offset);
void armStore(a64::MemOperand offset, const a64::Register& regRt);
void armStoreh(a64::MemOperand offset, const a64::Register& regRt);
void armStore(a64::MemOperand offset, const a64::VRegister& regRt);
void armStore(a64::MemOperand offset, uint64_t imm);
void armStore64(a64::MemOperand offset, uint64_t imm);

void armAdd(a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armAdd(const a64::Register& p_reg, a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);

void armAdd(const void* p_mop, a64::Operand p_value);
void armAdd(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value);
void armAddh(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armAddsh(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value, bool p_flagUpdate=false);

void armSub(a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armSub(const a64::Register& p_reg, a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armSub(const void* p_mop, a64::Operand p_value);
void armSub(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value);

void armAnd(a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armAnd(const a64::Register& p_reg, a64::MemOperand p_mop, a64::Operand p_value, bool p_flagUpdate=false);
void armAnd(const void* p_mop, a64::Operand p_value);
void armAnd(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value);

void armOrr(a64::MemOperand p_mop, a64::Operand p_value);
void armOrr(const a64::Register& p_reg, a64::MemOperand p_mop, a64::Operand p_value);
void armOrr(const void* p_mop, a64::Operand p_value);
void armOrr(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value);

void armEor(a64::MemOperand p_mop, a64::Operand p_value);
void armEor(const a64::Register& p_reg, a64::MemOperand p_mop, a64::Operand p_value);
void armEor(const void* p_mop, a64::Operand p_value);
void armEor(const a64::Register& p_reg, const void* p_mop, a64::Operand p_value);

void armMOVMSKPS(const a64::Register& reg32, const a64::VRegister& regQ);
void armPBLENDW(const a64::VRegister& regDst, const a64::VRegister& regSrc);
void armPACKSSWB(const a64::VRegister& regDst, const a64::VRegister& regSrc);
void armPMOVMSKB(const a64::Register& regDst, const a64::VRegister& regSrc);

//////////////////////////////////////////////////////////////////////////

void armSHUFPS(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int pIndex);
void armPSHUFD(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int pIndex);
void armShuffleTblx(const a64::VRegister& p_dst, const a64::VRegister& p_src, int p_a, int p_b, int p_c, int p_d, bool p_is_tbx);
void armShuffle(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int pIndex, bool p_is_tbx);
