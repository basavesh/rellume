/**
 * This file is part of Rellume.
 *
 * (c) 2016-2019, Alexis Engelke <alexis.engelke@googlemail.com>
 *
 * Rellume is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Rellume is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Rellume.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 **/

#include "x86-64/lifter.h"
#include "x86-64/lifter-private.h"

#include "arch.h"
#include "facet.h"
#include "instr.h"
#include "regfile.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/Utils/Cloning.h>

/**
 * @{
 **/

namespace rellume::x86_64 {

bool LiftInstruction(const Instr& inst, FunctionInfo& fi, const LLConfig& cfg,
                     ArchBasicBlock& ab) noexcept {
    return Lifter(fi, cfg, ab).Lift(inst);
}

bool Lifter::Lift(const Instr& inst) {
    // Set new instruction pointer register
    SetIP(inst.end());

    // Add instruction marker
    if (cfg.instr_marker) {
        llvm::StringRef str_ref{reinterpret_cast<const char*>(&inst),
                                sizeof(FdInstr)};
        llvm::MDString* md = llvm::MDString::get(irb.getContext(), str_ref);
        llvm::Value* md_val = llvm::MetadataAsValue::get(irb.getContext(), md);
        irb.CreateCall(cfg.instr_marker, {AddrIPRel(), md_val});
    }

    // Check overridden implementations first.
    const auto& override = cfg.instr_overrides.find(inst.type());
    if (override != cfg.instr_overrides.end()) {
        CallExternalFunction(override->second);
        return true;
    }

    switch (inst.type()) {
    default:
        SetIP(inst.start(), /*nofold=*/true);
        return false;

    case FDI_NOP: /* do nothing */ break;
    case FDI_RDSSP: /* do nothing */ break;
    case FDI_ENDBR64: /* do nothing */ break;
#if 0
    // Fadec doesn't support MPX.
    // Intel MPX, behave as NOP on processors without support (SDM Vol 1, 17.4)
    case FDI_BNDLDX: /* do nothing */ break;
    case FDI_BNDMOV: /* do nothing */ break;
    case FDI_BNDCU: /* do nothing */ break;
    case FDI_BNDCL: /* do nothing */ break;
    case FDI_BNDSTX: /* do nothing */ break;
    case FDI_BNDCN: /* do nothing */ break;
    case FDI_BNDMK: /* do nothing */ break;
#endif

    case FDI_PUSH: LiftPush(inst); break;
    case FDI_PUSHF: LiftPushf(inst); break;
    case FDI_POPF: LiftPopf(inst); break;
    case FDI_POP: LiftPop(inst); break;
    case FDI_LEAVE: LiftLeave(inst); break;
    case FDI_CALL: LiftCall(inst); break;
    case FDI_RET: LiftRet(inst); break;
    case FDI_SYSCALL: LiftSyscall(inst); break;
    case FDI_CPUID: LiftCpuid(inst); break;
    case FDI_RDTSC: LiftRdtsc(inst); break;
    // case FDI_CRC32: NOT IMPLEMENTED
    // case FDI_UD2: Intentionally not implemented.

    case FDI_LAHF: StoreGpFacet(ArchReg::RAX, Facet::I8H, FlagAsReg(8)); break;
    case FDI_SAHF: FlagFromReg(GetReg(ArchReg::RAX, Facet::I8H)); break;

    case FDI_MOV: LiftMovgp(inst); break;
    case FDI_MOVABS: LiftMovgp(inst); break;
    case FDI_MOVZX: LiftMovzx(inst); break;
    case FDI_MOVSX: LiftMovgp(inst); break;
    // TODO: set non-temporal hint
    case FDI_MOVNTI: LiftMovgp(inst); break;
    case FDI_MOVBE: LiftMovbe(inst); break;
    case FDI_ADD: LiftArith(inst, /*sub=*/false); break;
    case FDI_ADC: LiftArith(inst, /*sub=*/false); break;
    case FDI_SUB: LiftArith(inst, /*sub=*/true); break;
    case FDI_SBB: LiftArith(inst, /*sub=*/true); break;
    case FDI_CMP: LiftArith(inst, /*sub=*/true); break;
    case FDI_XADD: LiftArith(inst, /*sub=*/false); break;
    case FDI_CMPXCHG: LiftCmpxchg(inst); break;
    case FDI_XCHG: LiftXchg(inst); break;
    case FDI_LEA: LiftLea(inst); break;
    case FDI_XLATB: LiftXlat(inst); break;
    case FDI_NOT: LiftNot(inst); break;
    case FDI_NEG: LiftNeg(inst); break;
    case FDI_INC: LiftIncDec(inst); break;
    case FDI_DEC: LiftIncDec(inst); break;
    case FDI_AND: LiftAndOrXor(inst, llvm::Instruction::And, llvm::AtomicRMWInst::And); break;
    case FDI_OR: LiftAndOrXor(inst, llvm::Instruction::Or, llvm::AtomicRMWInst::Or); break;
    case FDI_XOR: LiftAndOrXor(inst, llvm::Instruction::Xor, llvm::AtomicRMWInst::Xor); break;
    case FDI_TEST: LiftAndOrXor(inst, llvm::Instruction::And, llvm::AtomicRMWInst::And, /*wb=*/false); break;
    case FDI_IMUL: LiftMul(inst); break;
    case FDI_MUL: LiftMul(inst); break;
    case FDI_IDIV: LiftDiv(inst); break;
    case FDI_DIV: LiftDiv(inst); break;
    case FDI_SHL: LiftShift(inst, llvm::Instruction::Shl); break;
    case FDI_SHR: LiftShift(inst, llvm::Instruction::LShr); break;
    case FDI_SAR: LiftShift(inst, llvm::Instruction::AShr); break;
    case FDI_ROL: LiftRotate(inst); break;
    case FDI_ROR: LiftRotate(inst); break;
    case FDI_SHLD: LiftShiftdouble(inst); break;
    case FDI_SHRD: LiftShiftdouble(inst); break;
    case FDI_BSF: LiftBitscan(inst, /*trailing=*/true); break;
    case FDI_TZCNT: LiftBitscan(inst, /*trailing=*/true); break; // TODO: support TZCNT
    case FDI_BSR: LiftBitscan(inst, /*trailing=*/false); break;
    case FDI_LZCNT: LiftBitscan(inst, /*trailing=*/false); break; // TODO: support LZCNT
    case FDI_POPCNT: LiftPopcnt(inst); break;
    case FDI_BT: LiftBittest(inst, llvm::Instruction::Or, llvm::AtomicRMWInst::Or); break;
    case FDI_BTC: LiftBittest(inst, llvm::Instruction::Xor, llvm::AtomicRMWInst::Xor); break;
    case FDI_BTR: LiftBittest(inst, llvm::Instruction::And, llvm::AtomicRMWInst::And); break;
    case FDI_BTS: LiftBittest(inst, llvm::Instruction::Or, llvm::AtomicRMWInst::Or); break;
    case FDI_BSWAP: LiftBswap(inst); break;
    case FDI_C_EX: LiftCext(inst); break;
    case FDI_C_SEP: LiftCsep(inst); break;

    case FDI_CLC: SetReg(ArchReg::CF, irb.getFalse()); break;
    case FDI_STC: SetReg(ArchReg::CF, irb.getTrue()); break;
    case FDI_CMC: SetReg(ArchReg::CF, irb.CreateNot(GetFlag(ArchReg::CF))); break;

    case FDI_CLD: SetReg(ArchReg::DF, irb.getFalse()); break;
    case FDI_STD: SetReg(ArchReg::DF, irb.getTrue()); break;
    case FDI_LODS: LiftLods(inst); break;
    case FDI_STOS: LiftStos(inst); break;
    case FDI_MOVS: LiftMovs(inst); break;
    case FDI_SCAS: LiftScas(inst); break;
    case FDI_CMPS: LiftCmps(inst); break;

    case FDI_CMOVO: LiftCmovcc(inst, Condition::O); break;
    case FDI_CMOVNO: LiftCmovcc(inst, Condition::NO); break;
    case FDI_CMOVC: LiftCmovcc(inst, Condition::C); break;
    case FDI_CMOVNC: LiftCmovcc(inst, Condition::NC); break;
    case FDI_CMOVZ: LiftCmovcc(inst, Condition::Z); break;
    case FDI_CMOVNZ: LiftCmovcc(inst, Condition::NZ); break;
    case FDI_CMOVBE: LiftCmovcc(inst, Condition::BE); break;
    case FDI_CMOVA: LiftCmovcc(inst, Condition::A); break;
    case FDI_CMOVS: LiftCmovcc(inst, Condition::S); break;
    case FDI_CMOVNS: LiftCmovcc(inst, Condition::NS); break;
    case FDI_CMOVP: LiftCmovcc(inst, Condition::P); break;
    case FDI_CMOVNP: LiftCmovcc(inst, Condition::NP); break;
    case FDI_CMOVL: LiftCmovcc(inst, Condition::L); break;
    case FDI_CMOVGE: LiftCmovcc(inst, Condition::GE); break;
    case FDI_CMOVLE: LiftCmovcc(inst, Condition::LE); break;
    case FDI_CMOVG: LiftCmovcc(inst, Condition::G); break;

    case FDI_SETO: LiftSetcc(inst, Condition::O); break;
    case FDI_SETNO: LiftSetcc(inst, Condition::NO); break;
    case FDI_SETC: LiftSetcc(inst, Condition::C); break;
    case FDI_SETNC: LiftSetcc(inst, Condition::NC); break;
    case FDI_SETZ: LiftSetcc(inst, Condition::Z); break;
    case FDI_SETNZ: LiftSetcc(inst, Condition::NZ); break;
    case FDI_SETBE: LiftSetcc(inst, Condition::BE); break;
    case FDI_SETA: LiftSetcc(inst, Condition::A); break;
    case FDI_SETS: LiftSetcc(inst, Condition::S); break;
    case FDI_SETNS: LiftSetcc(inst, Condition::NS); break;
    case FDI_SETP: LiftSetcc(inst, Condition::P); break;
    case FDI_SETNP: LiftSetcc(inst, Condition::NP); break;
    case FDI_SETL: LiftSetcc(inst, Condition::L); break;
    case FDI_SETGE: LiftSetcc(inst, Condition::GE); break;
    case FDI_SETLE: LiftSetcc(inst, Condition::LE); break;
    case FDI_SETG: LiftSetcc(inst, Condition::G); break;

    // Jumps are handled in the basic block generation code.
    case FDI_JMP: LiftJmp(inst); break;
    case FDI_JO: LiftJcc(inst, Condition::O); break;
    case FDI_JNO: LiftJcc(inst, Condition::NO); break;
    case FDI_JC: LiftJcc(inst, Condition::C); break;
    case FDI_JNC: LiftJcc(inst, Condition::NC); break;
    case FDI_JZ: LiftJcc(inst, Condition::Z); break;
    case FDI_JNZ: LiftJcc(inst, Condition::NZ); break;
    case FDI_JBE: LiftJcc(inst, Condition::BE); break;
    case FDI_JA: LiftJcc(inst, Condition::A); break;
    case FDI_JS: LiftJcc(inst, Condition::S); break;
    case FDI_JNS: LiftJcc(inst, Condition::NS); break;
    case FDI_JP: LiftJcc(inst, Condition::P); break;
    case FDI_JNP: LiftJcc(inst, Condition::NP); break;
    case FDI_JL: LiftJcc(inst, Condition::L); break;
    case FDI_JGE: LiftJcc(inst, Condition::GE); break;
    case FDI_JLE: LiftJcc(inst, Condition::LE); break;
    case FDI_JG: LiftJcc(inst, Condition::G); break;
    case FDI_JCXZ: LiftJcxz(inst); break;
    case FDI_LOOP: LiftLoop(inst); break;
    case FDI_LOOPZ: LiftLoop(inst); break;
    case FDI_LOOPNZ: LiftLoop(inst); break;
    }

    return true;
}

} // namespace rellume::x86_64

/**
 * @}
 **/
