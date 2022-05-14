#ifndef _EX_FUNC_CHECK_H_
#define _EX_FUNC_CHECK_H_


#include  <map>
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "path_check.h"
using namespace llvm;

typedef std::map<Function *, map<BasicBlock*,StateMapEntry>>
    callMapType; 
typedef std::map<CallInst *, int> visitCallType;
typedef std::vector<BasicBlock *> BasicBlocks;

void check_extra_functions(Module &M, blockInstMap allowedStates,callMapType
  &allPossibleStates);

#endif
