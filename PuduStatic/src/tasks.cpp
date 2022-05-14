// Effectively our "main" file here
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>
#include "dataflow.h"
#include "path_check.h"
#include "tasks.h"
#include "int_concurrency.h"
#include "extra_function_check.h"
#include "inference.h"
#include "auto_activation.h"
#include "atomic_report.h"

#include <iostream>
#include <iomanip>
#include <queue>
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Operator.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/DominanceFrontierImpl.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/InitializePasses.h"
#include "llvm-c/Initialization.h"
#include "llvm/PassRegistry.h"
#define EMPTY 0
#define BACKWARDS 0
using namespace llvm;

// Map of tasks to predecessor tasks
std::map<std::string,std::vector<Function*>>predTasks;
// Map of tasks to successor tasks
std::map<Function*,std::vector<Function*>> succTasks;
// Map of tasks to the basicblocks that are their exit points
std::map<Function*,std::vector<BasicBlock*>>exitPoints;
// Map of exit points to the names of the tasks they are transitioning to
std::map<BasicBlock*,std::string>exitMatch;

// Instructions where we change the state of a peripheral
std::map<std::string,std::vector<Instruction*>> periphStates;
// Map of functions to the instructions and the periph typestates they modify
std::map<Function*, map<Instruction*, std::string>>FuncPeriphMap;
// Map of basicblocks to all peripheral states at that basicblock
blockInstMap blockMap;
// Vector of active peripherals
periphVectType peripherals;

#ifdef TEST_ISR
// Make vector of interrupts
std::vector<Function*> interrupts;
// Map of blocks in ISR
blockInstMap blockMap_ISR;
// Vector of exit blocks from interrupts
std::map<Function*, std::vector<BasicBlock*>>exits_ISR;
#endif

// Make vector of disable functions
disEnMapType disablers;
disEnMapType enablers;

// Make map of basicblocks with loop iteration counts
iterCountMap loopIters;
// Make a map of Functions and their "energy" cost
EMapType Emap;

int64_t check_if_constant(Value *test) {
  int64_t valOut = -1;
  if ( isa<ConstantInt>(test)) {
    valOut = cast<ConstantInt>(test)->getSExtValue();
  }
#ifdef TEST_VERBOSE
  else {
    outs() << "[WARNING!] Assigned a non-constant value to periph\n";
  }
#endif
  return valOut;
}

namespace {
  Value *interrupt_dummy;
  // transfer function _per_instruction to define how typestate assignments are
  // propagated/killed
  void tf(Instruction *I,
    std::map<std::string,std::vector<Instruction*>>&output,
    std::map<std::string,std::vector<Instruction*>>&input) {
    std::map<std::string,std::vector<Instruction*>>temp;
    // TODO double check that this is copying and try to use output so we don't
    // have this extra copy
    temp = input;
    if (isa<CallInst>(*I) &&
          cast<CallInst>(*I).getCalledFunction() != NULL &&
          cast<CallInst>(*I).getCalledFunction()->hasFnAttribute("direct")) {
      Function *calledF = cast<CallInst>(*I).getCalledFunction();
      outs() << "Func name: " << calledF->getName() << "\n";
      for (const AttributeSet itA  : calledF->getAttributes()) {
        for (auto itA2 : itA) {
          StringRef itAS = itA2.getAsString();
          std::string temp1 = itAS.str();
          std::string  temp2 = "direct:";
          outs() << temp1 << " ";
          if (temp1.find(temp2) != std::string::npos) {
            outs() << "Digging deeper on: " << itAS << "\n";
            StringRef itNew = itAS.ltrim("direct:");
            unsigned count = 0;
            for (char  itCh : itNew) {
              count ++;
              if (itCh == ':') break;
              else outs() << itCh << " ";
            }
            StringRef otherSplit = itNew.drop_front(count);
            outs() << "Other approach: " << otherSplit << "\n";
            auto itPair = otherSplit.split(":");
            outs() << "var: " << itPair.first << "\n";
            APInt itNum;
            itPair.second.getAsInteger(10, itNum);
            outs() << "num: " << itNum << "\n";
            if ( temp.count( itPair.first )) {
              outs() << "Found assignment!\n";
            }
            else {
              outs() << itPair.first << " is not registered!\n";
            }
            break;
          }
        }
        outs() << " \n";
      } 
    }
    // Check if instruction is a periph modifier
    if (isa<StoreInst>(*I)) {
      // Get op, check if it's a periph
      Value *test;
      test = I->getOperand(1);
      if (isa<GlobalValue>(*test) &&
          cast<GlobalVariable>(*test).hasAttribute("periph_var")) {
        // Kill all prior assignments to the periph
        temp[test->getName().str()].clear();
        // Add the new assignment instruction
        temp[test->getName().str()].push_back(I);
      }
    }
    output = temp;
    return;
  }

  // Function to merge the insane map structure from a predecessor basicBlock
  void mergeMaps(BasicBlock* Pred,
    std::map<std::string,std::vector<Instruction*>>&inMap,
    map<BasicBlock*, map<std::string,std::vector<Instruction*>>> &blockMap) {
    // Iterate through all of the string names of assigned typestates that
    // reach the predecessor block
    #ifdef TEST_MEET
    outs() << "Inmap in:";
    for (auto it: inMap) {
      outs() <<  "\t" << it.first << " " << it.second.size() << "-->";
      for (auto it2: it.second) {
        if (DILocation *Loc = it2->getDebugLoc()) {
          unsigned Line = Loc->getLine();
          outs () << Line << " ";
        }
      }
    }
    #endif
    for (auto it : blockMap[Pred]) {
      // Check if the typestate has already been noted
      if ( inMap.find(it.first) != inMap.end()) {
        // Walk through and insert instructions that are not duplicates
        for ( auto it2 : it.second) {
          if ( find( inMap[it.first].begin(), inMap[it.first].end(),
            it2) == inMap[it.first].end()) {
            inMap[it.first].push_back(it2);
          }
        }
      }
      else {
        // insert the entire thing if we don't have it yet
        inMap.insert ( std::pair<std::string,std::vector<Instruction*>>
          (it.first, it.second));
      }
    }
    #ifdef TEST_MEET
    outs() << "\nInmap out:";
    for (auto it: inMap) {
      outs() <<  "\t" << it.first << " " << it.second.size() << "-->";
      for (auto it2: it.second) {
        if (DILocation *Loc = it2->getDebugLoc()) {
          unsigned Line = Loc->getLine();
          outs () << Line << " ";
        }
      }
    }
    outs() << "\n";
    #endif
    return;
  }
  
  // Meet operator specific to ISRs... it's cleaned up compared to the task
  // version, it also has fewer ifdefs/print statements because I can't stand
  // them anymore
  void meet_op_ISR(BasicBlock* BB,
    std::map<std::string,std::vector<Instruction*>>&inMap,
    map<BasicBlock*, map<std::string,std::vector<Instruction*>>> &blockMap) {
    // Handle first block in task here, we need to do merge from other tasks
    if ( BB == &(BB->getParent()->front())) {
      mergeMaps(BB, inMap, blockMap);
    }
    else {
      inMap.clear();
      // Iterate through the block's predecessors
      for ( BasicBlock *Pred : predecessors(BB)) {
        mergeMaps(Pred, inMap, blockMap);
      }
    }
    return;
  }

  // Meet operator that does a union on the reaching typestates to produce the
  // input map for any given basic block
  void meet_op(BasicBlock* BB,
    std::map<std::string,std::vector<Instruction*>>&inMap,
    map<BasicBlock*, map<std::string,std::vector<Instruction*>>> &blockMap) {
    // Handle first block in task here, we need to do merge from other tasks
    if ( BB == &(BB->getParent()->front())) {
      // Don't clear inMap if this is the front of a function that isn't a task
      #ifdef TEST_MEET
      outs() << "Front of " << BB->getParent()->getName() << "\n";
      #endif
      if ( BB->getParent()->getName().startswith("task")) {
        inMap.clear();
        std::string str = "_task_";
        str.append(BB->getParent()->getName());
        std::string str_short = BB->getParent()->getName();
        for ( auto it : predTasks[str]) {
          #ifdef TEST_MEET
          outs() << "Merging from pred task long str " << str << " : " <<
            it->getName() << "\n";
          #endif
          // Need to grab the output from the exit points
          int flag;
          flag = 0;
          for ( auto it2 : exitPoints[it]) {
            // Now we run through the search func for the exit Block
#ifdef TEST_TG
            outs() << it2 << " " << exitMatch[it2] << " " << str << "done\n";
#endif
            if ( exitMatch[it2].compare(str) == 0) {
              flag = 1;
              mergeMaps(it2, inMap, blockMap);
            }
          }
          #ifdef TEST_TG
          if (!flag) {
            outs() << "Error matching predecessor and exits! " << str << "\n";
          }
          #endif
        }
        #ifdef TEST_TG
        outs() << "Testing str_short: " << str_short <<  " " <<
        predTasks[str_short].size()<<  ".\n";
        #endif
        for ( auto it : predTasks[str_short]) {
          #ifdef TEST_MEET
          outs() << "Merging from pred task short str " << str_short << " : " <<
            it << "\n";
          #endif
          // Need to grab the output from the exit points
          for ( auto it2 : exitPoints[it]) {
            #ifdef TEST_MEET
            outs() << str_short << "vs " << exitMatch[it2] << " same?\n";
            #endif
            if ( exitMatch[it2].compare(str_short) == 0) {
              #ifdef TEST_TG
              outs() << "Found string match " << exitMatch[it2] << "\n";
              #endif
              // Now we run through the search func for the exit Block
              mergeMaps(it2, inMap, blockMap);
            }
          }
        }
      }
      // If we're at the front of a function that isn't a task
      else {
        #ifdef TEST_MEET
        outs() << "\n--------Checking inMap----------\n";
        for (auto it: inMap) {
          outs() << it.first << " " << it.second.size() << "-->";
          for (auto it2: it.second) {
            if (DILocation *Loc = it2->getDebugLoc()) {
              unsigned Line = Loc->getLine();
              outs () << Line << " ";
            }
          }
        }
        outs() << "\n---------------\n";
        #endif
        // I think we need a copy here to really represent what's happening
        mergeMaps(BB, inMap, blockMap);
      }
    }
    else {
      inMap.clear();
      #ifdef TEST_MEET
      outs() << "Merging to block at line ";
      if (DILocation *Loc = BB->front().getDebugLoc()) {
        unsigned Line = Loc->getLine();
        outs () << Line << "\n";
      }
      #endif
      // Iterate through the block's predecessors
      for ( BasicBlock *Pred : predecessors(BB)) {
        #ifdef TEST_MEET
        outs() << "Merging from pred block at line ";
        if (DILocation *Loc = Pred->front().getDebugLoc()) {
          unsigned Line = Loc->getLine();
          outs () << Line << "\n";
        }
        #endif
        mergeMaps(Pred, inMap, blockMap);
      }
    }
    #ifdef TEST_ISR
    visitBlockType visits; 
    for (auto it_isr : interrupts) {
      visits.clear();
      // Get matching enable/disable functions, and ONLY RUN ON TASKS
      StringRef isrName = it_isr->getName();
      // TODO this is a kludge for now! Double check this doesn't break
      // task-based programs
      outs() << "Looking for dis/enablers for interrupt: " << isrName
      << "|" << disablers[isrName] << "| ";
      easy_print(BB);
      outs() << "\n";
      if (disablers[isrName] &&
        (BB->getParent()->getName().startswith("task_") ||
        BB->getParent()->getName().startswith("mai"))) {
        // Check answer for this block
        int check = 0;
        check = searchEn(BB, disablers[isrName], enablers[isrName],visits);
        // If answer indicates we're disabled, do more checking
        if (check == 1) {
          outs() << "disabled!\n";
          int check2;
          check2 = 3;
          // Check if we disable anywhere in this block
          if (containsFunc(disablers[isrName],BB)) {
            outs() << "Running disable checker!\n";
            // If we do, run special disableBlockChecker
            check2 = checkDisable(BB, disablers[isrName],
                        enablers[isrName],visits);
          }
          if (check2) {
            outs() << "Skipping interrupt!\r\n";
            continue;
          }
        }
      }
      outs() << "Merging maps!\n";
      for (auto it2_isr : exits_ISR[it_isr]) {
        mergeMaps(it2_isr,inMap, blockMap_ISR);
      }
    }
    #endif
    return;
  }

  class TaskGraphs : public ModulePass {
    public:
    static char ID;
    TaskGraphs() : ModulePass(ID) { }
    virtual bool runOnModule(Module& M) {
      outs() << "Running!\n";
      // Just here for a test
			//thresholds[][]
      periphStates.clear();
      predTasks.clear();
      blockMap.clear();
      succTasks.clear();
      exitPoints.clear();
      disablers.clear();
      enablers.clear();
      find_isr_disEn(M,enablers,disablers);
      assembleLoopIters(M,loopIters);
      // Find the functions/variables that have a correctly formatted peripheral
      // attribute and add the attribute to the llvm object
      // Code based on post from Brandon Holt found here:
      // http://bholt.org/posts/llvm-quick-tricks.html
      // And another version found here:
      // https://gist.github.com/serge-sans-paille/c57d0791c7a9dbfc76e5c2f794e650b4
      //TODO this needs to be a function, it's a gross but self containted loop
      if(GlobalVariable* GA = M.getGlobalVariable("llvm.global.annotations")) {
        // the first operand holds the metadata
        for (Value *AOp : GA->operands()) {
          // all metadata are stored in an array of struct of metadata
          if (ConstantArray *CA = dyn_cast<ConstantArray>(AOp)) {
            // so iterate over the operands
            for (Value *CAOp : CA->operands()) {
              // get the struct, which holds a pointer to the annotated function
              // as first field, and the annotation as second field
              if (ConstantStruct *CS = dyn_cast<ConstantStruct>(CAOp)) {
                if (CS->getNumOperands() >= 2) {
                  Function* AF =
                    cast<Function>(CS->getOperand(0)->getOperand(0));
                  // the second field is a pointer to a global constant Array that
                  // holds the string
                  if (GlobalVariable *GAnn =
                      dyn_cast<GlobalVariable>(CS->getOperand(1)->getOperand(0))) {
                    if (ConstantDataArray *A =
                        dyn_cast<ConstantDataArray>(GAnn->getOperand(0))) {
                      // we have the annotation!
                      StringRef AS = A->getAsString();
                      auto str = A->getAsCString();
                      if (AS.find("periph")) {
                        outs() << "Found periph attr " << AS << "\n";
                      }
                      if (AS.startswith("periph") || AS.startswith("direct")) {
                        #ifdef TEST_ATTR
                        outs() << "attr name: " <<  AS << "\n";
                        #endif
                        if (isa<Function>(CS->getOperand(0)->getOperand(0))) {
                          if (AS.startswith("periph")) {
                            AF->addFnAttr("periph");
                          }
                          else {
                            // Need to add in both
                            #ifdef TEST_ATTR
                            outs() << "Added to both\n";
                            #endif
                            AF->addFnAttr("direct");
                            //AF->addFnAttr(AS);
                            AF->addFnAttr(str);
                          }
                          #ifdef TEST_ATTR
                          outs() << "Annotating function\n";
                          outs() << AF->getName() << " " << str << " ";
                          outs() << "Used  " << AF->getNumUses() << " times\n";
                          #endif
                          // Make all users of a driver function drivers as well
                          for (auto *it : AF->users()) {
                            if (isa<CallInst>(it)) {
                              Function *PF = cast<CallInst>(it)->getFunction();
                              PF->addFnAttr("periph");
                              #ifdef TEST_ATTR
                              outs() << "\t" << PF->getName() << " ";
                              #endif
                            }
                          }
                          #ifdef TEST_ATTR
                          outs() << "\n";
                          #endif
                        }
                        else {
                          if (isa<GlobalValue>(CS->getOperand(0)->getOperand(0))) {
                            GlobalVariable * GV =
                            cast<GlobalVariable>((CS->getOperand(0)->getOperand(0)));
                            GV->addAttribute(str);
                            peripherals.push_back(GV);
                            #ifdef TEST_ATTR
                            outs() << "Found var!\n";
                            outs() << "name: " << GV->getName() << " " << str << "\n";
                            #endif
                          }
                          else {
                            if (isa<Argument>(CS->getOperand(0)->getOperand(0)))
                              { outs() << "Found an argument!";
                              }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
#ifdef TEST_MOD
      for (Function &F : M) {
        Function *f = &F;
        add_breaks(f);
      }
#endif
      outs() << "Acquired attributes\n";
      // Iterate through the global variables
      for(Module::global_iterator gi = M.global_begin();
                            gi != M.global_end(); ++gi) {
      /*---------------------------------------------------------------*/
        // Find the tasks and figure out the predecessors
        // Note, assumes that task names are only used for transitions
        if ( gi->getName().startswith("_task")) {
          std::string type_str;
          llvm::raw_string_ostream rso(type_str);
          std::vector<Function*> *myVect = new vector<Function*>();
          myVect->clear();
          #ifdef TEST_TG
          outs() << "Adding: " << gi->getName() << "\n";
          #endif
          predTasks.insert(
          std::pair<std::string,std::vector<Function*>>(gi->getName().str(),*myVect));
          for (auto U : gi->users()) {
            if(auto I = dyn_cast<Instruction>(U)) {
              predTasks[gi->getName().str()].push_back(I->getFunction());
            }
          }
        }
      /*---------------------------------------------------------------*/
      // Find the ops where we assign a constant value to periph
        if ( gi->hasAttribute("periph_var")) {
          #ifdef TEST_ATTR
          outs() << "" << gi->getName() << "\n";
          #endif
          std::vector<Instruction*> *testVect = new vector<Instruction*>();
          testVect->clear();
          periphStates.insert(std::pair<std::string,std::vector<Instruction*>>
            (gi->getName().str(),*testVect));  
          for (auto *U : gi->users()) {
            if (isa<Instruction>(*U)) {
              if (isa<StoreInst>(*U)) {
                #ifdef TEST_ATTR
                outs() << "Found store inst line: ";
                if (DILocation *Loc = cast<Instruction>(*U).getDebugLoc()) {
                  outs() << Loc->getLine() << "\n";
                }
                #endif
                Instruction& myInst = cast<Instruction&>(*U);
                periphStates[gi->getName().str()].push_back(&myInst);
                // We also want to build up a mapping of functions to the periph
                // instructions inside them, so we'll do that here too.
                std::map<Instruction *, std::string> idata;
                idata.clear();
                idata.emplace(&myInst, gi->getName().str());
                FuncPeriphMap[myInst.getParent()->getParent()][&myInst] =
                  gi->getName().str();
              }
            }
          }
          #ifdef TEST_ATTR
          outs() << "check: " << periphStates.at(gi->getName().str()).size();
          for (auto f : periphStates.at(gi->getName().str())) {
            outs() << f->getName() << "\n";
          }
          #endif
        }
      }
      #ifdef TEST_TG
      for (auto it : predTasks) {
        outs() << it.first << " predecessors: " << it.second.size() << "\n";
        for (auto it2 : it.second) {
        outs() << it2->getName() << "\n";
        }
      }
      outs() << "Done TG\n";
      #endif
      #ifdef TEST_ATTR
      outs() << "checking periph states " << periphStates.size() << "\n";
      for (auto it : periphStates) {
        outs() << it.first << " with " << it.second.size() << " mods\n";
      }
      #endif
      outs() << "Analyzed globals\n";
      /*------------------------------------------------------------------*/
      // Fallback plan if we haven't found any exits
      for (auto &F: M) {
        if (F.getName().startswith("task")) {
          #ifdef TEST_TG
          outs() << "Found " << F.getName() << "\r\n";
          #endif
          std::string type_str;
          llvm::raw_string_ostream rso(type_str);
          std::vector<Function*> *myVect = new vector<Function*>();
          myVect->clear();
          predTasks.insert(
          std::pair<std::string,std::vector<Function*>>(F.getName().str(),*myVect));
          for (auto U : F.users()) {
            #ifdef TEST_TG
            outs() << F.getName() << " has users\r\n";
            #endif
            if(auto I = dyn_cast<Instruction>(U)) {
              #ifdef TEST_TG
              outs() << "Got I that uses " << F.getName() << "\n";
              #endif
              predTasks[F.getName().str()].push_back(I->getFunction());
            }
          }
          // Add function that calls this task to pred list
        }
      }
      #ifdef TEST_TG
      outs() << "printing\n";
      for (auto it : predTasks) {
        outs() << it.first << " predecessors: " << it.second.size() << "\n";
        for (auto it2 : it.second) {
        outs() << it2->getName() << "\n";
        }
      }
      #endif
      /*------------------------------------------------------------------*/
      // Find the basic blocks that contain transition calls within each
      // function
      for (auto &F: M) {
        // Only look at tasks
        if( !(F.getName().startswith("task"))) {
          #if defined(TEST_ATTR) || defined(TEST_TG)
          if (F.hasFnAttribute("periph")) {
            outs() << F.getName() << " has attribute!\n";
          }
          #endif
          continue;
        }
        #if TEST_TG
        outs() << "checking " << F.getName() << "\n";
        #endif
        for (auto &B: F) {
          for (auto &I: B) {
            Instruction *i = &I;
            // Find the instructions that call transition_to
            if (isa<CallInst>(i)) {
              if(cast<CallInst>(i)->getCalledFunction() == NULL) {
                #if TEST_TG
                outs() << "Skipping!\n";
                #endif
                continue;
              }
              #if TEST_TG
              outs() << "Got call inst " << "\n";
              #endif
              cast<CallInst>(i)->getCalledFunction()->getName();
              if (cast<CallInst>(i)->getCalledFunction()->getName().
                  startswith("transition")) {
                  #if TEST_TG
                  outs() << "Got transition" << "\n";
                  outs() << "No really: " <<
                  cast<CallInst>(i)->getCalledFunction()->getName() << "\n";
                  #endif
                exitPoints[&F].push_back(&B);
                // Grab argument fed into transition
                auto arg = cast<CallInst>(i)->getCalledFunction()->arg_begin();
                #if TEST_TG
                outs() << arg->getArgNo() << " " << "\n";
                arg->print(llvm::outs()); 
                outs() << "\n";
                outs() << arg->getType()->isPointerTy() << "\n";
                outs() << arg->getType()->isFunctionTy() << "\n";
                outs() << isa<GlobalVariable>(arg) << "\n";
                outs() << "=========================\n";
                #endif
                if ( isa<GlobalVariable>(arg)) {
                  exitMatch[&B] = cast<GlobalVariable>(arg)->getName().str();
                  #if TEST_TG
                  outs() << "Got transition to some fellow task:  " << exitMatch[&B]  << "\n";
                  #endif
                }
                else {
                  for(auto Pi = B.rbegin(); Pi != B.rend(); ++Pi) {
                    int count;
                    count = 0;
                    #ifdef TEST_TG
                    outs() << "Looking down here...\n";
                    for (auto op = Pi->op_begin(); op != Pi->op_end();count++, ++op) {
                      if ( isa<GlobalValue>(op) &&
                        cast<GlobalValue>(op)->getName().startswith("task")) {
                        outs() << cast<GlobalValue>(op)->getName() << " " << count
                        << "\n";
                      }
                      if ( isa<GlobalValue>(op) &&
                        cast<GlobalValue>(op)->getName().startswith("_task")) {
                        outs() << cast<GlobalValue>(op)->getName() << " " << count
                        << "\n";
                      }
                    }
                    #endif
                    for (auto op = Pi->op_begin(); op != Pi->op_end();count++, ++op) {
                      if ( isa<GlobalValue>(op) ) {
                        auto gv = cast<GlobalValue>(op);
                        if ( gv->getName().startswith("task") ||
                          gv->getName().startswith("_task")) {
                          #ifdef TEST_TG
                          outs() << "Found var: " << gv->getName().str() << "\n";
                          #endif
                          exitMatch[&B] = gv->getName().str();
                          break;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        #ifdef TEST_TG
        outs() << F.getName() << " has " << exitPoints[&F].size() << " exits\n";
        #endif
      }
      outs() << "Done finding exit points\n";
      // Now build the successor list instead of the predecessor list
      for (auto &Fi : M) {
        int count = 0;
        for (auto arg = Fi.arg_begin(); arg != Fi.arg_end(); ++arg) {
          count++;
        }
        if (count > 0) {
          interrupt_dummy = Fi.arg_begin();
        }
        if( !(Fi.getName().startswith("task"))) {
          continue;
        }
        for (auto &Fj : M) {
          if( !(Fj.getName().startswith("task"))) {
            continue;
          }
          std::string str = "_task_";
          // build name of inner loop task
          str.append(Fj.getName());
          std::string str_short = Fj.getName();
          // If Fi is in the list of predecessors of Fj
          if( find(predTasks[str].begin(), predTasks[str].end(), &Fi) !=
            predTasks[str].end() ||
            find(predTasks[str_short].begin(), predTasks[str_short].end(), &Fi) !=
            predTasks[str_short].end()) {
            // Add Fj as a successor to Fi
            succTasks[&Fi].push_back(&Fj);
            #ifdef TEST_TG
            outs() << Fi.getName() << "-->" << Fj.getName() << "\n";
            #endif
          }
        }
      }
      outs() << "Built successor list\n";

      /*------------------------------------------------------------------*/
      // Test the tf
      #ifdef TEST_TF
      outs() << "Testing tf!" << " Looking for " << periphStates.size() <<
      "\n";
      std::map<std::string,std::vector<Instruction*>>testOut,testIn;
      for( auto &F: M) {
        // Only look at tasks for now
        outs() << F.getName() << "\n";
        if(!(F.getName().startswith("task"))) {
          continue;
        }
        outs() << "\nExamining task: " <<  F.getName() << "\n";
        for (auto &BB : F) {
          outs() << "\n------------------\n";
          // We're clearing Out the beginning so we can look at the effects of a
          // single block, but we're setting In to see if the tf correctly kills
          // stuff
          testIn.clear();
          for (auto it : periphStates) {
              testIn[it.first].push_back(periphStates[it.first].at(0));
          }
          testOut.clear();
          for (BasicBlock::iterator i = BB.begin(), e = BB.end(); i != e; ++i) {
            // force auto cast
            Instruction *I = &*i;
            // Jump past this instruction if it's calling another function.
            if( isa<CallInst>(i)) {
              continue;
            }
            tf(I, testOut, testIn);
            // Confirm that this forces a copy
            testIn = testOut;
          }
          if (DILocation *Loc = BB.front().getDebugLoc()) {
            outs() << "Testing block at line " << Loc->getLine() << "\n";
          }
          for (auto it : testOut) {
            outs() << "Test: " << it.first << " "  << it.second.size() << "\n";
            for (auto it2 : it.second) {
              if (DILocation *Loc = it2->getDebugLoc()) {
                unsigned Line = Loc->getLine();
                outs () << "\t" << Line << "\n";
              }
            }
          }
        }
      }
      #endif
      outs() << "Done other stuff\n";
#ifdef TEST_ISR
      outs() << "Fire up the interrupts!\n";
      std::map<std::string,std::vector<Instruction*>>inMap_ISR;
      std::map<std::string,std::vector<Instruction*>>outMap_ISR;

      for (auto &F : M) {
        // If name contains ISR
        if ( F.getName().endswith("ISR") ) {
          outs() << "Found interrupt! " << F.getName() << "\n";
          interrupts.push_back(&F);
          for (auto &B : F) {
            for (auto &I : B) {
              if(isa<ReturnInst>(I)) {
                exits_ISR[&F].push_back(&B);
              }
            }
          }
        }
      }
      outs( )<< "Applying outcomes\n";
      for (auto isr : interrupts) {
        inMap_ISR.clear();
        outMap_ISR.clear();
        iterate(isr,tf, meet_op_ISR, blockMap_ISR, inMap_ISR, outMap_ISR);
        for (auto &itB : exits_ISR[isr]) {
          for (auto locP : blockMap_ISR[itB]){
            outs() << "\t" << locP.first  << "\n";
            for (auto &locI : locP.second) {
              if (DILocation *Loc = locI->getDebugLoc()) {
                // TODO: make this less gross and less redundant
                Value *test;
                test = locI->getOperand(0);
                int64_t reportedVal = check_if_constant(test);
                unsigned Line = Loc->getLine();
                auto *Scope = cast<DIScope>(Loc->getScope());
                std::string fileName = Scope->getFilename();
                outs() << "Loc:\t" << fileName << ": " << Line  <<
                  " : Value :"<< reportedVal <<"\n";
              }
              else {
                outs() << "\t\t No line found\n";
              }
            }
          }
        }
      }

#endif
      /*------------------------------------------------------------------*/
      // Run iterate with another worklist so we get the effect of task
      // transitions too
      queue<Function *>worklist;
      queue<Function *>tasks;
      // Initialize blockMap to empty
      blockMap.clear();
      // Add all tasks to worklist
			int task_count = 0;
      for (auto &F : M) {
        if( !(F.getName().startswith("task"))) {
          continue;
        }
        worklist.push(&F);
        tasks.push(&F);
				task_count++;
      }
			if (task_count == 0) {
				for (auto &F : M) {
					if (F.getName() == "main") {
						worklist.push(&F);
					}
				}
			}
      outs() << "Built worklist\n";
      int changed;
      std::map<std::string,std::vector<Instruction*>>inMap;
      std::map<std::string,std::vector<Instruction*>>outMap;
      while( !worklist.empty()) {
        // Debug here to figure out when the heater stuff is getting smushed
        Function *task = worklist.front();
        worklist.pop();
        // Clear changed for every iteration
        changed = 0;
        // Get all of the outputs for the basic blocks containing exit points
        std::map<BasicBlock*,map<std::string,std::vector<Instruction*>>>exits;
        #ifdef TEST_WL
        BasicBlock* curBB = &(task->front());
        outs() << "Task worklist: " << task->getName();
        for( auto itP : blockMap[curBB]) {
          outs() << "\t" << itP.first << "\n";
        }
        outs() << " size: " << worklist.size() << "\n";
        outs() << "Task has " << exitPoints[task].size() << " exits\n";
        #endif
        // Iterate through all the basic block exit points for this function
        for (auto it : exitPoints[task]) {
          exits[it] = blockMap[it];
        }
        iterate(task, tf, meet_op, blockMap, inMap, outMap);
        for (auto it : exitPoints[task]) {
          if (exits[it] != blockMap[it]) {
            changed = 1;
            break;
          }
        }
        // If output is different, add successor tasks to worklist
        if (changed) {
          for (auto it : succTasks[task]) {
        #ifdef TEST_WL
        outs() << "Pushing: " << it->getName() << "\n";
        #endif
            worklist.push(it);
          }
        }
      }
      outs() << "Done pass!\n";
      // Print final outputs
      // cycle through functions
#ifndef TEST_VERBOSE
      outs() << "Analyzing task starts\n";
#else
      outs() << "Analyzing blocks\n";
#endif
      outs() << "===============================================================\n";
      visitBlockType blockVisits;
      for ( auto &F : M) {
        //TODO another kludge...
        if (F.getName().startswith("task") || F.getName().startswith("mai")) {
          outs() << "Analyzing " << F.getName() << "\n";
          for ( BasicBlock &BB : F) {
            #if 0
            for (auto disIt : disablers) {
              blockVisits.clear();
              outs() << "checking disabler: " << disIt.first;
              outs() << " on line ";
              easy_print(&BB);
              int temp;
              temp = searchEn(&BB, disIt.second, enablers[disIt.first],
              blockVisits);
              outs() << "Result is " << temp << " on line " ;
              easy_print(&BB);
            }
            #endif
            #ifdef TEST_VERBOSE
            outs() << "Looking at block on line: ";
            easy_print(&BB);
            #endif
            // Cycle through each peripheral
            for ( auto itP : blockMap[&BB]) {
              outs() << "\t" << itP.first << "\n";
              // Walk through all the instructions
              int64_t oldVal; 
              oldVal = -1;
              int64_t reportedVal; 
              reportedVal = -1;
              int64_t flag;
              flag = 0;
              if(itP.second.size() > 1) {
                for ( auto ItI : itP.second) {
                  Value *test;
                  test = ItI->getOperand(0);
                  // Double check that we're not just assigning the same
                  // typestate from a different program point
                  if ( isa<ConstantInt>(test)) {
                    int64_t newVal;
                    newVal = cast<ConstantInt>(test)->getSExtValue();
                    if ( oldVal > -1 && oldVal != newVal) {
                      // Set flag to indicate a mismatch
                      flag = 1; 
                    }
                    else {
                      oldVal = newVal;
                      reportedVal = oldVal;
                    }
                  }
                  else {
                    outs() << "[WARNING!] 944: Assigned a non-constant value to periph\n";
                  }
                }
                if (flag) {
                  #ifndef TEST_VERBOSE
                  outs() << "[WARNING] multiple reaching typestates at task entry\n";
                  #else
                  outs() << "[WARNING] multiple reaching states at block\n";
                  #endif
                }
                else {
                  outs() << "1) Same value: " << oldVal << "\n";
                }
              }
              else {
                Value *test;
                test = itP.second.front()->getOperand(0);
                reportedVal = check_if_constant(test);
              }
              for ( auto itI : itP.second) {
                if (DILocation *Loc = itI->getDebugLoc()) {
                  // TODO: make this less gross and less redundant
                  Value *test;
                  test = itI->getOperand(0);
                  reportedVal = check_if_constant(test);
                  unsigned Line = Loc->getLine();
                  auto *Scope = cast<DIScope>(Loc->getScope());
                  std::string fileName = Scope->getFilename();
                  outs() << "Loc:\t" << fileName << ": " << Line  <<
                    " : Value :"<< reportedVal <<"\n";
                }
              }
            }
            // A shameless hack so that we don't have to fight with the llvm
            // types to loop through basicblocks of a function
          #ifndef TEST_VERBOSE
              break;
        
          #endif
          }
          outs() << "\n";
        }
      }
      #ifndef TEST_VERBOSE
      outs() << "Analyzing exits:\n";
      outs() << "===============================================================\n";
      for ( auto itF : exitPoints) {
        // Walk through exit blocks
        outs() << "\n";
        outs() << itF.first->getName() << ":\n";
        for ( auto itEx : itF.second) {
          outs() << "Exit on line: ";
          easy_print(itEx);
          // Walk through all the peripherals
            //outs() << "----------" << blockMap[itEx]->first << "----------\n";
          for ( auto itP : blockMap[itEx]) {
            outs() << "\t" << itP.first << "\n";
            int64_t oldVal = -1;
            int64_t reportedVal = -1;
            // Walk through all the instructions
            if(itP.second.size() > 1) {
              for ( auto ItI : itP.second) {
                Value *test;
                test = ItI->getOperand(0);
                // Double check that we're not just assigning the same
                // typestate from a different program point
                if ( isa<ConstantInt>(test)) {
                  int64_t newVal;
                  newVal = cast<ConstantInt>(test)->getSExtValue();
                  if ( oldVal > -1 && oldVal != newVal) {
                    outs() << "[WARNING] multiple reaching typestates at task exit\n";
                  }
                  else {
                    oldVal = newVal;
                    outs() << "2) Same value: " << oldVal << "\n";
                  }
                }
                else {
                  outs() << "[ERROR!] Assigned a non-constant value to periph\n";
                }
              }
            }
            else {
              Value *test;
              test = itP.second.front()->getOperand(0);
              reportedVal = check_if_constant(test);
            }
            for ( auto itI : itP.second) {
              if (DILocation *Loc = itI->getDebugLoc()) {
                unsigned Line = Loc->getLine();
                auto *Scope = cast<DIScope>(Loc->getScope());
                std::string fileName = Scope->getFilename();
                outs() << "\t\t" << fileName << ": " << Line  <<
                  " : Value :"<< reportedVal <<"\n";
              }
            }
          }
        }
      }
      #endif
      outs() << "Analyzing all blocks\n";
      // Test for first task
      #if 0
      {
        Function *task = tasks.front();
        BasicBlock *myExitBB = exitPoints[task].front();
        LLVMContext& Ctx = M.getContext();
        SmallVector<ArgsTy,1> myArgs{Type::getInt16PtrTy(Ctx)};
        Constant* transFunc = M.getOrInsertFunction(
        "__pacarana_to_alpaca_transition", Type::getVoidTy(Ctx),
        myArgs,NULL);

      }
      #endif
      // Walking through the list of tasks
      outs() << "Looking for non-dominating typestate assignments\n";
      while( !tasks.empty()) {
        Function *task = tasks.front();
        tasks.pop();
        outs() << "********Task name: " << task->getName() << "********\n";
        std::vector<BasicBlock*> UseBBs;
        UseBBs.clear();
        outs() << "Periph block lines: \n";
        for (auto it: FuncPeriphMap[task]) {
          // Build up UseBB's
          easy_print(it.first->getParent());
          UseBBs.push_back(it.first->getParent());
        }
        outs() << "Exits:\n";
        for (auto it: exitPoints[task]) {
          easy_print(it);
        }
        DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*task).getDomTree();
        test_path_check(&DT, UseBBs, exitPoints[task]);
        
      }
#ifdef TEST_INF
      process_preloaded_functions("./inputs/preloaded_functions.csv");
      //TODO we need more testing on topo sort
      CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
        std::map< Function * , unsigned> visits;
        std::vector< Function * >list;
        for (auto &F : M) {
          Function *f = &F;
          CallGraphNode *cg = CG[f];
          if (cg->getFunction() != NULL && cg->getFunction()->size() != 0)  {
            outs() << "Adding function: " << cg->getFunction()->getName() <<
            "\n";
            visits[cg->getFunction()] = 0;
          }
        }
        for (auto it : visits) {
          if (!visits[it.first]) {
            topoSortCG(it.first,CG,visits,list);
          }
        }
        // We do want to cycle through from beginning to end since we loaded to
        // the back --> our leaves are at the front
       for (auto F : list) {
        if (F == NULL) { 
          outs() << "Why is F null??\n";
          continue;
        }
        // first check to see if this function was already profiled
        if (preloaded.find(F->getName()) != preloaded.end()) {
          Emap[F] = preloaded[F->getName()];
          continue;
        }
        int flag;
        flag = 0;
        for (auto &fi : M) {
          if (F == &fi) {
            flag = 1;
            outs() << "Counting insts in: " << F->getName();
            detailed_print(F); 
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(fi).getLoopInfo();
            unsigned count = 0;
            count = calc_insts(F,LI,loopIters,Emap);
            Emap[F] = count;
            outs() << "    count is: " << count << "\n";
            outs() << count << "\n";
          }
        }
        if (!flag) {
          outs() << "ya done screwed up\n";
        }
       }
#endif
#ifdef TEST_SPEC
      report_spec_match(M, blockMap);
#endif
#ifdef TEST_ATOMIC
      outs() << "Analyzing atomic block starts:\n";
      outs() << "==============================================================\n\n";
			for (auto &F : M) {
				Function *f = &F;
        report_atomic_typestates(f,blockMap);
			}
#endif
#ifdef TEST_EXTRA
      //TODO turn this nonsense into a function, this is gross
      outs() << "Running check extra!\r\n";
      callMapType allPossibleStates, periphPossibleStates;
      check_extra_functions(M, blockMap,allPossibleStates);
      // Walk throuh all the functions in the structure 
      for (auto itf : allPossibleStates) {
        outs() << "looking at function: " << itf.first->getName() << "\n";
        for (auto itCB : allPossibleStates[itf.first]) {
          outs() << "Looking at call on line ";
          easy_print(itCB.first);
          // Cycle through each peripheral
          for ( auto itP : allPossibleStates[itf.first][itCB.first]) {
            outs() << "Checking perip: " << itP.first;
            int64_t reportedVal;
            for (auto itI : itP.second) {
              if (DILocation *Loc = itI->getDebugLoc()) {
                // TODO: make this less gross and less redundant
                Value *test;
                test = itI->getOperand(0);
                reportedVal = check_if_constant(test);
                unsigned Line = Loc->getLine();
                auto *Scope = cast<DIScope>(Loc->getScope());
                std::string fileName = Scope->getFilename();
                outs() << "Loc:\t" << fileName << ": " << Line  <<
                  " : Value :"<< reportedVal <<"\n";
              }
            }
          }
        }
      }

      for (auto itf : allPossibleStates) {
        // Only grab driver functions
        if (!itf.first->hasFnAttribute("periph")) {
          continue;
        }
        outs() << "looking at driver function: " << itf.first->getName() << "\n";
        for (auto itCB : allPossibleStates[itf.first]) {
          outs() << "Looking at call on line ";
          easy_print(itCB.first);
          // plumb the map for that call block through iterate to get at every
          // block in the driver
          StateMapEntry tempBlocks;
          blockInstMap tempMap;
          tempBlocks.clear();
          tempMap.clear();
          iterate(itf.first,tf,meet_op,tempMap,
            allPossibleStates[itf.first][itCB.first],tempBlocks);
          Function *curF = itf.first;
          for (BasicBlock &itB : *curF) {
            BasicBlock *tB;
            tB = &itB;
            outs() << "For driver block on line ";
            easy_print(tB);
            // Cycle through each peripheral
            for ( auto itP : tempMap[tB]) {
              outs() << "Checking perip: " << itP.first;
              int64_t reportedVal;
              for (auto itI : itP.second) {
                if (DILocation *Loc = itI->getDebugLoc()) {
                  // TODO: make this less gross and less redundant
                  Value *test;
                  test = itI->getOperand(0);
                  reportedVal = check_if_constant(test);
                  unsigned Line = Loc->getLine();
                  auto *Scope = cast<DIScope>(Loc->getScope());
                  std::string fileName = Scope->getFilename();
                  outs() << "Loc:\t" << fileName << ": " << Line  <<
                    " : Value :"<< reportedVal <<"\n";
                }
              }
            }
          }
        }
      }
#endif
#ifdef TEST_INF
      BreakEvenMapType Cycles;
      parse_break_even_data("./inputs/peripheral_states_data.csv",peripherals,Cycles);
      outs() << "Running low usage check\r\n";
      report_low_usage(M, peripherals,blockMap, Emap, Cycles);
      outs() << "Running file read\r\n";
#endif
#ifdef TEST_AUTO
			outs() << "Running auto config\n";
      read_thresholds("thresholds.csv");
			find_activation_functions(M);
			Module * m = &M;
			find_toggle_flags(m);
			for (auto &F : M) {
				Function *f = &F;
				if (f != NULL){
						add_toggle(f,blockMap);
				}
			}
      return true;
#else
      return true;
#endif
    }
    virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
      ModulePass::getAnalysisUsage(AU);
      AU.addRequired<CallGraphWrapperPass>();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
    }
    private:
  };

  char TaskGraphs::ID = 0;
  //  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  //  initializeIPA(Registry);
  RegisterPass<TaskGraphs> X("tasks", "Pacarana Tasks");
}
