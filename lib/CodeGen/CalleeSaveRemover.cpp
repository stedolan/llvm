//===-- CalleeSaveRemover.cpp - Callee-save register elimination pass -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the pass to remove unnecessary
// saving/restoring of calle-save registers across calls.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "calleesaveremover"
#include "CalleeSaveRemover.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

char CalleeSaveRemover::ID = 0;
INITIALIZE_PASS(CalleeSaveRemover, "callee-save-remover",
                "Callee-Save Remover", false, false)

char &llvm::CalleeSaveRemoverID = CalleeSaveRemover::ID;

FunctionPass* llvm::createCalleeSaveRemoverPass(){
  return new CalleeSaveRemover();
}

void CalleeSaveRemover::getAnalysisUsage(AnalysisUsage& Info) const{
  MachineFunctionPass::getAnalysisUsage(Info);
}

static bool shouldOmitCalleeSave(const GlobalValue* GV){
  const Function* F = dyn_cast_or_null<const Function>(GV);
  if (!F)
    return false; // not an LLVM function
  if (!F->hasFnAttr(Attribute::NoCalleeSave))
    return false; // not marked nocalleesave
  if (F->hasAddressTaken() || !F->hasLocalLinkage())
    return false; // can't safely change ABI of this function

  return true;
}

static bool shouldOmitCalleeSave(const MachineOperand& Op){
  return Op.isGlobal() && shouldOmitCalleeSave(Op.getGlobal());
}

static bool shouldOmitCalleeSave(const MachineFunction& MF){
  return shouldOmitCalleeSave(MF.getFunction());
}

bool CalleeSaveRemover::runOnMachineFunction(MachineFunction& MF){
  bool changed = false;
  for (MachineFunction::iterator MBI = MF.begin(), MBE = MF.end();
       MBI != MBE; ++MBI){
    MachineBasicBlock& MBB = *MBI;
    for (MachineBasicBlock::iterator MII = MBB.begin(), MIE = MBB.end();
         MII != MIE; ++MII){
      MachineInstr& MI = *MII;
      if (MI.getDesc().isCall()){
        const MachineOperand& callee = MI.getOperand(0);
        // FIXME FIXME FIXME
        // the first operand is only guaranteed to be the callee on
        // certain (most) platforms.
        // MIPS is a notable counterexample, so this breaks there.
        if (shouldOmitCalleeSave(callee)){
          // this call instruction won't preserve the usual
          // callee-save registers, so we mark them imp-def'd
          const TargetRegisterInfo* TRI = MF.getTarget().getRegisterInfo();
          assert(TRI);
          const unsigned* RegStart = TRI->getCalleeSavedRegs(&MF);
          for (const unsigned* I = RegStart; unsigned Reg = *I; ++I){
            MachineInstrBuilder(&MI).addReg(Reg, RegState::Define | RegState::Implicit);
          }
          changed = true;
        }
      }
    }
  }
  if (shouldOmitCalleeSave(MF)){
    MF.getFrameInfo()->setOmitCalleeSaveRegisters(true);
  }
  return changed;
}
