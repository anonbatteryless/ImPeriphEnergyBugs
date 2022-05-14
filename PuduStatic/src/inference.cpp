#include "inference.h"
#include "path_check.h"
#include "tasks.h"
//#include "periph_schedule.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/LoopInfoImpl.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"


#include <map>
#include <string>
#include <assert.h>
#include <fstream>
#include <sstream>

void parse_break_even_data(std::string filename, periphVectType &periphs,
BreakEvenMapType &Cycles) {
  std::ifstream infile(filename);
  std::string line;
  std::vector<std::string> results;
  while(std::getline(infile,line)) {
    std::string field;
    std::stringstream ss(line);
    unsigned count = 0;
    GlobalVariable *cur = NULL;
    outs() << "line is: " << line << "\n";
    if(std::getline(ss,field,',')) {
      for (auto it : periphs) {
          outs() << "name in file is " << field << " but periph name is " <<
          it->getName() << "\n";
        if(it->getName().equals(field)){
          outs() << "match!\r\n";
          cur = it;
        }
      }
    }
    if (cur == NULL) {
      outs() << "Error parsing peripheral!\r\n";
    }
    // cycle through and grab state/cycle combo
    while(1) {
      unsigned state,cycles;
      if(std::getline(ss,field,',')) {
        state = stoi(field);
      }
      else {
        break;
      }
      if(std::getline(ss,field,',')) {
        cycles = stoi(field);
      }
      Cycles[cur][state] = cycles;
    }
    outs() << "\n";
  }
  return;
}


using namespace llvm;
std::map<string, unsigned> preloaded;

void process_preloaded_functions(std::string filename) {
  std::ifstream infile(filename);
  std::string line;
  std::vector<std::string> results;
  while(std::getline(infile,line)) {
    std::string name, count;
    std::stringstream ss(line);
    if(!std::getline(ss,name,',')) {
      outs() << "Error reading function name from file!\r\n";
    }
    if(!std::getline(ss,count,',')) {
      outs() << "Error reading  cycle count from file!\r\n";
    }
    preloaded[name] = stoi(count);
  }
  return;
}


//TODO a bunch of this is deprecated, figure out what
void report_low_usage(Module &M, periphVectType &periphs,blockInstMap &states,
EMapType &Emap, BreakEvenMapType &Cycles) {
  // For each peripheral
  for (auto periph: periphs) {
  // For each task
    for (auto &F : M) {
      if (!F.getName().startswith("task")) {
        continue;
      }
      // If periph status not written in task --> we'll start here and maybe
      // refine later on
      int flag = 0;
      for (auto *it : periph->users()) {
        Instruction *temp;
        if (!isa<Instruction>(it)) {
          continue;
        }
        temp = cast<Instruction>(it);
        if (temp->getFunction() == NULL) {
          continue;
        }
        if (temp->getFunction() == &F) {
          flag = 1;
          break;
        }
      }
      if (flag) {
        // Go on to the next peripheral in the chain
        continue;
      }
      // check state of the peripheral
      BasicBlock *temp = &(F.front());
      auto curStates = states[temp][periph->getName()];
      flag = 0;
      unsigned max_cycles = 0;
      for (auto it2 : curStates) {
        Value *test;
        test = it2->getOperand(0);
        int64_t reportedVal;
        reportedVal = check_if_constant(test);
        if ( reportedVal != 0 ) {
          if (Cycles.find(periph) != Cycles.end()) {
            if (Cycles.find(periph)->second.find(reportedVal) !=
              Cycles.find(periph)->second.end()) {
              if (Cycles[periph][reportedVal] > max_cycles) {
                max_cycles = Cycles[periph][reportedVal];
              }
            }
          }
          flag = 1;
        }
      }
      if (flag) {
        // Check if the number of instructions in the function is greater than the
        // break even point
        unsigned cycles = 0;
        if (max_cycles == 0) {
          outs() << "Function " << F.getName() << " requires " <<
            Emap[&F]*FUDGE_FACTOR <<
            "instructions, but break even cycle are not defined for periph "
            << periph->getName() << " and state.\n";
        }
        else {
          if ( Emap[&F] * FUDGE_FACTOR > max_cycles) {
            outs() << "Function " << F.getName() << 
            " requires more cycles than the break even point for the peripheral " <<
            periph->getName() << "\n";
          }
        }
      }
    }
  }
}

void topoSortCG( Function *F, CallGraph &CG,
      std::map< Function * , unsigned> &visits,
      std::vector< Function * >&list) {
  visits[F] = 1;
  for (auto itCh = CG[F]->begin(); itCh != CG[F]->end(); ++itCh) {
    if (visits.find(itCh->second->getFunction()) == visits.end()) {
      continue;
    }
    if (!visits[itCh->second->getFunction()]) {
      topoSortCG(itCh->second->getFunction(), CG, visits, list);
    }
  }
  list.push_back(F);
  return;
}



// Returns 1 if *not* all blocks have been visited
unsigned checkVisits(std::map<BasicBlock *, unsigned>visits) {
  for (auto it : visits) {
    if (it.second == 0) {
      return 1;
    }
  }
  return 0;
}

BasicBlock * getMinUnvisited( std::map<BasicBlock *, unsigned> dists,
                            std::map<BasicBlock *, unsigned> visited) {
  unsigned minVal = 0xFFFFFFFF;
  BasicBlock *minBlock = NULL;
  int flag = 0;
  for (auto it: visited) {
    if (!it.second) {
      if (dists[it.first] <= minVal) {
        minVal = dists[it.first];
        minBlock = it.first;
        flag = 1;
      }
    }
  }
  if (minBlock == NULL) {
    outs() << "ERROR: MinBlock should not be NULL\n";
  }
  assert(minBlock!=NULL);
  return minBlock;
}

 BranchInst *getLoopGuardBranch(Loop * loop) {
   if (!loop->isLoopSimplifyForm())
     return nullptr;
  outs() << "Simplified..\r\n";
  BasicBlock *Preheader = loop->getLoopPreheader();
   BasicBlock *Latch = loop->getLoopLatch();
   assert(Preheader && Latch &&
          "Expecting a loop with valid preheader and latch");
  outs() << "Preheader acquired\r\n";
   // Loop should be in rotate form.
   if (!loop->isLoopExiting(Latch))
     return nullptr;
  outs() << "Loop is exiting\r\n";

   // Disallow loops with more than one unique exit block, as we do not verify
   // that GuardOtherSucc post dominates all exit blocks.
   BasicBlock *ExitFromLatch = loop->getUniqueExitBlock();
   if (!ExitFromLatch)
     return nullptr;
  outs() << "exit from latch\r\n";

   BasicBlock *ExitFromLatchSucc = ExitFromLatch->getUniqueSuccessor();
   if (!ExitFromLatchSucc)
     return nullptr;
  outs() << "exit from latch unique\r\n";

   BasicBlock *GuardBB = Preheader->getUniquePredecessor();
   if (!GuardBB)
     return nullptr;
  outs() << "guardbb pred is unique\r\n";

   assert(GuardBB->getTerminator() && "Expecting valid guard terminator");

   BranchInst *GuardBI = dyn_cast<BranchInst>(GuardBB->getTerminator());
   if (!GuardBI || GuardBI->isUnconditional())
     return nullptr;

  outs() << "got terminator\r\n";
   BasicBlock *GuardOtherSucc = (GuardBI->getSuccessor(0) == Preheader)
                                    ? GuardBI->getSuccessor(1)
                                    : GuardBI->getSuccessor(0);
   return (GuardOtherSucc == ExitFromLatchSucc) ? GuardBI : nullptr;
 }


// TODO add instruction counter that sorts by type and handles call insts
// correctly for the different types... e.g. llvm.debug really doesn't need to
// get factored in.
unsigned calc_insts(Function *F, LoopInfo &LI,
                iterCountMap &loopCounts, EMapType &Emap) {
  std::map<BasicBlock *, unsigned> dists;
  std::map<BasicBlock *, unsigned> visited;
  for (auto &B : *F) {
    BasicBlock *bb = &B;
    if((LI.getLoopFor(bb)  && !LI.isLoopHeader(bb)) ||
    (LI.getLoopFor(bb) && LI.getLoopFor(bb)->getLoopDepth() > 1)) {
      continue;
    }
    dists[bb] = 0xFFFFFFFF;
    visited[bb] = 0;
  }
  // Set front distance to 0
  BasicBlock *Front = &(F->front());
  dists[Front] = 0;
  // loop until we've seen everything
  while(checkVisits(visited)) {
    // Get untouched node with min cost
    BasicBlock *bb = getMinUnvisited(dists,visited);
    visited[bb] = 1;
    // If bb contains a ret(or task transition), we're done
    // Calc cost of B if it's a loop header
    if (LI.isLoopHeader(bb)) {
      Loop *L = LI.getLoopFor(bb);
      unsigned mult = 0;
      unsigned temp;
      if (getLoopGuardBranch(L) != nullptr) {
        outs() << "Got a guard branch!\r\n";
        auto *temp2 = getLoopGuardBranch(L);
        outs() << "Type "<< temp2->getCondition()->getType()->getTypeID() << "\n";
        CmpInst *cp = dyn_cast<CmpInst>(temp2->getCondition());
        if (cp) {
          outs() << "icmp: " << cp->getPredicate() << "\n";
          outs() << cp->getOperand(0) << " " << cp->getOperand(1) << "\n";
          if (isa<ConstantInt>(cp->getOperand(0))) {
            outs() << "constant part is " <<
            cast<ConstantInt>(cp->getOperand(0))->getSExtValue() << "\n";
          }
          else {
            outs() << "constant part is " <<
            cast<ConstantInt>(cp->getOperand(1))->getSExtValue()<< "\n";
          }
        }
      }
      temp = countInstsInLoop(L, LI, loopCounts, mult, Emap);
      // Add the cost of the block to the cost of getting to it
      if (dists.find(bb) == dists.end()) {
        outs() << "Skipping add!\n";
        continue;
      }
      dists[bb] +=  temp * mult;
      outs() << "Adding count by mult: " << temp << "*" << mult << "\n";
    }
    else {
      // Just count the instructions
      unsigned count = 0;
      if (bb == NULL) {
        outs() << "ERROR! Block should not be NULL\n";
        return 0;
      }
      for (auto &I : *bb) {
        Instruction *i = &I;
        count++;
        if (isa<CallInst>(i)) {
          Function *temp = cast<CallInst>(i)->getCalledFunction();
          if (Emap.find(temp) == Emap.end()) {
            if (temp != NULL && preloaded.find(temp->getName()) != preloaded.end()) {
              count += preloaded[temp->getName()];
            }
            else {
              outs() << "Empty lookup for inst " << I << "\n";
              count += GENERIC_EVAL;
            }
          }
          else {
            count += Emap[temp];
          }
        }
      }
      dists[bb] += count;
    }
    // For each successor, see if this is a faster way
    if (LI.isLoopHeader(bb)) {
      if (LI.getLoopFor(bb)->getExitBlock()) {
        BasicBlock *le = LI.getLoopFor(bb)->getExitBlock();
        if (dists[bb] < dists[le]) {
          dists[le] = dists[bb];
        }
      }
    }
    else {
      for (BasicBlock *b_succ : successors(bb)) {
        if (dists.find(b_succ) == dists.end()) {
          continue;
        }
        if (dists[bb] < dists[b_succ]) {
          dists[b_succ] = dists[bb];
        }
      }
    }
  }
  // Find all exits
  vector<BasicBlock *>exits;
  for (auto &B: *F) {
    for (auto &I : B) {
      BasicBlock *bb = &B;
      Instruction *i = &I;
      if (isa<CallInst>(i) &&
        cast<CallInst>(i)->getCalledFunction() != NULL &&
        cast<CallInst>(i)->getCalledFunction()->getName().startswith("transition_to")){
        exits.push_back(bb);
        break;
      }
      else if(isa<ReturnInst>(i)) {
        exits.push_back(bb);
        break;
      }
    }
  }
  unsigned minVal = 0xFFFFFFFF;
  // Find min instruction traversal for exits
  for (BasicBlock* exit : exits) {
    BasicBlock *bl = exit;
    if (dists.find(exit) == dists.end()) {
    // This only happens if the exit is in a loop
      bl = LI.getLoopFor(exit)->getHeader();
    }
    if (dists[bl] < minVal) {
      minVal = dists[bl];
    }
  }
  return minVal;
}

unsigned countInstsInLoop(Loop *L, LoopInfo &LI, iterCountMap &loopCounts,
                        unsigned &multiplier,EMapType &Emap) {
  unsigned count = 0;
  // We assume iterations is always > 1 for a loop... otherwise why is it a
  // loop?
  multiplier = 0;
  for (BasicBlock *BB : L->getBlocks()) {
    if (LI.getLoopFor(BB) == L && loopCounts.find(BB) != loopCounts.end()) {
      multiplier = loopCounts[BB];
    }
    for (auto &I : *BB) {
      Instruction *i = &I;
      count++;
      // Add func cost if calling a function
      if (isa<CallInst>(i)) {
        Function *temp = cast<CallInst>(i)->getCalledFunction();
        if (Emap.find(temp) == Emap.end()) {
          if (temp != NULL && preloaded.find(temp->getName()) != preloaded.end()) {
            count += preloaded[temp->getName()];
          }
          else {
            outs() << "Empty lookup for inst " << I << "\n";
            count += GENERIC_EVAL;
          }
        }
        else {
          count += Emap[temp];
        }
      }
    }
  }
  if (multiplier == 0) {
    outs() << "undefined loop bound\n";
    multiplier = 1;
  }
  for (Loop * subL : L->getSubLoops()) {
    unsigned subCount = 0;
    unsigned subMult = 1;
    subCount = countInstsInLoop(subL, LI, loopCounts, subMult, Emap);
    // Subtract out the original subLoop instructions from the full loop count
    count -= subCount;
    // Now add in the multiplied version
    count += subCount*subMult;
  }
  outs() << "Returning count, multiplier " << count << ", " << multiplier <<
  "\n";
  return count;
}

void assembleLoopIters(Module &M, iterCountMap &iters) {
  for (Module::global_iterator gi = M.global_begin();
        gi != M.global_end(); ++gi) {
    if (gi->getName().startswith("_loop_count_paca")) {
      for (auto *U : gi->users()) {
        if (isa<Instruction>(*U)) {
          if (isa<StoreInst>(*U)) {
            Value *test;
            test = cast<StoreInst>(*U).getOperand(0);
            int64_t val = check_if_constant(test);
            BasicBlock * BB = cast<StoreInst>(*U).getParent();
            iters[BB] = val;
            outs() << "Got block with " << val << " loops on line ";
            easy_print(BB);
            outs() << " ";
            detailed_print(BB->getParent());
          }
        }
      }
      break;
    }
  }
  return;
}
