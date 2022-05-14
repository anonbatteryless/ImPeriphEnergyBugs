#include "int_concurrency.h"
#include "path_check.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/GenericDomTreeConstruction.h"
#include <string>
#include <vector>


using namespace llvm;


void find_isr_disEn(Module &M, disEnMapType &enables, disEnMapType &disables) {
  // Run through functions and extract enable/disable functions
  for (auto &F : M) {
    if (F.getName().endswith("_disable_")) {
      disables[F.getName().drop_back(9)] = &F;
      //disables.insert({F.getName().drop_back(8), F});
    }
    else if (F.getName().endswith("_enable_")) {
      enables[F.getName().drop_back(8)] = &F;
    }
  }
  //  Double check that there are matching enable/disable pairs and throw
  //  warning if not (we'll go through everything in disable and check if
  //  there's a corresponding enable)
  for (auto &it : disables) {
    if ( enables.find(it.first) == enables.end()) {
      outs() << "[WARNING] no matching enable function for " <<
      it.second->getName() << "\n";
      // This probably won't work...
      disables.erase(it.first);
    }
  }
  outs() << "disable functions are:\n";
  for (auto &it : disables) {
    outs() << it.second->getName() << "\n";
  }

  return;
}

#if 0
bool maybeEn(BasicBlock *B, Function *en, Function *dis, DominatorTree *DT) {
  // Get all uses of disable function
  for (auto UseI : dis->users) {
    if (DT->dominates(UseI.getParent(), B)) {
      // look for enable on path from dis to block

    }
  }
  return true;
}
#endif

bool containsFunc(Function *F, BasicBlock *B) {
  for (auto &it : *B) {
    Instruction *I = &it;
    if( isa<CallInst>(I) &&
          cast<CallInst>(I)->getCalledFunction() != NULL) { 
      if (cast<CallInst>(I)->getCalledFunction() == F) {
        return true;
      }
    }
  }
  return false;
}

// We just skip the immediate return on the current block because we know it's a
// disable
int checkDisable(BasicBlock *BB, Function *dis, Function *en,visitBlockType &bl)
{ int test, flag = 3;
  int count;
  count = 0;
  for (auto *B : predecessors(BB)) {
    if ( bl[B] == VISITED ) {
      continue;
    }
    outs() << "Looking through " << pred_size(BB) << " preds\n";
    outs() << "Going to pred on line: ";
    easy_print(B);
    outs() << "Running number " << count << "\n";
    count++;
    test = searchEn(B, dis,en, bl);
    outs() << "Test is : " << test << "\n";
    switch (test) {
      // Immediately end search if we have a 2
      case 2: outs() << "\tVal is 2\n"; return 2;
      // We got a disable, so go check all the other paths
      case 1: outs() << "\tVal is 1\n";flag = 1; break;
      // We found apath that reaches the front without hitting a disable, so
      // depending on the system semantics, interrupts *could* be enabled. For
      // now we'll return immediately, but that may not always be the desired
      // behavior.
      case 0: outs() << "\tVal is 0\n"; return 0;
      default: outs() << "Nothing interesting yet\n"; 
    }
  }
  return flag;
}


int searchEn(BasicBlock *BB, Function *dis, Function *en,visitBlockType &bl) {
  outs() << "looking at block on : ";
  easy_print(BB);
  bl[BB] = VISITED;

  // check for call to enable
  if (containsFunc(en, BB)) {
    outs() << "returning 2\n";
    return 2;
  }
  // check for call to disable
  if (containsFunc(dis, BB)) {
    outs() << "returning 1\n";
    return 1;
  }
  // check for front of function
  if (BB == &(BB->getParent()->front())) {
    outs() << "returning 0\n";
    return 0;
  }
  int test, flag = 3;
  int count;
  count = 0;
  for (auto *B : predecessors(BB)) {
    if ( bl[B] == VISITED ) {
      continue;
    }
    outs() << "Looking through " << pred_size(BB) << " preds\n";
    outs() << "Going to pred on line: ";
    easy_print(B);
    outs() << "Running number " << count << "\n";
    count++;
    test = searchEn(B, dis,en, bl);
    outs() << "Test is : " << test << "\n";
    switch (test) {
      // Immediately end search if we have a 2
      case 2: outs() << "\tVal is 2\n"; return 2;
      // We got a disable, so go check all the other paths
      case 1: outs() << "\tVal is 1\n";flag = 1; break;
      // We found apath that reaches the front without hitting a disable, so
      // depending on the system semantics, interrupts *could* be enabled. For
      // now we'll return immediately, but that may not always be the desired
      // behavior.
      case 0: outs() << "\tVal is 0\n"; return 0;
      default: outs() << "Nothing interesting yet\n"; 
    }
  }
  return flag;
}



