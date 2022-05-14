#ifndef _INT_CONCURRENCY_H_
#define INT_CONCURRENCY_H_
#include  <map>
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

using namespace llvm;

#define VISITED 1
typedef std::map<std::string, Function*> disEnMapType;
typedef std::map<BasicBlock *, int> visitBlockType;

void find_isr_disEn(Module &M, disEnMapType &enables, disEnMapType &disables);
int searchEn(BasicBlock *BB, Function *dis, Function *en, visitBlockType &bl);
int checkDisable(BasicBlock *BB, Function *dis, Function *en,visitBlockType
&bl);
bool containsFunc(Function *F, BasicBlock *B);

#endif

