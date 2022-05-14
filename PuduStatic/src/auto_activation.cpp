#include "auto_activation.h"
#include "dataflow.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ilist.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/GenericDomTreeConstruction.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/ADT/APInt.h"

#include <string>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#define CSV_IO_NO_THREAD
#include "csv.h"
#include "tasks.h"
#include "inference.h"

//Map of function names to the threshold levels of their knob variables for
//different peripherals
//Example access: thresholds[fft][gyro_80]
std::map<std::string,std::map<std::string,int>>thresholds;

// vector of functions
std::vector<std::string>simple_funcs;
// Vector of peripherals
std::vector<std::string>simple_periphs;

// Map of peripherals to sleep functions
std::map<std::string,Function*>periph_sleepers;
// Map of peripherals to restore functions
std::map<std::string,Function*>periph_restorers;

// Array corresponding to the state flags we flip
std::map<std::string,Value*>toggleFlags;


int index_find(std::vector<std::string>x,std::string key) {
	int i = 0;
	for (auto it : x) {
		if ( it == key ) {
			break;
		}
		i++;
	}
	return i;
}

// Identifies all of the toggle flags
void find_toggle_flags(Module *M) {
	for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi) {
		GlobalVariable *gv;
		gv = &(*gi);
		//Value *valGi = cast<Value*>(gi);
		if (gi->getName().find("_paca_flag")!= std::string::npos) {
			toggleFlags[gi->getName().split("_").first] = gv;
		}
	}
}


// Tracks down the labeled restore and sleep functions and the paca_flag
void find_activation_functions(Module &M) {
	outs() << "Inside find_activation\r\n";
	for (auto &F : M) {
		//If function name starts with keyword, add to one map or the other
		if(F.getName().startswith("_restore__")){
			std::string cur_periph = F.getName().split("__").second;
			// TODO actually dig out the function here or somehow confirm that we're
			// not making two function calls (the func we actually want is nested
			// inside of another one thanks to the macros)
			periph_restorers[cur_periph] = &F;
			outs() << "Found restore" << F.getName() << "\n";
		}
		if(F.getName().startswith("_sleep__")){
			std::string cur_periph = F.getName().split("__").second;
			periph_sleepers[cur_periph] = &F;
			outs() << "Found sleep" << periph_sleepers[cur_periph]->getName() <<
			" for periph "<< cur_periph << "\n";
		}
	}
}

// Processes csv file with threshold information
// format is periph typestates listed across the first row, function names along
// the first column and knob variable value where the runtime surpasses the
// break-even point
void read_thresholds(std::string filename) {
  io::LineReader in(filename);
  // parse first line to get all of the peripherals
  StringRef header = StringRef(in.next_line());
  std::vector<std::string>periphs;
  // This first split should snag "function" at the start of the header
  auto str_out = header.split(",");
  while(str_out.second != "") {
    str_out = str_out.second.split(",");
    // TODO figure out the casting here
    periphs.push_back(str_out.first);
		std::string periph_short = str_out.first.split("_").first;
		//add periph to simple_periphs if not in there yet
		if (count(simple_periphs.begin(),simple_periphs.end(),periph_short) < 1) {
			simple_periphs.push_back(periph_short);
		}
    //outs() <<"Adding " << str_out.first << "\n";
  }
  // Now we go through and grab each line
  while(char*line = in.next_line()) {
    //First field is the function name
    StringRef cur = StringRef(line);
    StringRef func = cur.split(",").first;
		simple_funcs.push_back(func);
    int i = 0;
    while(cur.split(",").second != "") {
      cur = cur.split(",").second;
      int new_int = 0;
      //outs()  << cur.split(",").first << "\n";
      cur.split(",").first.trim(" ").getAsInteger(10,new_int);
      outs() <<"Adding int: " << new_int << " on func " <<
			func << " and periph " << periphs[i] << "\n";
      thresholds[func][periphs[i]] = new_int;
      outs() << "Double check " << thresholds[func][periphs[i]] << "\n";
			i++;
    }
  }
}

// Checks function runtime vs peripheral and returns the sleep and restore
// functions if the function's runtime is likely to pass the peripheral's
// break-even point
void check_toggle_conditions(Function **sleeper, Function **restorer,std::string
		periph, Function *fut, Value *knob) {
	//outs() << "Checking toggle " << knob->getValue() << "\n";
	// Check if knob variable is large enough
	int64_t knob_val;
	if(ConstantInt *ci = dyn_cast<ConstantInt>(knob)) {
		//outs() << "Running cast " << ci->getValue() << "\n";
		knob_val = ci->getZExtValue();
		//outs() << "knob is " << knob_val << "\n";
	}
	else {
		knob_val = -1;
	}
	if(knob_val < 0) {
		outs() << "Error! Non-constant knob val!\n";
		return;
	}
	if(thresholds[fut->getName()][periph] <= knob_val) {
		*sleeper = periph_sleepers[periph];
		*restorer = periph_restorers[periph];
	}
}

// Handles the general case where we add in code that pulls the break-even point
// from the runtime value of the typestate and the knob value
void insert_multi_state_branch(Instruction **disI, Instruction **reI, std::string periph,
	std::string periphState, FunctionType * toggleType, Function* fut,
	Value* knob) {
	Instruction *I = *disI;
	// Insert walk through options here to find one where we're disabled AND
	// one where we're not
	int p, f;
	p = index_find(simple_periphs, periph);
	f = index_find(simple_funcs, fut->getName());
	LLVMContext &Ctx = fut->getContext();
	std::vector<Type*> locParamTypes =
		{Type::getInt16Ty(Ctx),Type::getInt16PtrTy(Ctx),Type::getInt16Ty(Ctx)};
	Type *locRetType = Type::getInt16Ty(Ctx);
	FunctionType *checkFuncType =
		FunctionType::get(locRetType,locParamTypes,false);
	Constant *checkFunc = fut->getParent()->getOrInsertFunction(
		"__paca_read_table", checkFuncType);
	// Assemble arguments
	Value *periphVal = ConstantInt::get(llvm::Type::getInt16Ty(Ctx),p);
	Value *funcVal = ConstantInt::get(llvm::Type::getInt16Ty(Ctx),f);
	Value *stateVal = NULL;
	for (auto pe : peripherals) {
		auto temp = pe->getName().find(periph);
		if (temp != std::string::npos) {
			stateVal = pe;
			break;
		}
	}
	if  (stateVal == NULL) {
		outs() << "Error! no state found\n";
	}
	std::vector<Value*>args;
	args.push_back(funcVal);
	args.push_back(stateVal);
	args.push_back(periphVal);
	IRBuilder<>builder(I);
  // Get debug information
  unsigned Line;
  std::string fileName;
  if (DILocation *Loc = I->getDebugLoc()) {
    Line = Loc->getLine();
    auto *Scope = cast<DIScope>(Loc->getScope());
    fileName = Scope->getFilename();
  }
	builder.SetInsertPoint(I);
	builder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	Value * breakPoint = builder.CreateCall(checkFunc,args);
	// Update disI so we can hand it to the next insert call one function up
	*disI = &(*(--builder.GetInsertPoint()));
	// Get sleeper functions
	BasicBlock *B = I->getParent();
	Function *F  = B->getParent();
	Function *sleeper = periph_sleepers[periph];
	Function * restorer = periph_restorers[periph];
	Constant *sleepFunc = F->getParent()->getOrInsertFunction(sleeper->getName(),toggleType);
	Constant *restoreFunc = F->getParent()->getOrInsertFunction(restorer->getName(), toggleType);
	// TODO figure out if using knobLess will work when we stack up peripherals
	Value *knobLess = builder.CreateICmpULT(breakPoint,knob);
	Value *zeroVal = ConstantInt::get(llvm::Type::getInt16Ty(Ctx),0);
	// Check that breakpoint exists (i.e. because state is not disabled)
	Value *validState = builder.CreateICmpULT(zeroVal,breakPoint);
	Value *takeBranch = builder.CreateAnd(validState,knobLess);
	auto last = SplitBlockAndInsertIfThen(takeBranch, I,false,nullptr,nullptr,nullptr);
	// Add enable/disable
	IRBuilder<>disBuilder(last);
	disBuilder.SetInsertPoint(last);
	disBuilder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	disBuilder.CreateCall(sleepFunc);
	// Inc builder point to the reenable insert point
	Instruction *newPoint = *reI;
	auto newLast = SplitBlockAndInsertIfThen(takeBranch,newPoint,false,nullptr,nullptr,nullptr);
	disBuilder.SetInsertPoint(newLast);
	disBuilder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	disBuilder.CreateCall(restoreFunc);
	//Leave rei alone-- we want to keep using reI as our split point so all of the
  // if/then code is before it
	//*reI = ((*reI)->getNextNode());

  // Print out the code we're adding
  outs( ) << "Adding following code before original line " << Line << " file "
    << fileName << "\n";
  outs() << "int __breakpoint = " << checkFunc->getName() << "("<< f << "," <<
    stateVal->getName() << "," << p <<
    ");\nif( __breakpoint > 0 && knob > __breakpoint){"
    << sleepFunc->getName() << "();}\n";
  outs( ) << "Adding following code after original line " << Line << " file "
    << fileName << "\n";
  outs( )<< "if( __breakpoint > 0 && knob > __breakpoint){"
     << restoreFunc->getName() << "();}\n";
}

// Handles the optimization where we check the knob vs the break-even threshold
// at compile time because everything is fixed.
void insert_const_single_state_opt(Instruction **disI, Instruction **reI,
	std::string periph, FunctionType * toggleType, Function* fut) {
	Instruction *I = *disI;
  // Get debug information
  unsigned Line;
  std::string fileName;
  if (DILocation *Loc = I->getDebugLoc()) {
    Line = Loc->getLine();
    auto *Scope = cast<DIScope>(Loc->getScope());
    fileName = Scope->getFilename();
  }
	Function *sleeper = periph_sleepers[periph];
	Function * restorer = periph_restorers[periph];
	// Add enable/disable
	//outs() << "Adding sleeper " << sleeper->getName() << "\n";
	Constant *sleepFunc = fut->getParent()->getOrInsertFunction(sleeper->getName(),toggleType);
	Constant *restoreFunc = fut->getParent()->getOrInsertFunction(
	restorer->getName(), toggleType);
	//outs() << "Adding restorer " << restorer->getName() << "\n";
  outs( ) << "Adding following code before original line " << Line << " file "
    << fileName << "\n";
  outs() << sleepFunc->getName() << "();\n";
  outs() << "Adding following code after original line " << Line << " file " <<
    fileName << "\n";
  outs() << restoreFunc->getName() << "();\n";
	IRBuilder<>builder(I);
	builder.SetInsertPoint(I);
	builder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	builder.CreateCall(sleepFunc);
	builder.SetInsertPoint(*reI);
	builder.CreateCall(restoreFunc);
}

// Handles the optimization where we just check the knob value at runtime, the
// typestate is fixed
void insert_nonconst_single_state_opt(Instruction **disI, Instruction **reI,
	std::string periph, std::string periphState, FunctionType * toggleType,
	Function* fut, Value* knob) {
	Instruction *I = *disI;
	BasicBlock *B = I->getParent();
	Function *F = B->getParent();
	Function *sleeper = periph_sleepers[periph];
	Function * restorer = periph_restorers[periph];
	//outs() << "Adding sleeper knob < 0;" << sleeper->getName() << "\n";
	Constant *sleepFunc = F->getParent()->getOrInsertFunction(sleeper->getName(),toggleType);
	Constant *restoreFunc = F->getParent()->getOrInsertFunction(restorer->getName(), toggleType);
	//outs() << "Adding restorer knob < 0;"<< restorer->getName() << "\n";
	// Insert if statements before and after func
	IRBuilder<>builder(I);
  // Get debug information
  unsigned Line;
  std::string fileName;
  if (DILocation *Loc = I->getDebugLoc()) {
    Line = Loc->getLine();
    auto *Scope = cast<DIScope>(Loc->getScope());
    fileName = Scope->getFilename();
  }
	//outs() << "Indexing with " << fut->getName() << " " << periphState << "\n";
	int breakval = thresholds[fut->getName()][periphState] - 1;
	LLVMContext &Ctx = fut->getContext();
	ConstantInt *breakEvenInt = ConstantInt::get(Ctx,llvm::APInt(16,breakval,false));
	//outs() << "Checking breakval " << breakval << " vs " << *breakEvenInt << "\n";
	Value *knobLess = builder.CreateICmpULT(breakEvenInt,knob);
  outs( ) << "Adding following code before original line " << Line << " file "
    << fileName << "\n";
  outs() << "if (knob > " << breakEvenInt->getValue() << "){ " << sleepFunc->getName() <<
    "();}\n";
  outs() << "Adding following code after original line " << Line << " file " <<
    fileName << "\n";
  outs() << "if (knob > " << breakEvenInt->getValue() << "){ " << restoreFunc->getName() <<
    "();}\n";
	auto last = SplitBlockAndInsertIfThen(knobLess, I,false,nullptr,nullptr,nullptr);
	// Add enable/disable
	IRBuilder<>disBuilder(last);
	disBuilder.SetInsertPoint(last);
	disBuilder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	disBuilder.CreateCall(sleepFunc);
	// Inc builder point
	Instruction *newPoint = *reI;
	auto newLast = SplitBlockAndInsertIfThen(knobLess,newPoint,false,nullptr,nullptr,nullptr);
	disBuilder.SetInsertPoint(newLast);
	disBuilder.SetCurrentDebugLocation(findClosestDebugLoc(I));
	disBuilder.CreateCall(restoreFunc);
	return;
}

// Checks the function runtime vs active peripherals and adds in toggling
// code/conditions
void insert_toggle_conditions(periphStateMap &periphs, Function *fut, Value
*knob, Instruction *I) {
	Instruction *insertDis = I;
	Instruction *insertRe = I->getNextNode();
	if (insertRe == NULL) {
		outs() << "Error! insert reenable not defined!\n";
	}
	// Check if knob variable is large enough
	int64_t knob_val;
	if(ConstantInt *ci = dyn_cast<ConstantInt>(knob)) {
		//outs() << "Running cast " << ci->getValue() << "\n";
		knob_val = ci->getZExtValue();
		//outs() << "knob is " << knob_val << "\n";
	}
	else {
		// We'll use the knob_val variable to indicate that the knob argument is not
		// a constant
		knob_val = -1;
	}
	// Select periph here
	std::string periph;
	std::string periphState;
	LLVMContext &Ctx = fut->getContext();
	//For now we'll just stack peripheral calls assuming the lowest power
	//peripherals are listed first
  outs() << "Looking at " << periphs.size() << " periphs\r\n";
	for (auto itP : periphs) {
		int multiState = 0;
		periph = StringRef(itP.first).split("_").first;
    if (periph_sleepers.count(periph) == 0) {
      outs() << "ERROR: No sleep function provided. Continuing!\r\n";
      return;
    }
		//outs() << "Analyzing periph " << periph << "\n";
		// append typestate to the peripheral
		periphState = periph + "_" + std::to_string(itP.second.front());
		if (itP.second.size() > 1) {
			multiState = 1;
		}
		// Declare types n'at for setting up toggling functions call
		std::vector<Type*> paramTypes = {};
		Type *retType = Type::getVoidTy(Ctx);
		FunctionType *toggleType = FunctionType::get(retType,paramTypes,false);
		// Trigger optimizations
		if (multiState == 0 && itP.second.front() != 0) {
			// If only one peripheral and knob val is *not* constant
			if (knob_val < 0) {
				outs() << "\tEntering nonconst single state opt\r\n";
				insert_nonconst_single_state_opt(&insertDis, &insertRe, periph,
				periphState, toggleType, fut, knob);
			}
			// If only one periph typestate is possible and knob_val is constant
			else if(thresholds[fut->getName()].count(periphState) > 0 &&
				thresholds[fut->getName()][periphState] <= knob_val) {
				outs() << "\tEntering const single state opt\r\n";
				insert_const_single_state_opt(&insertDis,&insertRe,periph,toggleType,fut);
			}
			else {
				outs() << "\tKnob val constant, too small, comp result for : " <<
        periphState << " " << 
				thresholds[fut->getName()].count(periphState) << " " <<
				thresholds[fut->getName()][periphState] << " " << knob_val << "\n";
			}
		}
		else {
			outs() << "\tEntering multistate generic function\r\n";
			insert_multi_state_branch(&insertDis, &insertRe, periph, periphState,
			toggleType, fut, knob);
		}
    if (insertRe == NULL) {
      outs() << "Warning:  insertion point is NULL\r\n";
      return;
    }

	}
	//outs() << "done insert\n";
}

// Main function of this whole file, we assume this is fed in by the main pass
// This function adds in toggle code around calls to the input function
void add_toggle(Function *F,blockInstMap &states) {
  // Check if function is in thresholds map
	if (thresholds.count(F->getName()) == 0) {
		return;
	}
  outs( )<< "Found function " << F->getName() << "\n";
  //Walk through Callsites
  for (User *U : F->users()) {
		Instruction *I = dyn_cast<Instruction>(U);
		if (I == NULL) {
			continue;
		}
		BasicBlock *B = I->getParent();
    CallSite CS(I);
    Value *arg = CS.getArgument(0);
		// At each call site, check for peripheral typestates
		periphStateMap active_periphs;
		get_typestates(states, active_periphs, B);
		// Check toggle condition & insert code
    outs() << "About to insert toggle conditions\r\n";
		insert_toggle_conditions(active_periphs,F,arg,I);
  }
}
