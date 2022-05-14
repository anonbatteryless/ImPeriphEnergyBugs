#ifndef __AUTO_ACTIVATION_H_
#define __AUTO_ACTIVATION_H_
#include <string>

#include "llvm/IR/Module.h"
#include "path_check.h"
using namespace llvm;

void read_thresholds(std::string filename);
void find_activation_functions(Module &M);
void find_toggle_flags(Module *M);
void add_toggle(Function *F, blockInstMap &states);
//void add_toggle(Function *F);

#endif
