#define OUTPUT_JIT_CODE
const bool ENABLE_JIT_CODE_DEBUG_SYMBOLS = false;
const bool OPTIMIZE_JIT_CODE = true;

// STD
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <unistd.h>

// LLVM-C //DELETE
#include <llvm-c/Core.h> //DELETE

// LLVM IR creation
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

// LLVM compilation
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

// LLVM execution
//#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
//#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/GenericValue.h"

// LLVM optimization
#include <llvm/Transforms/Coroutines.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>

#include "ast.h"
#include "codegen.h"
#include "llvm_constants.h"
#include "KaleidoscopeJIT.h"
#include "module.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern Function* currentFunc;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;
extern KaleidoscopeJIT* jit;
extern DIFile* dfile;
extern Value* closure;

std::string currentSourcePath;
DIBasicType* intType;

XXXModule::XXXModule(const std::string& moduleName, const Location& loc, bool outputDebugSymbols, bool optimizeCode)
	: prevModule(currentModule), prevDbuilder(dbuilder), prevDfile(dfile), prevFunc(currentFunc), prevBB(currentBB), loc(loc)
{
	// Create module
	module = std::make_unique<Module>(moduleName, *context);
	currentModule = module.get();
	module->setDataLayout(jit->getTargetMachine().createDataLayout());

	if (outputDebugSymbols)
	{
		// Create debug builder
		currentModule->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);
		dbuilder = new DIBuilder(*currentModule);
		DIFile* difile = nullptr;
		if (strcmp(loc.filename, "-") != 0)
		{
			const std::string sourcePath = loc.filename;
			size_t slpos = sourcePath.find_last_of("/\\");
			difile = dbuilder->createFile(sourcePath.substr(slpos + 1), sourcePath.substr(0, slpos));
		}
		DICompileUnit* dcu = dbuilder->createCompileUnit(dwarf::DW_LANG_C, difile, "minc", 0, "", 0);
		dfile = dbuilder->createFile(dcu->getFilename(), dcu->getDirectory());

		// Create primitive types
		intType = dbuilder->createBasicType("int", 32, dwarf::DW_ATE_signed);
	}
	else
	{
		// Disable debug symbol generation
		dbuilder = nullptr;
		dfile = nullptr;
	}

	if (optimizeCode)
	{
		// Create pass manager for module
		jitPassManager = new legacy::FunctionPassManager(module.get());
		PassManagerBuilder jitPassManagerBuilder;
		jitPassManagerBuilder.OptLevel = 3; // -O3
		jitPassManagerBuilder.SizeLevel = 0;
		jitPassManagerBuilder.Inliner = createFunctionInliningPass(jitPassManagerBuilder.OptLevel, jitPassManagerBuilder.SizeLevel, false);
		jitPassManagerBuilder.DisableUnitAtATime = false;
		jitPassManagerBuilder.DisableUnrollLoops = false;
		jitPassManagerBuilder.LoopVectorize = true;
		jitPassManagerBuilder.SLPVectorize = true;
		jit->getTargetMachine().adjustPassManager(jitPassManagerBuilder);
		//addCoroutinePassesToExtensionPoints(jitPassManagerBuilder);
		jitPassManagerBuilder.populateFunctionPassManager(*jitPassManager);
		jitPassManager->doInitialization();
	}
	else
		jitPassManager = nullptr;
}

void XXXModule::finalize()
{
	// Close main function
	if (dbuilder)
	{
		dbuilder->finalizeSubprogram(currentFunc->getSubprogram());
		builder->SetCurrentDebugLocation(DebugLoc());
	}

	if (currentBB != prevBB)
		builder->SetInsertPoint(currentBB = prevBB);

	std::string errstr;
	raw_string_ostream errstream(errstr);
	bool haserr = verifyFunction(*currentFunc, &errstream);

	// Close module
	if (dbuilder)
		dbuilder->finalize();

	if (jitPassManager && !haserr)
		jitPassManager->run(*currentFunc);

	currentFunc = prevFunc; // Switch back to parent function
	currentModule = prevModule; // Switch back to file module
	dbuilder = prevDbuilder; // Reenable debug symbol generation
	dfile = prevDfile;

	if (haserr && errstr[0] != '\0')
	{
		char* errFilename = new char[strlen(loc.filename) + 1];
		strcpy(errFilename, loc.filename);
		Location* errloc = new Location{errFilename, loc.begin_line, loc.begin_col, loc.end_line, loc.end_col};
		throw CompileError("error compiling module\n" + errstr, *errloc);
	}
}

void XXXModule::print(const std::string& outputPath)
{
	std::error_code ec;
	raw_fd_ostream ostream(outputPath, ec);
	module->print(ostream, nullptr);
	ostream.close();
}

void XXXModule::print()
{
	module->print(outs(), nullptr);
}

bool XXXModule::compile(const std::string& outputPath, std::string& errstr)
{
module->setTargetTriple(sys::getDefaultTargetTriple());
module->setDataLayout(jit->getTargetMachine().createDataLayout());

std::error_code EC;
raw_fd_ostream dest(outputPath, EC, sys::fs::F_None);

if (EC)
{
	errstr = "Could not open file: " + EC.message();
	return false;
}

legacy::PassManager pass;
auto FileType = TargetMachine::CGFT_ObjectFile;

if (jit->getTargetMachine().addPassesToEmitFile(pass, dest, nullptr, FileType))
{
	errstr = "TheTargetMachine can't emit a file of this type";
	return false;
}

pass.run(*module);
dest.flush();
return true;
}

void XXXModule::run()
{
	assert(0);
}

FileModule::FileModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, bool optimizeCode)
	: XXXModule(sourcePath == "-" ? "main" : sourcePath, { sourcePath.c_str(), 1, 1, 1, 1 }, outputDebugSymbols, optimizeCode), prevSourcePath(currentSourcePath = sourcePath)
{
	// Generate main function
	FunctionType *mainType = FunctionType::get(Types::Int32, {}, false);
	mainFunc = currentFunc = Function::Create(mainType, Function::ExternalLinkage, "main", currentModule);
	mainFunc->setDSOLocal(true);
	mainFunc->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::NoInline);

	if (dbuilder)
	{
		DIScope *FContext = dfile;
		unsigned LineNo = 1, ScopeLine = 1;
		DISubprogram *SP = dbuilder->createFunction(
			FContext, "main", StringRef(), dfile, LineNo,
			dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray(SmallVector<Metadata*, 8>({ intType }))),
			ScopeLine, DINode::FlagPrototyped, DISubprogram::SPFlagDefinition)
		;
		mainFunc->setSubprogram(SP);
	}

	// Create entry BB in main function
	builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", mainFunc));
}

void FileModule::finalize()
{
	// Add implicit `return 0;` to main function
	if (!currentBB->getTerminator())
		builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));

	currentSourcePath = prevSourcePath;
//		XXXModule::finalize();

try {
	XXXModule::finalize();
}
catch (CompileError err) {
	print("error.ll");
	throw;
}
}

void FileModule::run()
{
	ExecutionEngine* EE = EngineBuilder(std::unique_ptr<Module>(module.get())).create();
	int result = EE->runFunctionAsMain(mainFunc, {}, nullptr);
	outs() << "./minc Result: " << result << "\n";
}

void JitFunction::init()
{
	// Create JIT
	jit = new KaleidoscopeJIT();
}

JitFunction::JitFunction(BlockExprAST* parentBlock, BlockExprAST* blockAST, Type *returnType, std::vector<ExprAST*>& params, std::string& name)
	: XXXModule("module", blockAST->loc, ENABLE_JIT_CODE_DEBUG_SYMBOLS, OPTIMIZE_JIT_CODE), jitModuleKey(0), name(name), closureType(StructType::create(*context, "closureType"))
{
	closureType->setBody(ArrayRef<Type*>());

//capturedScope.clear();

	// Create function
	funcType = FunctionType::get(returnType, {
		Types::LLVMOpaqueBuilder->getPointerTo(),
		Types::LLVMOpaqueModule->getPointerTo(),
		Types::LLVMOpaqueValue->getPointerTo(),
		Types::BlockExprAST->getPointerTo(),
		Types::ExprAST->getPointerTo()->getPointerTo(),
		closureType->getPointerTo()
	}, false);
	Function* jitFunction = currentFunc = Function::Create(funcType, Function::ExternalLinkage, name, module.get());

	if (dbuilder)
	{
		unsigned ScopeLine = blockAST->loc.begin_line;
		DISubprogram *SP = dbuilder->createFunction(
			dfile, name, StringRef(), dfile, loc.begin_line,
			dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray(SmallVector<Metadata*, 8>({ }))),
			ScopeLine, DINode::FlagPrototyped, DISubprogram::SPFlagDefinition)
		;
		currentFunc->setSubprogram(SP);
		builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, 1, SP));
	}
	else
		builder->SetCurrentDebugLocation(DebugLoc());
	

	// Create entry BB in currentFunc
	builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", currentFunc));

	AllocaInst* builderPtr = builder->CreateAlloca(Types::LLVMOpaqueBuilder->getPointerTo(), nullptr, "builder");
	builderPtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin(), builderPtr)->setAlignment(8);
	blockAST->addToScope("builder", BuiltinTypes::LLVMBuilderRef, new XXXValue(builderPtr));

	AllocaInst* modulePtr = builder->CreateAlloca(Types::LLVMOpaqueModule->getPointerTo(), nullptr, "module");
	modulePtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 1, modulePtr)->setAlignment(8);
	blockAST->addToScope("module", BuiltinTypes::LLVMModuleRef, new XXXValue(modulePtr));

	AllocaInst* functionPtr = builder->CreateAlloca(Types::LLVMOpaqueValue->getPointerTo(), nullptr, "function");
	functionPtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 2, functionPtr)->setAlignment(8);
	blockAST->addToScope("function", BuiltinTypes::LLVMValueRef, new XXXValue(functionPtr));

	AllocaInst* parentBlockPtr = builder->CreateAlloca(Types::BlockExprAST->getPointerTo(), nullptr, "parentBlock");
	parentBlockPtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 3, parentBlockPtr)->setAlignment(8);
	blockAST->addToScope("parentBlock", BuiltinTypes::BlockExprAST, new XXXValue(parentBlockPtr));

	Value* paramsVal = currentFunc->args().begin() + 4;
	paramsVal->setName("params");
	blockAST->setBlockParams(params, new XXXValue(paramsVal));

	closure = currentFunc->args().begin() + 5;
	closure->setName("closure");
}

void JitFunction::finalize()
{
	// Create implicit void-function return
	if (currentFunc->getReturnType()->isVoidTy())
		builder->CreateRetVoid();

/*std::vector<Type*> capturedTypes;
capturedTypes.reserve(capturedScope.size());
for (auto&& [name, var]: capturedScope)
{
	capturedTypes.push_back(var->type);
}
closureType->setBody(capturedTypes);*/

	closure = nullptr;
	XXXModule::finalize();
}

uint64_t JitFunction::compile()
{
	try {
		finalize();
//print(name + ".ll");
	}
	catch (CompileError err) {
#ifdef OUTPUT_JIT_CODE
		print("error.ll");
#endif
		throw;
	}

	// Compile module
	jitModuleKey = jit->addModule(std::move(module));
	auto jitFunctionSymbol = jit->findSymbol(name);
	uint64_t jitFunctionPointer = cantFail(jitFunctionSymbol.getAddress());

	return jitFunctionPointer;
}

void JitFunction::removeCompiledModule()
{
	if (jitModuleKey)
	{
		jit->removeModule(jitModuleKey);
		jitModuleKey = 0;
	}
}