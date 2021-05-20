#include <string>
#include <cstring>
#include <filesystem>
#include <iostream>
#include "minc_api.hpp"
#include "minc_pkgmgr.h"

// LLVM IR creation
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/CFG.h>

// LLVM compilation
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

// LLVM optimization
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>

#include "minc_llvm.h"

using namespace llvm;

// Create context
LLVMContext context;

// Create builder
IRBuilder<> builder(context);

// Reference: https://lists.llvm.org/pipermail/llvm-dev/2018-May/123569.html
class DeadCodeEliminationPass : public llvm::FunctionPass
{
	static char ID;

	void deleteDeadCode(llvm::BasicBlock * basicBlock);
	llvm::BasicBlock * splitBasicBlock(llvm::BasicBlock *basicBlock, llvm::BasicBlock::iterator it);

public:
	DeadCodeEliminationPass() : llvm::FunctionPass(ID) {}

	bool runOnFunction(llvm::Function &F);
};

::Module::Module(StringRef ModuleID, LLVMContext& C)
	: llvm::Module(ModuleID, C), name(ModuleID), executionEngine(nullptr)
{
	static llvm::TargetMachine* target = nullptr;
	if (target == nullptr)
	{
		// Initialize target
		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();
		InitializeNativeTargetAsmParser();
		target = EngineBuilder().selectTarget();
	}

	setTargetTriple(sys::getDefaultTargetTriple());
	setDataLayout(target->createDataLayout());

	// Create pass manager for module
	funcPassMgr = new legacy::FunctionPassManager(this);
	modulePassMgr = new legacy::PassManager();
#ifdef OPTIMIZE
	PassManagerBuilder passMgrBuilder;
	passMgrBuilder.OptLevel = 3; // -O3
	passMgrBuilder.SizeLevel = 0;
	passMgrBuilder.Inliner = createFunctionInliningPass(passMgrBuilder.OptLevel, passMgrBuilder.SizeLevel, false);
	passMgrBuilder.DisableUnrollLoops = false;
	passMgrBuilder.LoopVectorize = true;
	passMgrBuilder.SLPVectorize = true;
	target->adjustPassManager(passMgrBuilder);
	passMgrBuilder.populateFunctionPassManager(*funcPassMgr);
	passMgrBuilder.populateModulePassManager(*modulePassMgr);
#endif
	funcPassMgr->add(new DeadCodeEliminationPass());
	funcPassMgr->doInitialization();
}

::Module::~Module()
{
	delete modulePassMgr;
	delete funcPassMgr;
}

Function* ::Module::createFunction(FunctionType *Ty, GlobalValue::LinkageTypes Linkage, const Twine &N)
{
	Function* function = llvm::Function::Create(Ty, Linkage, N, *this);
	functions.push_back(function);
	return function;
}

void ::Module::finalize()
{
	std::string errstr;
	raw_string_ostream errstream(errstr);

	// Verify and optimize functions and module
	for (Function* function: functions)
	{
		if (verifyFunction(*function, &errstream))
		{
			if (errstr.empty())
			{
print(outs(), nullptr);
				throw CompileError("Error verifying function");
			}
			else
			{
print(outs(), nullptr);
				throw CompileError("Error verifying function: " + errstr);
			}
		}
		funcPassMgr->run(*function);
	}
	funcPassMgr->doFinalization();
	if (verifyModule(*this, &errstream))
	{
		if (errstr.empty())
		{
print(outs(), nullptr);
			throw CompileError("Error verifying module");
		}
		else
		{
print(outs(), nullptr);
			throw CompileError("Error verifying module: " + errstr);
		}
	}
	modulePassMgr->run(*this);
}

void ::Module::dump()
{
	// Print IR to stdout
	//module->print(outs(), nullptr);

	// Print IR to file
	std::error_code ec;
	raw_fd_ostream ostream(name + ".ll", ec);
	print(ostream, nullptr);
	ostream.close();
}

uint64_t ::Module::getFunctionAddress(const std::string& name)
{
	if (executionEngine == nullptr)
		executionEngine = EngineBuilder(std::unique_ptr<Module>(this)).create();

	// Load all dependent and sub-dependent modules
	std::set<::Module*> recursiveDependencies;
	std::stack<::Module*> recursiveDependencyStack;
	recursiveDependencies.insert(this);
	recursiveDependencyStack.push(this);
	while (!recursiveDependencyStack.empty())
	{
		::Module* currentModule = recursiveDependencyStack.top(); recursiveDependencyStack.pop();
		for (::Module* dependency: currentModule->dependencies)
			if (recursiveDependencies.find(dependency) == recursiveDependencies.end())
			{
				executionEngine->addModule(std::unique_ptr<Module>(dependency));
				recursiveDependencies.insert(dependency);
				recursiveDependencyStack.push(dependency);
			}
	}

	// Get executable function
	return executionEngine->getFunctionAddress(name);
}

void ::Module::addDependency(::Module* module)
{
	dependencies.insert(module);
}

char DeadCodeEliminationPass::ID = 0;

void DeadCodeEliminationPass::deleteDeadCode(BasicBlock * basicBlock)
{
	for (auto it = basicBlock->begin(); it != basicBlock->end(); ++it)
	{
		// Split after first return instruction.
		if (it->getOpcode() == Instruction::Ret)
		{
			++it;
			// Split only if there is a following instruction.
			if (it != basicBlock->getInstList().end())
			{
				auto deadCodeBlock = splitBasicBlock(basicBlock, it);
				deadCodeBlock->eraseFromParent();
			}
			return;
		}
	}
}


BasicBlock * DeadCodeEliminationPass::splitBasicBlock(BasicBlock *basicBlock, BasicBlock::iterator it)
{
	assert(basicBlock->getTerminator() && "Block must have terminator instruction.");
	assert(it != basicBlock->getInstList().end() && "Can't split block since there is no following instruction in the basic block.");

	auto newBlock = BasicBlock::Create(basicBlock->getContext(), "splitedBlock", basicBlock->getParent(), basicBlock->getNextNode());

	// Move all of the instructions from original block into new block.
	newBlock->getInstList().splice(newBlock->end(), basicBlock->getInstList(), it, basicBlock->end());

	// Now we must loop through all of the successors of the New block (which
	// _were_ the successors of the 'this' block), and update any PHI nodes in
	// successors.  If there were PHI nodes in the successors, then they need to
	// know that incoming branches will be from New, not from Old.
	//
	for (succ_iterator I = succ_begin(newBlock), E = succ_end(newBlock); I != E; ++I)
	{
		// Loop over any phi nodes in the basic block, updating the BB field of
		// incoming values...
		BasicBlock *Successor = *I;
		for (auto &PN : Successor->phis())
		{
			int Idx = PN.getBasicBlockIndex(basicBlock);
			while (Idx != -1)
			{
				PN.setIncomingBlock((unsigned)Idx, newBlock);
				Idx = PN.getBasicBlockIndex(basicBlock);
			}
		}
	}

	return newBlock;
}

bool DeadCodeEliminationPass::runOnFunction(Function& func)
{
	for (auto& bb: func.getBasicBlockList())
		deleteDeadCode(&bb);
	return false;
}