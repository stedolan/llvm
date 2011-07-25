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

#include "StackSegmenter.h"
#include "PrologEpilogInserter.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetOptions.h"

#include <iostream>

using namespace llvm;

char StackSegmenter::ID = 0;

INITIALIZE_PASS_BEGIN(StackSegmenter, "stacksegmenter", "Stack Segmenter", true,
                      false)
INITIALIZE_PASS_DEPENDENCY(PEI)
INITIALIZE_PASS_END(StackSegmenter, "stacksegmenter", "Stack Segmenter", true,
                    false)

void StackSegmenter::getAnalysisUsage(AnalysisUsage &info) const {
  info.addRequired<PEI>();
  MachineFunctionPass::getAnalysisUsage(info);
}

bool StackSegmenter::runOnMachineFunction(MachineFunction &MF) {
  if (!EnableSegmentedStacks)
    return false;
  return true;
}

FunctionPass *llvm::createStackSegmenter() {
  return new StackSegmenter();
}
