#ifndef __PATH_CHECK__
#define __PATH_CHECK__
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>
#include "dataflow.h"
#include <iostream>
#include <iomanip>
#include <queue>
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Operator.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/GenericDomTree.h"
#include "llvm/Support/GenericDomTreeConstruction.h"

using namespace llvm;

typedef map<std::string,std::vector<Instruction*>>StateMapEntry;
typedef map<std::string,std::vector<int>>periphStateMap;

// Map of basicblocks to all peripheral states at that basicblock
typedef std::map<BasicBlock*,StateMapEntry> blockInstMap;

int multi_path_check(DominatorTree * DT, BasicBlock * UseBB, BasicBlock * TE);
void test_path_check(DominatorTree * DT, std::vector<BasicBlock*> UseBBs,
  std::vector<BasicBlock*> TestBBs);
void detailed_print(Function *F);
void easy_print(BasicBlock* BB);
void report_spec_match(Module& M, blockInstMap allowedStates);
int get_typestates(blockInstMap&,periphStateMap&,BasicBlock*);

#endif

