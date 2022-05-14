#include "path_check.h"
#include "tasks.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/GenericDomTreeConstruction.h"
using namespace llvm;

/*
add the following to the module pass:
DT = &getAnalysis<DominatorTree>();
*/

void easy_print(BasicBlock * BB) {
  int flag = 0;
  for (auto &I : *BB) {
    if (DILocation *Loc = I.getDebugLoc()) {
      unsigned Line = Loc->getLine();
      outs () << Line << "\n";
      flag = 1;
      break;
    }
  }
  if (!flag) {
    outs() << "No line found\n";
  }
}

void detailed_print(Function *F) {
  BasicBlock &B = F->front();
  BasicBlock *BB = &B;
  for (auto &I : *BB) {
    if (DILocation *Loc = I.getDebugLoc()) {
      unsigned Line = Loc->getLine();
      auto *Scope = cast<DIScope>(Loc->getScope());
      std::string fileName = Scope->getFilename();
      outs() << fileName << " : " << Line << "\n";
      break;
    }
  }
  return;
}


int64_t get_constant(Instruction *ItI) {
  Value * test = ItI->getOperand(0);
  int64_t val = -1;
  if (isa<ConstantInt>(test)) {
    val = cast<ConstantInt>(test)->getSExtValue();
  }
  else {
    outs() << "[WARNING] non-constant assignment\n";
  }
  return val; 
}

int multi_path_check(DominatorTree * DT, BasicBlock * UseBB, BasicBlock * TestBB) {
  if (!DT->dominates(UseBB, TestBB)) {
    //auto cur = getNodeForBlock(UseBB, DT);
    auto cur = DT->getNode(UseBB);
    while(!DT->dominates(cur->getBlock(), TestBB)) {
      //outs() << "Cur block starts at :";
      //easy_print(cur->getBlock());
      cur = cur->getIDom();
    }
    // Dump dominator node
    outs() << "Last shared block starts at line: ";
    easy_print(cur->getBlock());
    return 1;
  }
  else {
    //outs() << "Exit dominated: ";
    //easy_print(TestBB);
    return 0;
  }
}

void test_path_check(DominatorTree * DT, std::vector<BasicBlock*> UseBBs,
  std::vector<BasicBlock*> TestBBs) {
  // Walk through UseBBs and figure out last shared node with TestBBs
  int flag = 0;
  for (auto UB: UseBBs) {
    //outs() << "Testing use block starting at: ";
    //easy_print(UB);
    flag = 0;
    for (auto TB: TestBBs) {
      //outs() << "Testing exit block starting at: ";
      //easy_print(TB);
      flag = 0;
      flag = multi_path_check(DT, UB, TB);
      if(flag) {
        outs() << "Found non-dominating assignment in block at ";
        easy_print(UB);
        outs() << "Block does not dominate exit at ";
        easy_print(TB);
        break;
      }
    }
  }
}


void report_spec_match(Module& M, blockInstMap allowedStates) {
  // Find possible_states_inner function
  Function *func = NULL;
  for ( auto &F : M ) {
    if ( F.getName().startswith("possible_states")) {
      func = &F;
      break;
    }
  }
  if ( func == NULL ) {
    outs() << "No state checks\n";
    return;
  }
  // Walk through all uses
  for (auto *it : func->users()) {
    if (!isa<CallInst>(it)) {
      continue;
    }
    outs() << "Looking at state check on line: ";
    easy_print(cast<CallInst>(it)->getParent());
    StringRef periph;
    std::string periphName;
    Use *opIt;
    // Dig out peripheral in first arg
    opIt = it->op_begin();
    if ( isa<ConstantExpr>(opIt) ) {
      if (GlobalVariable *itGV =
        dyn_cast<GlobalVariable>(cast<ConstantExpr>(opIt)->getOperand(0))) {
        if ( ConstantDataArray * itCA =
          dyn_cast<ConstantDataArray>(itGV->getOperand(0))) {
          periph = itCA->getAsString();
          periph = periph.drop_back(1);;
          Twine fullName = periph + "_status";
          periphName = fullName.str();
          outs() << "\t\tperiphName is: " << periphName << "\n";
        }
      }
    }
    ++opIt;
    if (opIt == it->op_end()) {
      outs() << "[ERROR] no possible states provided\n";
    }
    // Walk through arguments and accumulate arr
    std::vector<int64_t> expectedVals;
    expectedVals.clear();
    for (; opIt != it->op_end(); ++opIt) {
      if ( ConstantInt *temp = dyn_cast<ConstantInt>(opIt) ) {
        expectedVals.push_back(temp->getSExtValue());
      }
    }
    // Grab basic block's assignments for that periph
    std::vector<Instruction*> tVect; 
    std::vector<int64_t> observableVals;
    observableVals.clear();
    tVect = allowedStates[cast<CallInst>(it)->getParent()][periphName];
    outs() << "\t\t" << tVect.size() << " possible states at block\n";
    for (Instruction *itI : tVect) {
        outs() << "\t\tState is: " << get_constant(itI) << "\n";
        observableVals.push_back(get_constant(itI));
    }
    int flag;
    for ( int64_t e : expectedVals) {
      flag = 0;
      for ( int64_t o : observableVals) {
        if ( e == o) {
          flag = 1;
          break;
        } 
      }
      if ( !flag ) {
        // Throw error if state isn't reachable
        outs() << "[WARNING] " << e << " is not observable at line ";
        easy_print(cast<CallInst>(it)->getParent());
        outs() << " for periph " << periphName << "\n";
      }
    }
    // TODO: remove repeats in these vectors
    // Throw error if warning if extra states are reachable
    if (expectedVals.size() < observableVals.size()) {
      outs() << "[WARNING] more observable states than expected states for periph " 
      <<  periphName << " at line ";
      easy_print(cast<CallInst>(it)->getParent());
      outs() << "\n";
    }
  }
}


int get_typestates(blockInstMap &blockMap,periphStateMap &periphStates, BasicBlock *BB) {
	// Walk through peripherals
	for (auto ItP: blockMap[BB]) {
		for (auto ItI: ItP.second) {
			// Walk through all assignments and drop into vector
			Value *test;
			test = ItI->getOperand(0);
			int reportedVal = check_if_constant(test);
			// Scrub duplicates
			if (count(periphStates[ItP.first].begin(),
				periphStates[ItP.first].end(),reportedVal) == 0) {
				outs() << "Adding active! " << ItP.first << "\n";
				periphStates[ItP.first].push_back(reportedVal);
			}
		}
	}
	return 0;
}
