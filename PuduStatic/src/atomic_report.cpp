#include "auto_activation.h"
#include "dataflow.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ilist.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/GenericDomTreeConstruction.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/ADT/APInt.h"

#include <string>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#define CSV_IO_NO_THREAD
#include "csv.h"
#include "tasks.h"
#include "inference.h"

int report_problem_typestates(blockInstMap &blockMap, BasicBlock *BB) {
  periphStateMap periphStates;
  int reportedVal;
	// Walk through peripherals
	for (auto ItP: blockMap[BB]) {
    outs() << "\tChecking " << ItP.first;
    // If there's more than one assignment
    for (auto ItI: ItP.second) {
      // Walk through all assignments and drop into vector
      Value *test;
      test = ItI->getOperand(0);
      reportedVal = check_if_constant(test);
      // Scrub duplicates
      if (count(periphStates[ItP.first].begin(),
        periphStates[ItP.first].end(),reportedVal) == 0) {
        periphStates[ItP.first].push_back(reportedVal);
      }
    }
    // If there's more than one possible typestate
    if (periphStates[ItP.first].size() > 1) {
      outs() << ": WARNING, multiple state definitions\n";
      for (auto ItI: ItP.second) {
        Value *test;
        test = ItI->getOperand(0);
        reportedVal = check_if_constant(test);
        if (DILocation *Loc = ItI->getDebugLoc()) {
          unsigned Line = Loc->getLine();
          auto *Scope = cast<DIScope>(Loc->getScope());
          std::string fileName = Scope->getFilename();
          outs() << "\t\tLoc: " << fileName << ": " << Line  <<
            " : Value :"<< reportedVal <<"\n";
        }
        else {
          outs() << "No debug info! " << "Value : " << reportedVal << "\n";
        }
      }
    }
    else {
      outs() << ": OK, state is " << periphStates[ItP.first].front() << "\n";
    }
	}
	return 0;
}

void report_atomic_typestates(Function *F,blockInstMap &states) {
  //Check if atomic_start call
  //TODO make this not hardcoded
	if (F->getName().startswith("start_atomic")) {
    //Walk through Callsites
    for (User *U : F->users()) {
      Instruction *I = dyn_cast<Instruction>(U);
      if (I == NULL) {
        continue;
      }
      outs() << "Checking atomic block starting on line:";
      BasicBlock *B = I->getParent();
      easy_print(B);
      CallSite CS(I);
      Value *arg = CS.getArgument(0);
      // At each call site, check for peripheral typestates
      report_problem_typestates(states, B);
    }
  }
}


