#ifndef _TASKS_H_
#define _TASKS_H_

using namespace llvm;


void tf(Instruction *I,
  std::map<std::string,std::vector<Instruction*>>&output,
  std::map<std::string,std::vector<Instruction*>>&input);

void meet_op(BasicBlock* BB,
  std::map<std::string,std::vector<Instruction*>>&inMap,
  map<BasicBlock*, map<std::string,std::vector<Instruction*>>> &blockMap);

void mergeMaps(BasicBlock* Pred,
  std::map<std::string,std::vector<Instruction*>>&inMap,
  map<BasicBlock*, map<std::string,std::vector<Instruction*>>> &blockMap);

int64_t check_if_constant(Value *test);

#endif
