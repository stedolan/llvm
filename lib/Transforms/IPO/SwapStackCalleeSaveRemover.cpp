//===-- SwapStackCalleeSaveRemover.cpp - Mark functions nocalleesave ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the pass to mark functions which context-switch as
// "nocalleesave", which reduces register spilling for swapstack.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sscalleesaveremover"

#include "llvm/Transforms/IPO.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/CallGraphSCCPass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Attributes.h"
#include "llvm/Support/ErrorHandling.h"
#include <queue>
using namespace llvm;
using std::queue;

namespace{
  struct SwapStackCalleeSaveRemover : public CallGraphSCCPass {
    static char ID;
    DenseSet<Function*> CallsSwapStackFunc;
    DenseMap<Function*, bool> memoCanChangeABI;
    SwapStackCalleeSaveRemover() : CallGraphSCCPass(ID) {
      initializeSwapStackCalleeSaveRemoverPass(*PassRegistry::getPassRegistry());
    }
    bool runOnSCC(CallGraphSCC&);

    bool canChangeABI(Function* F);
  };
}
char SwapStackCalleeSaveRemover::ID = 0;
INITIALIZE_PASS(SwapStackCalleeSaveRemover, "sscsremover",
                "Remove callee-save registers in context-switching functions", 
                false, false)

Pass* llvm::createSwapStackCalleeSaveRemoverPass() {
  return new SwapStackCalleeSaveRemover();
}

static bool containsLocalSwapStack(Function& F){
  for (Function::iterator I = F.begin(), E = F.end(); I != E; ++I){
    BasicBlock& BB = *I;
    for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI){
      if (CallInst* CI = dyn_cast<CallInst>(&(*BI))){
        if (CI->getCallingConv() == CallingConv::SwapStack){
          return true;
        }
      }
    }
  }
  return false;
}

static bool usedOnlyInCallOrNewstack(Value* v){
  bool ret = true;
  for (Value::use_iterator UI = v->use_begin(), UE = v->use_end();
       ret && UI != UE; ++UI){
    Use& U = UI.getUse();
    if (CallInst* CI = dyn_cast<CallInst>(U.getUser())){
      if (&CI->getCalledUse() == &U){
        // direct call
        continue;
      }else if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(CI)){
        if (II->getIntrinsicID() == Intrinsic::newstack){
          // used in newstack
          continue;
        }else{
          // used in a call to some other intrinsic
          ret = false;
        }
      }else{
        // used in a call, but passed as an argument
        // thus, address is taken
        ret = false;
      }
    }else if (CastInst* CSTI = dyn_cast<CastInst>(U.getUser())){
      // cast, check the uses of the cast
      ret &= usedOnlyInCallOrNewstack(CSTI);
    }else if (ConstantExpr* CEXP = dyn_cast<ConstantExpr>(U.getUser())){
      // check if it's a cast constexpr and the uses are OK
      ret &= CEXP->isCast() && usedOnlyInCallOrNewstack(CEXP);
    }else{
      // address taken
      ret = false;
    }
  }
  return ret;
}

bool SwapStackCalleeSaveRemover::canChangeABI(Function* F){
  // We can mark a function as nocalleesave if:
  //  - it has local linkage
  //  - it is only "call"ed, never "invoke"d
  //  - its address is not taken except in calls to @llvm.newstack
  // FIXME: we could probably relax condition 2
  if (!F->hasLocalLinkage()) return false;
  

  // Memoize
  if (memoCanChangeABI.count(F)){
    return memoCanChangeABI[F];
  }else{
    bool b = usedOnlyInCallOrNewstack(F);
    memoCanChangeABI.insert(std::make_pair(F, b));
    return b;
  }
}

template <class T>
static void mark(T* obj, DenseSet<T*>& alreadySeen, queue<T*>& worklist){
  if (!alreadySeen.count(obj)){
    // object has changed state, add to worklist
    DEBUG(dbgs() << "Marking " << obj->getName() << "\n");
    alreadySeen.insert(obj);
    worklist.push(obj);
  }
}

bool SwapStackCalleeSaveRemover::runOnSCC(CallGraphSCC& SCC){
  DenseSet<Function*> CurrentSCC;

  DenseSet<BasicBlock*> SwapStackBeforeEnd;

  // Worklist of newly-changed basicblocks
  queue<BasicBlock*> BBWork;

  // Worklist of newly-changed functions
  queue<Function*> FuncWork;


  DEBUG(dbgs() << "SwapStack SCC: ");
  for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I){
    Function* F = (*I)->getFunction();
    if (F){
      CurrentSCC.insert(F);
      if (containsLocalSwapStack(*F) && canChangeABI(F))
        mark(F, CallsSwapStackFunc, FuncWork);
      DEBUG(dbgs() << F->getName() << " ");

      for (Function::iterator II = F->begin(), IE = F->end();
           II != IE; ++II){
        for (BasicBlock::iterator BI = II->begin(), BE = II->end();
             BI != BE; ++BI){
          if (CallInst* CI = dyn_cast<CallInst>(&*BI)){
            if (CI->getCalledFunction() && 
                CallsSwapStackFunc.count(CI->getCalledFunction()))
              mark(CI->getParent(), SwapStackBeforeEnd, BBWork);
          }
        }
      }
    }
  }
  DEBUG(dbgs() << "\n");


  while (FuncWork.size() + BBWork.size() > 0){
    if (!FuncWork.empty()){
      Function* F = FuncWork.front();
      FuncWork.pop();
      assert(CallsSwapStackFunc.count(F));
      
      // Check whether any callers of F have become SwapStackBeforeEnd
      SmallVector<BasicBlock*,4> callsites;
      assert(canChangeABI(F));
      for (Value::use_iterator UI = F->use_begin(), UE = F->use_end();
           UI != UE; ++UI){
        CallSite CS(*UI);//FIXME
        if (CS){
          CallInst* call = cast<CallInst>(CS.getInstruction());
          if (&call->getCalledUse() == &UI.getUse()){
            mark(call->getParent(), SwapStackBeforeEnd, BBWork);
          }
        }
      }
    }else if (!BBWork.empty()){
      BasicBlock* BB = BBWork.front();
      BBWork.pop();
      assert(SwapStackBeforeEnd.count(BB));

      // Check whether any successors of BB have become SwapStackBeforeEnd
      for (succ_iterator BI = succ_begin(BB), BE = succ_end(BB);
           BI != BE; ++BI){
        if (!SwapStackBeforeEnd.count(*BI)){
          // check if all of *BI's predecessors are now SwapStackBeforeEnd
          bool allPred = true;
          for (pred_iterator PI = pred_begin(*BI), PE = pred_end(*BI);
               PI != PE; ++PI){
            allPred &= SwapStackBeforeEnd.count(*PI);
            if (!allPred) break;
          }
          if (allPred)
            mark(*BI, SwapStackBeforeEnd, BBWork);
        }
      }

      // Check whether the function containing BB has become CallsSwapStack
      Function* F = BB->getParent();
      if (!CallsSwapStackFunc.count(F) && 
          isa<ReturnInst>(BB->getTerminator()) && 
          canChangeABI(F)){
        // check to see whether all of the function's exit blocks
        // are marked
        bool allMarked = true;
        for (Function::iterator FI = F->begin(), FE = F->end();
             allMarked && FI != FE; ++FI){
          if (isa<ReturnInst>(FI->getTerminator())){
            allMarked &= SwapStackBeforeEnd.count(&*FI);
          }
        }
        if (allMarked)
          mark(F, CallsSwapStackFunc, FuncWork);
      }
    }else llvm_unreachable(0);
  }


  bool changed = false;
  for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I){
    Function* F = (*I)->getFunction();
    if (F && CallsSwapStackFunc.count(F)){
      DEBUG(dbgs() << F->getName() << " is nocalleesave\n");
      assert(F->hasLocalLinkage());
      F->addFnAttr(Attribute::NoCalleeSave);
      assert(F->hasFnAttr(Attribute::NoCalleeSave));
      changed = true;
    }
  }
  return changed;
}
