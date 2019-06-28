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

// Local includes
#include "cparser.h"
#include "codegen.h"
#include "llvm_constants.h" //DELETE
#include "KaleidoscopeJIT.h"

using namespace llvm;

class FileModule;

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
KaleidoscopeJIT* jit;
BaseType BASE_TYPE;

// Current state
std::string currentSourcePath;
Module* currentModule;
Function *currentFunc;
BasicBlock *currentBB;
DIBuilder *dbuilder;
DIFile *dfile;

// Misc
DIBasicType* intType;
Value* closure;
BlockExprAST* fileBlock = nullptr;
int foo = 0;

void initBuiltinSymbols();
void defineBuiltinSymbols(BlockExprAST* block);

struct StaticStmtContext : public IStmtContext
{
private:
	StmtBlock cbk;
public:
	StaticStmtContext(StmtBlock cbk) : cbk(cbk) {}
	void codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		cbk(parentBlock, params);
	}
};
struct StaticExprContext : public IExprContext
{
private:
	ExprBlock cbk;
	BaseType* const type;
public:
	StaticExprContext(ExprBlock cbk, BaseType* type) : cbk(cbk), type(type) {}
	virtual Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params);
	}
	virtual BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};
struct StaticExprContext2 : public IExprContext
{
private:
	ExprBlock cbk;
	ExprTypeBlock typecbk;
public:
	StaticExprContext2(ExprBlock cbk, ExprTypeBlock typecbk) : cbk(cbk), typecbk(typecbk) {}
	virtual Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params);
	}
	virtual BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typecbk(parentBlock, params);
	}
};


struct DynamicStmtContext : public IStmtContext
{
private:
	typedef void (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params, void* closure);
	funcPtr cbk;
	void* closure;
public:
	DynamicStmtContext(uint64_t funcAddr, void* closure = nullptr) : cbk(reinterpret_cast<funcPtr>(funcAddr)), closure(closure) {}
	virtual void codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, params.data(), closure);
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
	}
};

struct DynamicExprContext : public IExprContext
{
private:
	typedef Value* (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params);
	funcPtr const cbk;
	BaseType* const type;
public:
	DynamicExprContext(uint64_t funcAddr, BaseType* type) : cbk(reinterpret_cast<funcPtr>(funcAddr)), type(type) {}
	virtual Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		Value* foo = cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, params.data());
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
		return Variable(type, new XXXValue(foo));
	}
	virtual BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};

extern "C"
{
	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope)
	{
		return expr->codegen(scope);
	}

	LLVMValueRef codegenExprValue(ExprAST* expr, BlockExprAST* scope)
	{
		return wrap(expr->codegen(scope).value->val);
	}

	void codegenStmt(StmtAST* stmt, BlockExprAST* scope)
	{
		stmt->codegen(scope);
	}

	BaseType* getType(ExprAST* expr, const BlockExprAST* scope)
	{
		return expr->getType(scope);
	}

	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params)
	{
		tplt->collectParams(scope, expr, params);
	}

	std::string ExprASTToString(const ExprAST* expr)
	{
		return expr->str();
	}

	std::string StmtASTToString(const StmtAST* stmt)
	{
		return stmt->str();
	}

	bool ExprASTIsId(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::ID;
	}
	bool ExprASTIsParam(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::PARAM;
	}
	bool ExprASTIsBlock(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::BLOCK;
	}

	const char* getIdExprASTName(const IdExprAST* expr)
	{
		return expr->name;
	}
	const char* getLiteralExprASTValue(const LiteralExprAST* expr)
	{
		return expr->value;
	}
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr)
	{
		return expr->parent;
	}
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent)
	{
		expr->parent = parent;
	}

	unsigned getExprLine(const ExprAST* expr)
	{
		return expr->loc.begin_line;
	}
	unsigned getExprColumn(const ExprAST* expr)
	{
		return expr->loc.begin_col;
	}

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, XXXValue* value)
	{
		scope->addToScope(name, type, value);
	}

	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, uint64_t funcAddr, void* closure)
	{
		scope->defineStatement(tplt, new DynamicStmtContext(funcAddr, closure));
	}

	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprListAST* tplt = tpltBlock->stmts->front()->exprs;
		scope->defineStatement(tplt, new StaticStmtContext(codeBlock));
	}

	void defineExpr(BlockExprAST* scope, ExprAST* tplt, uint64_t funcAddr, BaseType* type)
	{
		scope->defineExpr(tplt, new DynamicExprContext(funcAddr, type));
	}

	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->stmts->front()->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext(codeBlock, type));
	}

	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->stmts->front()->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext2(codeBlock, typeBlock));
	}

	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, uint64_t funcAddr)
	{
		scope->defineCast(fromType, toType, new DynamicExprContext(funcAddr, toType));
	}

	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock)
	{
		scope->defineCast(fromType, toType, new StaticExprContext(codeBlock, toType));
	}

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured)
	{
		return scope->lookupScope(name, isCaptured);
	}

	StmtAST* lookupStmt(const BlockExprAST* scope, const std::vector<ExprAST*>& exprs)
	{
		StmtAST* stmt = new StmtAST({0}, new ExprListAST('\0', exprs));
		if (!scope->lookupStatement(stmt))
		{
			delete stmt;
			return nullptr;
		}
		return stmt;
	}

	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType)
	{
		BaseType* fromType = expr->getType(scope);
		if (fromType == toType)
			return expr;

		IExprContext* castContext = scope->lookupCast(fromType, toType);
		if (castContext == nullptr)
			return nullptr;

		ExprAST* castExpr = new PlchldExprAST({0}, ""); //TODO: Create CastExprAST class
		castExpr->resolvedContext = castContext;
		castExpr->resolvedParams.push_back(expr);
		return castExpr;
	}

	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr)
	{
		std::string report = "";
		std::multimap<MatchScore, const std::pair<const ExprAST*, IExprContext*>&> candidates;
		std::vector<ExprAST*> resolvedParams;
		scope->lookupExprCandidates(expr, candidates);
		for (auto& candidate: candidates)
		{
			const MatchScore score = candidate.first;
			const std::pair<const ExprAST*, IExprContext*>& context = candidate.second;
			resolvedParams.clear();
			context.first->collectParams(scope, const_cast<ExprAST*>(expr), resolvedParams);
			BuiltinType* t = (BuiltinType*)context.second->getType(scope, resolvedParams);
			report += "\tcandidate(score=" + std::to_string(score) + "): " +  context.first->str() + "<" + (t ? t->name : "NULL") + ">\n";
		}
		return report;
	}

	BaseType* getBaseType()
	{
		return &BASE_TYPE;
	}

	void raiseCompileError(const char* msg, const ExprAST* loc)
	{
		throw CompileError(msg, loc->loc);
	}

	void importModule(BlockExprAST* scope, const char* path, const ExprAST* loc)
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
		IModule* importedModule = createModule(realPath, importedBlock, dbuilder != nullptr, scope);

		scope->import(importedBlock);

		//TODO: Free importedModule
	}

	BaseType* createFuncType(const char* name, bool isVarArg, BaseType* resultType, BaseType** argTypes, int numArgTypes)
	{
		std::vector<BuiltinType*> builtinArgTypes;
		for (int i = 0; i < numArgTypes; ++i)
			builtinArgTypes.push_back((BuiltinType*)argTypes[i]);
		return new FuncType(name, (BuiltinType*)resultType, builtinArgTypes, isVarArg);
	}

	BuiltinType* getPointerToBuiltinType(BuiltinType* type)
	{
		return type->Ptr();
	}

	void AddToScope(BlockExprAST* targetBlock, IdExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		targetBlock->addToScope(((IdExprAST*)nameAST)->name, type, new XXXValue(unwrap(val)));
	}

	void AddToFileScope(ExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		fileBlock->addToScope(((IdExprAST*)nameAST)->name, type, new XXXValue(unwrap(val)));
	}

	void DefineStatement(BlockExprAST* targetBlock, ExprAST** params, int numParams, uint64_t funcPtr, void* closure)
	{
		targetBlock->defineStatement(std::vector<ExprAST*>(params, params + numParams), new DynamicStmtContext(funcPtr, closure));
	}

	Value* getValueFunction(XXXValue* value)
	{
		return value->getFunction(currentModule);
	}
}

void init()
{
	context = unwrap(LLVMGetGlobalContext());//new LLVMContext();
	builder = new IRBuilder<>(*context);

	// Initialize target registry etc.
InitializeNativeTarget();
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	// Create JIT
	jit = new KaleidoscopeJIT();

	// Declare types
	Types::create(*context);

	// Initialize builtin symbols
	initBuiltinSymbols();
}

class XXXModule : public IModule
{
protected:
	Module* const prevModule;
	DIBuilder* const prevDbuilder;
	DIFile* const prevDfile;
	Function* const prevFunc;
	BasicBlock* const prevBB;
	const Location loc;

	std::unique_ptr<Module> module;
	legacy::FunctionPassManager* jitPassManager;

public:
	XXXModule(const std::string& moduleName, const Location& loc, bool outputDebugSymbols, bool optimizeCode)
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

	virtual void finalize()
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

	void print(const std::string& outputPath)
	{
		std::error_code ec;
		raw_fd_ostream ostream(outputPath, ec);
		module->print(ostream, nullptr);
		ostream.close();
	}

	void print()
	{
		module->print(outs(), nullptr);
	}

	bool compile(const std::string& outputPath, std::string& errstr)
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

	void run()
	{
		assert(0);
	}
};

class FileModule : public XXXModule
{
private:
	const std::string prevSourcePath;
	Function* mainFunc;

public:
	FileModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, bool optimizeCode)
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

	void finalize()
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

	void run()
	{
		ExecutionEngine* EE = EngineBuilder(std::unique_ptr<Module>(module.get())).create();
		int result = EE->runFunctionAsMain(mainFunc, {}, nullptr);
		outs() << "./minc Result: " << result << "\n";
	}
};

class JitFunction : public XXXModule
{
private:
	llvm::orc::VModuleKey jitModuleKey;

public:
	StructType* closureType;

	JitFunction(BlockExprAST* parentBlock, BlockExprAST* blockAST, Type *returnType, std::vector<ExprAST*>& params)
		: XXXModule("module", blockAST->loc, ENABLE_JIT_CODE_DEBUG_SYMBOLS, OPTIMIZE_JIT_CODE), jitModuleKey(0), closureType(StructType::create(*context, "closureType"))
	{
		closureType->setBody(ArrayRef<Type*>());

//capturedScope.clear();

		// Create function
		std::vector<Type*> argTypes;
		argTypes.push_back(Types::LLVMOpaqueBuilder->getPointerTo());
		argTypes.push_back(Types::LLVMOpaqueModule->getPointerTo());
		argTypes.push_back(Types::LLVMOpaqueValue->getPointerTo());
		argTypes.push_back(Types::BlockExprAST->getPointerTo());
		argTypes.push_back(Types::ExprAST->getPointerTo()->getPointerTo());
		argTypes.push_back(closureType->getPointerTo());
		FunctionType* funcType = FunctionType::get(returnType, argTypes, false);
		Function* jitFunction = currentFunc = Function::Create(funcType, Function::ExternalLinkage, "jitFunction", module.get());

		if (dbuilder)
		{
			unsigned ScopeLine = blockAST->loc.begin_line;
			DISubprogram *SP = dbuilder->createFunction(
				dfile, "jitFunction", StringRef(), dfile, loc.begin_line,
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

	void finalize()
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

	uint64_t compile(const std::string& outputPath)
	{
		try {
			finalize();
		}
		catch (CompileError err) {
#ifdef OUTPUT_JIT_CODE
			if (outputPath[0] != '\0')
				print(outputPath);
#endif
			throw;
		}

		// Compile module
		jitModuleKey = jit->addModule(std::move(module));
		auto jitFunctionSymbol = jit->findSymbol("jitFunction");
		uint64_t jitFunctionPointer = cantFail(jitFunctionSymbol.getAddress());

		return jitFunctionPointer;
	}

	void removeCompiledModule()
	{
		if (jitModuleKey)
		{
			jit->removeModule(jitModuleKey);
			jitModuleKey = 0;
		}
	}
};

extern "C"
{
	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params)
	{
		return new JitFunction(scope, blockAST, unwrap(((BuiltinType*)returnType)->llvmtype), params);
	}

	uint64_t compileJitFunction(JitFunction* jitFunc, const char* outputPath)
	{
		return jitFunc->compile(outputPath);
	}

	void removeJitFunctionModule(JitFunction* jitFunc)
	{
		jitFunc->removeCompiledModule();
	}

	void removeJitFunction(JitFunction* jitFunc)
	{
		delete jitFunc;
	}
}

IModule* createModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, BlockExprAST* parentBlock)
{
	FileModule* module = new FileModule(sourcePath, moduleBlock, outputDebugSymbols, !outputDebugSymbols);
	defineBuiltinSymbols(moduleBlock);
	moduleBlock->codegen(parentBlock);
	module->finalize();
	return module;
}

Variable BlockExprAST::codegen(BlockExprAST* parentBlock)
{
	parent = parentBlock;

	if (fileBlock == nullptr)
		fileBlock = this;

	for (auto stmt: *stmts)
		stmt->codegen(this);

	if (dbuilder)
		builder->SetCurrentDebugLocation(DebugLoc());

	if (fileBlock == this)
		fileBlock = nullptr;

	//parent = nullptr;
	return Variable(nullptr, new XXXValue(Constant::getNullValue(Type::getVoidTy(*context)->getPointerTo())));
}

Variable ExprAST::codegen(BlockExprAST* parentBlock)
{
	if (!resolvedContext)
		parentBlock->lookupExpr(this);

	if (resolvedContext)
	{
		if (dbuilder)
			builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		const Variable var = resolvedContext->codegen(parentBlock, resolvedParams);
		const BuiltinType *expectedType = (BuiltinType*)resolvedContext->getType(parentBlock, resolvedParams), *gotType = (BuiltinType*)var.type;
		if (expectedType != gotType)
		{
			std::string expectedTypeStr = expectedType == nullptr ? "NULL" : expectedType->name, gotTypeStr = gotType == nullptr ? "NULL" : gotType->name;
			throw CompileError(
				("invalid expression return type: " + ExprASTToString(this) + "<" + gotTypeStr + ">, expected: <" + expectedTypeStr + ">").c_str(),
				this->loc
			);
		}
		return var;
	}
	else
		throw UndefinedExprException{this};
}

/*void StmtAST::codegen(BlockExprAST* parentBlock)
{
	exprs->resolveTypes(parentBlock);

	std::vector<std::pair<ExprAST*, IExprContext*>> casts;
	if (parentBlock->lookupStatement(this, casts))
	{
		if (dbuilder)
			builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));

		for (auto cast: casts)
			cast.second->codegen(parentBlock, cast.first->resolvedParams);

		resolvedContext->codegen(parentBlock, resolvedParams);
	}
	else
		throw UndefinedStmtException{this};
}*/
void StmtAST::codegen(BlockExprAST* parentBlock)
{
	exprs->resolveTypes(parentBlock);

	if (parentBlock->lookupStatement(this))
	{
		if (dbuilder)
			builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		resolvedContext->codegen(parentBlock, resolvedParams);
	}
	else
		throw UndefinedStmtException{this};
}

void ExprAST::resolveTypes(BlockExprAST* block)
{
	block->lookupExpr(this);
}

BaseType* PlchldExprAST::getType(const BlockExprAST* parentBlock) const
{
	switch(p1)
	{
	case 'E': return nullptr;
	case 'L': return BuiltinTypes::LiteralExprAST;
	case 'I': return BuiltinTypes::IdExprAST;
	case 'B': return BuiltinTypes::BlockExprAST;
	dafault: assert(0); return nullptr; //TODO: Throw exception
	case '\0':
		{
			const Variable* var = parentBlock->lookupScope(p2);
			if (var == nullptr)
				throw UndefinedIdentifierException(new IdExprAST(loc, p2));
			return (BaseType*)var->value->getConstantValue();
		}
	}
}

Variable ParamExprAST::codegen(BlockExprAST* parentBlock)
{
	Value* params = parentBlock->getBlockParamsVal()->val;

	Value* idxVal = dynamicIdx ?
		dynamicIdx->codegen(parentBlock).value->val :
		Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(64, staticIdx, true))
	;

	// param = params[idxVal]
	Value* gep = builder->CreateInBoundsGEP(params, { idxVal });
	LoadInst* param = builder->CreateLoad(gep);
	param->setAlignment(8);

	if (!dynamicIdx)
	{
		std::vector<ExprAST*>* blockParams = parentBlock->getBlockParams();
		if (blockParams != nullptr && staticIdx < blockParams->size())
		{
			ExprAST* blockParamExpr = blockParams->at(staticIdx);
			if (blockParamExpr->exprtype == ExprAST::ExprType::PLCHLD)
			{
				const Variable* var;
				switch (((PlchldExprAST*)blockParamExpr)->p1)
				{
				case 'L': return Variable(BuiltinTypes::LiteralExprAST, new XXXValue(builder->CreateBitCast(param, Types::LiteralExprAST->getPointerTo())));
				case 'I': return Variable(BuiltinTypes::IdExprAST, new XXXValue(builder->CreateBitCast(param, Types::IdExprAST->getPointerTo())));
				case 'B': return Variable(BuiltinTypes::BlockExprAST, new XXXValue(builder->CreateBitCast(param, Types::BlockExprAST->getPointerTo())));
				/*case '\0':
					var = parentBlock->lookupScope(((PlchldExprAST*)blockParamExpr)->p2);
					if (var != nullptr)
						return Variable(var->type, new XXXValue(builder->CreateBitCast(param, var->value->type)));*/
				}
			}
			else
				assert(0); //TODO: In what scenarios do we hit this? How should it be handled?
		}
	}

	return Variable(BuiltinTypes::ExprAST, new XXXValue(param));
}

BaseType* ParamExprAST::getType(const BlockExprAST* parentBlock) const
{
	/*if (!dynamicIdx)
	{
		std::vector<ExprAST*>* blockParams = parentBlock->getBlockParams();
		if (blockParams != nullptr && staticIdx < blockParams->size())
			return blockParams->at(staticIdx)->getType(parentBlock);
	}
	return nullptr;*/

	if (!dynamicIdx)
	{
		std::vector<ExprAST*>* blockParams = parentBlock->getBlockParams();
		if (blockParams != nullptr && staticIdx < blockParams->size())
		{
			ExprAST* blockParamExpr = blockParams->at(staticIdx);
			if (blockParamExpr->exprtype == ExprAST::ExprType::PLCHLD)
			{
				//return blockParamExpr->getType(parentBlock);
				/*switch (((PlchldExprAST*)blockParamExpr)->p1)
				{
				case 'L': return BuiltinTypes::LiteralExprAST;
				case 'I': return BuiltinTypes::IdExprAST;
				case 'B': return BuiltinTypes::BlockExprAST;
				}*/
				switch(((PlchldExprAST*)blockParamExpr)->p1)
				{
				case 'L': return BuiltinTypes::LiteralExprAST;
				case 'I': return BuiltinTypes::IdExprAST;
				case 'B': return BuiltinTypes::BlockExprAST;
				case '\0':
					{
						BaseType* codegenType = parentBlock->lookupScope(((PlchldExprAST*)blockParamExpr)->p2)->type;
						//TODO // Store codegenType as a variable inside the BuiltinTypes::ExprAST struct
						//TODO // Inside codegen: Build type cast to codegenType
						int abc = 0;
					}
				}
			}
			else
				assert(0); //TODO: In what scenarios do we hit this? How should it be handled?
		}
	}
	return BuiltinTypes::ExprAST;
}