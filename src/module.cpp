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
#include "llvm_constants.h"
#include "KaleidoscopeJIT.h"
#include "module.h"
#include "cparser.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern Function* currentFunc;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;
extern DIFile* dfile;
extern Value* closure;

// Singletons
LLVMContext* context;
IRBuilder<> *builder;
Module* currentModule;
Function *currentFunc;
BasicBlock *currentBB;
DIBuilder *dbuilder;
DIFile *dfile;

// Misc
KaleidoscopeJIT* jit;
std::string currentSourcePath;
DIBasicType* intType;
Value* closure;

struct DynamicStmtContext : public CodegenContext
{
private:
	typedef void (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, LLVMMetadataRef, ExprAST** params, void* stmtArgs);
	funcPtr cbk;
	void* stmtArgs;
public:
	DynamicStmtContext(JitFunction* func, void* stmtArgs = nullptr) : cbk(reinterpret_cast<funcPtr>(func->compile())), stmtArgs(stmtArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, wrap(dfile), params.data(), stmtArgs);
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
		return getVoid();
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return nullptr;
	}
};

struct DynamicExprContext : public CodegenContext
{
private:
	typedef Value* (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, LLVMMetadataRef, ExprAST** params);
	funcPtr const cbk;
	BaseType* const type;
public:
	DynamicExprContext(JitFunction* func, BaseType* type) : cbk(reinterpret_cast<funcPtr>(func->compile())), type(type) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		Value* foo = cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, wrap(dfile), params.data());
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
		return Variable(type, new XXXValue(foo));
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};

struct DynamicExprContext2 : public CodegenContext
{
private:
	typedef Value* (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, LLVMMetadataRef, ExprAST** params);
	typedef BaseType* (*typeFuncPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, const BlockExprAST* parentBlock, LLVMMetadataRef, ExprAST*const* params);
	typeFuncPtr const typeCbk;
	funcPtr const cbk;
	// Keep typeCbk before cbk, so that typeFunc->compile() is called before func->compile().
	// This is currently necessary to ensure JitFunction's are finalized in opposit order in which they were created.
	// Otherwise, the line `builder->SetInsertPoint(currentBB = prevBB);` would not recover prevBB.
public:
	DynamicExprContext2(JitFunction* func, JitFunction* typeFunc) : typeCbk(reinterpret_cast<typeFuncPtr>(typeFunc->compile())), cbk(reinterpret_cast<funcPtr>(func->compile())) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		Value* foo = cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, wrap(dfile), params.data());
		BaseType* type = typeCbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, wrap(dfile), params.data());
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
		return Variable(type, new XXXValue(foo));
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typeCbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, wrap(dfile), params.data());
	}
};

extern "C"
{
	void initCompiler()
	{
		context = unwrap(LLVMGetGlobalContext());//new LLVMContext();
		builder = new IRBuilder<>(*context);

		JitFunction::init();

		registerStepEventListener([](const ExprAST* loc) {
			if (dbuilder)
				builder->SetCurrentDebugLocation(
					loc == nullptr
					? DebugLoc()
					: DebugLoc::get(getExprLine(loc), getExprColumn(loc), currentFunc->getSubprogram())
				);
		});
	}

	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, JitFunction* func, void* stmtArgs)
	{
		if (tplt.empty())
			assert(0); //TODO: throw CompileError("error parsing template " + std::string(tplt.str()), tplt.loc);
		if (tplt.back()->exprtype != ExprAST::ExprType::PLCHLD || ((PlchldExprAST*)tplt.back())->p1 != 'B')
		{
			std::vector<ExprAST*> stoppedTplt(tplt);
			stoppedTplt.push_back(new StopExprAST(Location{}));
			scope->defineStatement(stoppedTplt, new DynamicStmtContext(func, stmtArgs));
		}
		else
			scope->defineStatement(tplt, new DynamicStmtContext(func, stmtArgs));
	}

	/*void DefineStatement(BlockExprAST* targetBlock, ExprAST** params, int numParams, JitFunction* func, void* closure)
	{
		targetBlock->defineStatement(std::vector<ExprAST*>(params, params + numParams), new DynamicStmtContext(func, closure));
	}*/

	void defineExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type)
	{
		scope->defineExpr(tplt, new DynamicExprContext(func, type));
	}

	void defineExpr4(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, JitFunction* typeFunc)
	{
		scope->defineExpr(tplt, new DynamicExprContext2(func, typeFunc));
	}

	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func)
	{
		scope->defineCast(fromType, toType, new DynamicExprContext(func, toType));
	}

	IModule* createModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols)
	{
		// Unbind parseCFile filename parameter lifetime from local filename parameter
		char* path = new char[sourcePath.size() + 1];
		strcpy(path, sourcePath.c_str());

		return new FileModule(path, moduleBlock, outputDebugSymbols, !outputDebugSymbols);
	}

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params, std::string& name)
	{
		return new JitFunction(scope, blockAST, unwrap(((BuiltinType*)returnType)->llvmtype), params, name);
	}

	uint64_t compileJitFunction(JitFunction* jitFunc)
	{
		return jitFunc->compile();
	}

	void removeJitFunctionModule(JitFunction* jitFunc)
	{
		jitFunc->removeCompiledModule();
	}

	void removeJitFunction(JitFunction* jitFunc)
	{
		delete jitFunc;
	}

	void importModule(BlockExprAST* scope, const char* path, const ExprAST* loc, BaseScopeType* fileScope)
	{
		std::ifstream file(path);
		if (!file.good())
			throw CompileError(std::string(path) + ": No such file or directory\n", loc->loc);

		//TODO: Cache imported symbols, statements and expressions, instead of ignoring already imported files
		char buf[1024];
		realpath(path, buf);
		char* realPath = new char[strlen(buf) + 1];
		strcpy(realPath, buf);
		static std::set<std::string> importedPaths;
		if (importedPaths.find(realPath) != importedPaths.end()) return;
		importedPaths.insert(realPath);

		// Parse imported file
		CLexer lexer(file, std::cout);
		BlockExprAST* importedBlock;
		yy::CParser parser(lexer, realPath, &importedBlock);
		if (parser.parse())
			throw CompileError("error parsing file " + std::string(path), loc->loc);

		// Generate module from parsed file
		setScopeType(importedBlock, fileScope);
		FileModule* importedModule = new FileModule(realPath, importedBlock, dbuilder != nullptr, dbuilder == nullptr);
		importedBlock->codegen(scope);
		importedModule->finalize();

		scope->import(importedBlock);

		//TODO: Free importedModule
	}
}

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
		jitFunctionPassManager = new legacy::FunctionPassManager(module.get());
		jitModulePassManager = new legacy::PassManager();
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
		jitPassManagerBuilder.populateFunctionPassManager(*jitFunctionPassManager);
		jitPassManagerBuilder.populateModulePassManager(*jitModulePassManager);
		jitFunctionPassManager->doInitialization();
	}
	else
	{
		jitFunctionPassManager = nullptr;
		jitModulePassManager = nullptr;
	}
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

	if (jitFunctionPassManager && !haserr)
	{
		jitFunctionPassManager->run(*currentFunc);
		jitFunctionPassManager->doFinalization();
	}

	if (jitModulePassManager && !haserr)
		jitModulePassManager->run(*currentModule);

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

FileModule::FileModule(const char* sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, bool optimizeCode)
	: XXXModule(sourcePath == "-" ? "main" : sourcePath, { sourcePath, 1, 1, 1, 1 }, outputDebugSymbols, optimizeCode), prevSourcePath(currentSourcePath = sourcePath)
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
	// Initialize target registry etc.
InitializeNativeTarget();
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

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
		Types::LLVMOpaqueMetadata->getPointerTo(),
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
	blockAST->defineSymbol("builder", BuiltinTypes::LLVMBuilderRef, new XXXValue(builderPtr));

	AllocaInst* modulePtr = builder->CreateAlloca(Types::LLVMOpaqueModule->getPointerTo(), nullptr, "module");
	modulePtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 1, modulePtr)->setAlignment(8);
	blockAST->defineSymbol("module", BuiltinTypes::LLVMModuleRef, new XXXValue(modulePtr));

	AllocaInst* functionPtr = builder->CreateAlloca(Types::LLVMOpaqueValue->getPointerTo(), nullptr, "function");
	functionPtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 2, functionPtr)->setAlignment(8);
	blockAST->defineSymbol("function", BuiltinTypes::LLVMValueRef, new XXXValue(functionPtr));

	AllocaInst* parentBlockPtr = builder->CreateAlloca(Types::BlockExprAST->getPointerTo(), nullptr, "parentBlock");
	parentBlockPtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 3, parentBlockPtr)->setAlignment(8);
	blockAST->defineSymbol("parentBlock", BuiltinTypes::BlockExprAST, new XXXValue(parentBlockPtr));

	AllocaInst* dfilePtr = builder->CreateAlloca(Types::LLVMOpaqueMetadata->getPointerTo(), nullptr, "dfile");
	dfilePtr->setAlignment(8);
	builder->CreateStore(currentFunc->args().begin() + 4, dfilePtr)->setAlignment(8);
	blockAST->defineSymbol("dfile", BuiltinTypes::LLVMMetadataRef, new XXXValue(dfilePtr));

	Value* paramsVal = currentFunc->args().begin() + 5;
	paramsVal->setName("params");
	blockAST->blockParams.clear();
	int i = 0;
	for (ExprAST* blockParamExpr: params)
	{
		Value* gep = builder->CreateInBoundsGEP(paramsVal, { Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(64, i++, true)) });
		LoadInst* param = builder->CreateLoad(gep);
		param->setAlignment(8);

		Variable paramVar = Variable(BuiltinTypes::ExprAST, new XXXValue(param));
		if (blockParamExpr->exprtype == ExprAST::ExprType::PLCHLD)
		{
			PlchldExprAST* blockParamPlchldExpr = (PlchldExprAST*)blockParamExpr;
			switch(blockParamPlchldExpr->p1)
			{
			default: assert(0); //TODO: Throw exception
			case 'L': paramVar = Variable(BuiltinTypes::LiteralExprAST, new XXXValue(builder->CreateBitCast(param, Types::LiteralExprAST->getPointerTo()))); break;
			case 'I': paramVar = Variable(BuiltinTypes::IdExprAST, new XXXValue(builder->CreateBitCast(param, Types::IdExprAST->getPointerTo()))); break;
			case 'B': paramVar = Variable(BuiltinTypes::BlockExprAST, new XXXValue(builder->CreateBitCast(param, Types::BlockExprAST->getPointerTo()))); break;
			case 'S': break;
			case 'E':
				if (blockParamPlchldExpr->p2 == nullptr)
					break;
				if (const Variable* var = parentBlock->importSymbol(blockParamPlchldExpr->p2))
				{
					BaseType* codegenType = (BaseType*)var->value->getConstantValue();
					paramVar = Variable(TpltType::get("ExprAST<" + std::string(blockParamPlchldExpr->p2) + ">", BuiltinTypes::ExprAST, codegenType), new XXXValue(param));
					break;
				}
			}
		}
		else if (blockParamExpr->exprtype == ExprAST::ExprType::LIST)
		{
			ExprListAST* blockParamListExpr = (ExprListAST*)blockParamExpr;
			assert(blockParamListExpr->exprs.size());

			BuiltinType* exprType = BuiltinTypes::ExprAST;
			PlchldExprAST* blockParamPlchldExpr = (PlchldExprAST*)blockParamListExpr->exprs.front();
			switch(blockParamPlchldExpr->p1)
			{
			default: assert(0); //TODO: Throw exception
			case 'L': exprType = BuiltinTypes::LiteralExprAST; break;
			case 'I': exprType = BuiltinTypes::IdExprAST; break;
			case 'B': exprType = BuiltinTypes::BlockExprAST; break;
			case 'S': break;
			case 'E':
				if (blockParamPlchldExpr->p2 == nullptr)
					break;
				if (const Variable* var = parentBlock->importSymbol(blockParamPlchldExpr->p2))
				{
					BaseType* codegenType = (BaseType*)var->value->getConstantValue();
					exprType = TpltType::get("ExprAST<" + std::string(blockParamPlchldExpr->p2) + ">", BuiltinTypes::ExprAST, codegenType);
					break;
				}
			}

			paramVar = Variable(
				TpltType::get("ExprListAST<" + getTypeName(exprType) + ">", BuiltinTypes::ExprListAST, exprType),
				new XXXValue(builder->CreateBitCast(param, Types::ExprListAST->getPointerTo()))
			);
		}
		else if (blockParamExpr->exprtype == ExprAST::ExprType::ELLIPSIS)
		{
			paramVar = Variable(BuiltinTypes::ExprAST, new XXXValue(Constant::getNullValue((Types::ExprAST->getPointerTo())))); break;
			continue;
		}
		else
			assert(0);

		blockAST->blockParams.push_back(paramVar);
	}

	closure = currentFunc->args().begin() + 6;
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

/*for(GlobalVariable& g: module->globals())
{
new GlobalVariable(
	*currentModule,
	g.getType(),
	g.isConstant(),
	GlobalValue::PrivateLinkage,
	nullptr,
	"",
	nullptr,
	GlobalVariable::NotThreadLocal,
	0
);
}*/

	// Compile module
	jitModuleKey = jit->addModule(std::move(module));
	auto jitFunctionSymbol = jit->findSymbol(name);
//auto foo = cantFail(jit->findSymbol("MY_CONSTANT").getAddress());
//TODO: Implement https://lists.llvm.org/pipermail/llvm-dev/2011-May/040236.html -> possibility 2
	auto jitFunctionPointer = jitFunctionSymbol.getAddress();
	if (jitFunctionPointer)
		return *jitFunctionPointer;
	else
		throw CompileError("error linking JIT function\n" + toString(jitFunctionPointer.takeError()), loc);
}

void JitFunction::removeCompiledModule()
{
	if (jitModuleKey)
	{
		jit->removeModule(jitModuleKey);
		jitModuleKey = 0;
	}
}