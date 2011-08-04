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
// saving/restoring of callee-save registers across calls.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_CALLEESAVEREMOVER_H
#define LLVM_CODEGEN_CALLEESAVEREMOVER_H

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

struct CalleeSaveRemover : public MachineFunctionPass {
  static char ID;
  CalleeSaveRemover() : MachineFunctionPass(ID) {
    initializeCalleeSaveRemoverPass(*PassRegistry::getPassRegistry());
  }

  const char* getPassName() const {
    return "Callee save register remover";
  }

  virtual void getAnalysisUsage(AnalysisUsage& Info) const;
  virtual bool runOnMachineFunction(MachineFunction&);
};

} // end namespace llvm

#endif // LLVM_CODEGEN_CALLEESAVEREMOVER_H
