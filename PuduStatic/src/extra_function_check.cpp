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
#include "extra_function_check.h"
#include "path_check.h"

#ifndef VISITED
#define VISITED 1
#endif

using namespace llvm;

int isTopLevelCall(CallInst * CI, visitCallType &cl, BasicBlocks &bls) {
  cl[CI] = VISITED;
  outs() << "Starting top level check\r\n";
  if (CI == NULL) {
    return 0;
  }
  if (CI->getParent() == NULL) {
    outs() << "Parent is nll!\r\n";
    return 0;
  }
  outs() << CI->getParent()->getParent()->getName() << "\n";
  // Add call block into list of basicblocks
  if(CI->getParent()->getParent()->getName().startswith("task")) {
    outs() << "Adding from task on line ";
    easy_print(CI->getParent());
    bls.push_back(CI->getParent());
    return 1;
  }
  if(CI->getParent()->getParent()->getName().startswith("main")) {
    outs() << "Adding from main on line ";
    bls.push_back(CI->getParent());
    return 1;
  }
  int temp = 0;
  //for (auto *it : CI->getCalledFunction()->users()) {
  for (auto *it : CI->getParent()->getParent()->users()) {
    if (!isa<CallInst>(it)) {
      continue;
    }
    if (cl[cast<CallInst>(it)] == VISITED) {
      continue;
    }
    temp = isTopLevelCall(cast<CallInst>(it),cl,bls);
  }
  return temp;
}

#ifdef TEST_EXTRA
void check_extra_functions(Module &M, blockInstMap allowedStates,callMapType
  &allPossibleStates) {
  BasicBlocks blocksCalledFrom;
  // Loop through all of the extra functions and descend into non-driver,
  // non-tasks functions... we'll deal with drivers later
  for (auto &F : M) {
    blocksCalledFrom.clear();
    if (F.getName().startswith("task") || F.getName().startswith("llvm")) {
      continue;
    }
    // Find use so we can feed it into isTopLevelCall
    CallInst *cur;
    cur = NULL;
    outs() << "Running check on " << F.getName() << "\r\n";
    int count;
    count = 0;
    for (auto *it : F.users()) {
      if (isa<CallInst>(it)) {
        count++;
        outs() << "Casting\r\n";
        cur = cast<CallInst>(it); 
        // Check if this is a top function
        visitCallType visits;
        visits.clear();
        isTopLevelCall(cur, visits, blocksCalledFrom);
      }
    }
    outs() << count << "possible users\r\n";
    // Use the output list of basicblocks to figure out the entire map of
    // possible states
    outs() << F.getName() << "called from " << blocksCalledFrom.size() <<
    "sites\n";
    Function *f = &F;
    for (auto it2 : blocksCalledFrom) {
      allPossibleStates[f][it2] = allowedStates[it2];
    }
  }
}

#endif
