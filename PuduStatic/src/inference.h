#ifndef _INFERENCE_H_
#define _INFERENCE_H_

#include "llvm/IR/Module.h"
#include <vector>
#include <map>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "path_check.h"

using namespace llvm;

typedef std::vector<GlobalVariable*> periphVectType;
typedef std::map<BasicBlock*, unsigned> iterCountMap;
typedef std::map<Function *, unsigned> EMapType;
// This maps variables to a map of states and break even cycles
typedef std::map<GlobalVariable*,std::map<unsigned,unsigned>> BreakEvenMapType;

void report_low_usage(Module &M, periphVectType &periphs,blockInstMap &states,
EMapType &Emap, BreakEvenMapType &Cycles);
unsigned calc_insts(Function *F, LoopInfo &L, iterCountMap &counts, EMapType &emap);
unsigned countInstsInLoop(Loop *, LoopInfo &LI, iterCountMap &loopCounts, 
                          unsigned &mult, EMapType &emap);
void assembleLoopIters(Module &M,iterCountMap &iters);
void calcAllFunctions(CallGraph & CG);
void topoSortCG( Function *F, CallGraph &CG,
    std::map<  Function * , unsigned> &visits,
    std::vector< Function * >&list);
void parse_break_even_data(std::string filename, periphVectType &periphs,
  BreakEvenMapType &Cycles);
void process_preloaded_functions(std::string);
extern std::map<string, unsigned> preloaded;

extern periphVectType peripherals;


#define FUDGE_FACTOR 2
#define GENERIC_EVAL 0

#endif
