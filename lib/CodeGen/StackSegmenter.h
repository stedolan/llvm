//===-- StackSegmenter.h - Prolog/Epilog code insertion -------*- C++ -* --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass is responsible for injecting the code required to have a
// MachineFunction support segmented stacks.
//
// This pass can only be run after the prologues and epilogues have been
// inserted.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_STACK_SEG_H
#define LLVM_CODEGEN_STACK_SEG_H

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetRegisterInfo.h"

namespace llvm {
  class StackSegmenter : public MachineFunctionPass {
  public:
    static char ID;
    StackSegmenter() : MachineFunctionPass(ID) {
      initializeStackSegmenterPass(*PassRegistry::getPassRegistry());
    }

    const char *getPassName() const {
      return "Stack segmenter";
    }

    bool runOnMachineFunction(MachineFunction &);
    void getAnalysisUsage(AnalysisUsage &) const;
  };
}

#endif
