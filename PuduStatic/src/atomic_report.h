#ifndef __ATOMIC_REPORT_H_
#define __ATOMIC_REPORT_H_

#include "llvm/IR/Module.h"
#include "path_check.h"
using namespace llvm;

void report_atomic_typestates(Function *F,blockInstMap &states);

#endif
