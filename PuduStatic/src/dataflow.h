// 15-745 S18 Assignment 2: dataflow.h
// Group: milijans, eruppel
////////////////////////////////////////////////////////////////////////////////

#ifndef __CLASSICAL_DATAFLOW_H__
#define __CLASSICAL_DATAFLOW_H__

#include <stdio.h>
#include <iostream>
#include <queue>
#include <vector>
#include <map>

#include "llvm/IR/Instructions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/CFG.h"

using namespace std;


namespace llvm {

typedef void (*mop_t)(BasicBlock*,
              std::map<std::string,std::vector<Instruction*>>&in,
              map<BasicBlock*,
              map<std::string,std::vector<Instruction*>>>&block);

typedef void (*tf_t)(Instruction* bb,
  std::map<std::string,std::vector<Instruction*>>&output,
  std::map<std::string,std::vector<Instruction*>>&input);

	// Restricting to only the forward dataflow, 0 initialization required for our
  // pass
void iterate( Function* F, tf_t t_func, mop_t meet_op, 
    std::map<BasicBlock*,map<std::string,std::vector<Instruction*>>>&block,
    std::map<std::string,std::vector<Instruction*>>&inMap,
    std::map<std::string,std::vector<Instruction*>>&outMap);
	
void add_isr(BasicBlock * B, Function * ISR, Function * current, Value *temp);
void add_breaks(Function *F);
DebugLoc findClosestDebugLoc( Instruction *I);

}

#endif
