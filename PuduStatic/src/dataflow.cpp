// Based on 15-745 S18 Assignment 2: dataflow.cpp
// By (mostly) Milijana Surbatovich and Emily Ruppel
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "dataflow.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <map>
#include <queue>
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/MemorySSAUpdater.h"

using namespace std;

namespace llvm {

  DebugLoc findClosestDebugLoc( Instruction *I) {
    DIScope *scope = I->getFunction()->getSubprogram();
    Instruction *IwDebug = I;
    while (!IwDebug->getDebugLoc() && IwDebug->getPrevNode() != NULL ) {
      IwDebug = IwDebug->getPrevNode();
    }
    if (IwDebug->getDebugLoc()) {
      return DebugLoc(IwDebug->getDebugLoc());
    }
    else {
      return DebugLoc::get(I->getFunction()->getSubprogram()->getLine(),0,scope);
    }
  }
	/* parameters:  Function* F - the LLVM function we're examining,
					void* t_func - the transfer function
					void* meet_op - the meet operator
					bool dir - direction of the iteration, true is forwards, false is back
					int len - length of bit vector
					int boundary - how entry/exit should be initialized
					int interior - how the interior should be initialized
	returns map of bb -> bitvector
	*/

 void iterate(Function* F, tf_t t_func, mop_t meet_op,
    std::map<BasicBlock*,map<std::string,std::vector<Instruction*>>>&blockMap,
    std::map<std::string,std::vector<Instruction*>>&inMap,
    std::map<std::string,std::vector<Instruction*>>&outMap) {
    #ifdef TEST_WL
    outs() << "Analyzing func: " << F->getName() << "\n";
    #endif
    queue<BasicBlock*> to_visit;
		//make sure all the bit vectors are initialized
		for (BasicBlock& bb : *F) {
		  // Add initializer for all BB's
      to_visit.push(&bb);
		}
		//make sure entry/exit are set correctly
		//Now we start the working set algorithm
		while(!to_visit.empty()) {
      BasicBlock* bb = to_visit.front();
			to_visit.pop();
      // Run meet operator
      meet_op(bb, inMap, blockMap);
      int changed;
      changed = 0;
      for( Instruction& inst : *bb) {
        // Check if we've got a call inst, then check if it's a recursive call,
        // then check if it's going to modify peripheral state
        if (isa<CallInst>(inst) &&
          cast<CallInst>(inst).getCalledFunction() != NULL &&
          cast<CallInst>(inst).getCalledFunction() != F &&
          cast<CallInst>(inst).getCalledFunction()->hasFnAttribute("periph")) {
          #ifdef TEST_DRVR
          if (DILocation *Loc = inst.getDebugLoc()) {
            unsigned Line = Loc->getLine();
            outs () << "Looking at" << " " << Line << "\n";
          }
          for (auto it : inMap) {
            outs() << it.first << " " << it.second.size() << "\n";
          }
          outs() << "Done\n";
          #endif
          // call iterate with a clean set of basic block if there's a nested
          // function
          std::map<BasicBlock*,map<std::string,
            std::vector<Instruction*>>>localBlockMap;
          // Clear local block
          localBlockMap.clear();
          localBlockMap[bb] = inMap;
          // We'll rely on carefully placed clears in meet_op to correctly pass
          // the values for the first block in the function
          Function *nestedF = cast<CallInst>(inst).getCalledFunction();
          #ifdef TEST_DRVR
          outs() << "Func In: " << nestedF->getName() << "\n";
          for (auto it2 : inMap) {
            outs() << it2.first << " " << it2.second.size() << "\n";
          }
          #endif
          iterate(nestedF, t_func, meet_op, localBlockMap, inMap, outMap);
          #ifdef TEST_DRVR
          outs() << "\n-----Func: " << nestedF->getName() << "----\n";
          for (auto it2 : outMap) {
            outs() << it2.first << " " << it2.second.size() << "\n";
            for (auto it3: it2.second) {
              if (DILocation *Loc = it3->getDebugLoc()) {
                unsigned Line = Loc->getLine();
                outs () << Line << " ";
              }
            }
          }
          outs() << "\n----------------\n";
          #endif
        }
        else {
          // Report error if we found a recursive call, but keep going anyway
          if( isa<CallInst>(inst) &&
          cast<CallInst>(inst).getCalledFunction() != NULL &&
          cast<CallInst>(inst).getCalledFunction() == F) {
            outs() << "ERROR! Skipping recursive function call to peripheral\n";
            outs() << F->getName() << " is a recursive function\n";
          }
          t_func(&inst, outMap, inMap);
          // copy effect of each instruction
          // We need this because our merge is at the BB level but our tf is at
          // the instruction level
          #ifdef TEST_DRVR
          if(!F->getName().startswith("task")) {
            outs() << "\n----block output------\n";
            for (auto it2 : outMap) {
              outs() << it2.first << " " << it2.second.size() << "\n";
              for (auto it3: it2.second) {
                if (DILocation *Loc = it3->getDebugLoc()) {
                  unsigned Line = Loc->getLine();
                  outs () << Line << " ";
                }
              }
            }
          }
          #endif
          inMap = outMap;
        }
      }
      // This comparison works for instances where we dug into a nested function
      // too since outMap will be the output of the last basic block in the
      // function
      if ( blockMap[bb] != outMap) {
        changed = 1;
        blockMap[bb] = outMap;
      }
      if ( changed) {
        #ifdef TEST_WL
        outs() << "Pushing successor blocks at lines ";
        #endif
        for ( auto curr = succ_begin(bb), end = succ_end(bb); curr != end; ++curr) {
          BasicBlock* succ = *curr;
          to_visit.push(succ);
          #ifdef TEST_WL
          if (DILocation *Loc = succ->front().getDebugLoc()) {
            unsigned Line = Loc->getLine();
            outs () << Line << " ";
          }
          #endif
        }
        #ifdef TEST_WL
        outs() << "\n";
        #endif
      }
		}
    #ifdef TEST_DRVR
    if(!F->getName().startswith("task")) {
      outs() << "Done recursion!\n";
    }
    #endif

		return;
	}

  void add_isr(BasicBlock * B, Function * ISR, Function * current, Value *dummy) {
    // Declare types n'at for setting up the function call
    LLVMContext &Ctx = current->getContext();
    std::vector<Type*> paramTypes = {};
    //std::vector<Type*> paramTypes = {Type::getVoidTy(Ctx)};
    Type *retType = Type::getVoidTy(Ctx);
    FunctionType *isrType = FunctionType::get(retType,paramTypes,false);
    //outs() << "Adding isr func: " << ISR->getName() << "\n";
    Constant *isrFunc  = current->getParent()->getOrInsertFunction(ISR->getName(),isrType);
    std::vector<Instruction *> insts;
    insts.clear();
    for (Instruction &I : *B) {
      Instruction *i = &I;
      insts.push_back(i);
    }
    for (int i = 0; i < insts.size(); i++) {
      if (insts[i]->isTerminator()) {
        continue;
      }
      IRBuilder<>builder(insts[i]);
      builder.SetInsertPoint(B, ++builder.GetInsertPoint());
      builder.SetCurrentDebugLocation(findClosestDebugLoc(insts[i]));
      auto *newInst = builder.CreateCall(isrFunc);
    }
  }

  bool isPeriphModifier(Instruction *I) {
    // Check if instruction is a periph modifier
    if (isa<StoreInst>(*I)) {
      // Get op, check if it's a periph
      Value *test;
      test = I->getOperand(1);
      if (isa<GlobalValue>(*test) &&
          cast<GlobalVariable>(*test).hasAttribute("periph_var")) {
        return 1;
      }
    }
    return 0;
  }

  void add_breaks(Function *F) {
    // Declare types n'at for setting up the function call
    LLVMContext &Ctx = F->getContext();
    outs() << "Modifiying function " << F->getName() << "\n";
    std::vector<BasicBlock *> blocks;
    blocks.clear();
    for (BasicBlock &b : *F) {
      BasicBlock *B = &b;
      blocks.push_back(B); 
    }
    // For all blocks in a function
    for (int j = 0; j < blocks.size(); j++) {
      BasicBlock *B = blocks[j];
      // For all intstructions in a block
      std::vector<Instruction *> insts;
      insts.clear();
      for (Instruction &I : *B) {
        Instruction *i = &I;
        insts.push_back(i);
      }
      for (int i = 0; i < insts.size(); i++) {
        if (!isPeriphModifier(insts[i])) {
          continue;
        }
        // If there's a periph state change
        // Split at state change
        BasicBlock *newBB = insts[i]->getParent()->splitBasicBlock(insts[i]);
        // Split new block after state change
        BasicBlock *anothernewBB =
        insts[i]->getParent()->splitBasicBlock(insts[i+1]);
        //TODO confirm that we never get to the end of the insts list with this
        //i+1 nonsense. The terminatorInst should fail that first check.
      }
    }
  }

}

