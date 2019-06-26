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
#include <llvm/IR/LegacyPassManager.h>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

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
std::list<Func> llvm_c_functions;
BlockExprAST* fileBlock = nullptr;
int foo = 0;

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

	void _LLVMPositionBuilder(LLVMBasicBlockRef bb)
	{
		builder->SetInsertPoint(currentBB = unwrap(bb));
	}
	LLVMValueRef _LLVMBuildInBoundsGEP1(LLVMValueRef Pointer, LLVMValueRef Idx0, const char *Name)
	{
		return LLVMBuildInBoundsGEP(wrap(builder), Pointer, &Idx0, 1, Name);
	}
	LLVMValueRef _LLVMBuildInBoundsGEP2(LLVMValueRef Pointer, LLVMValueRef Idx0, LLVMValueRef Idx1, const char *Name)
	{
		LLVMValueRef Idxs[] = { Idx0, Idx1 };
		return LLVMBuildInBoundsGEP(wrap(builder), Pointer, Idxs, 2, Name);
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


	// >>> Create builtin types

	BuiltinTypes::Builtin = new BuiltinType("builtinType", wrap(Types::BuiltinType), 8);

	// Primitive types
	BuiltinTypes::Void = new BuiltinType("void", LLVMVoidType(), 0);
	BuiltinTypes::VoidPtr = BuiltinTypes::Void->Ptr();
	BuiltinTypes::Int1 = new BuiltinType("bool", wrap(Types::Int1), 1);
	BuiltinTypes::Int1Ptr =BuiltinTypes::Int1->Ptr();
	BuiltinTypes::Int8 = new BuiltinType("char", wrap(Types::Int8), 1);
	BuiltinTypes::Int8Ptr = new BuiltinType("string", wrap(Types::Int8Ptr), 8);
	BuiltinTypes::Int16 = new BuiltinType("short", wrap(Types::Int16), 2);
	BuiltinTypes::Int16Ptr = BuiltinTypes::Int16->Ptr();
	BuiltinTypes::Int32 = new BuiltinType("int", LLVMInt32Type(), 4);
	BuiltinTypes::Int32Ptr = BuiltinTypes::Int32->Ptr();
	BuiltinTypes::Int64 = new BuiltinType("long", wrap(Types::Int64), 8);
	BuiltinTypes::Int64Ptr = BuiltinTypes::Int64->Ptr();
	BuiltinTypes::Half = new BuiltinType("half", LLVMHalfType(), 2);
	BuiltinTypes::HalfPtr = BuiltinTypes::Half->Ptr();
	BuiltinTypes::Float = new BuiltinType("float", LLVMFloatType(), 4);
	BuiltinTypes::FloatPtr = BuiltinTypes::Float->Ptr();
	BuiltinTypes::Double = new BuiltinType("double", LLVMDoubleType(), 8);
	BuiltinTypes::DoublePtr = BuiltinTypes::Double->Ptr();

	// LLVM types
	BuiltinTypes::LLVMAttributeRef = new BuiltinType("LLVMAttributeRef", wrap(Types::LLVMOpaqueAttributeRef->getPointerTo()), 8);
	BuiltinTypes::LLVMBasicBlockRef = new BuiltinType("LLVMBasicBlockRef", wrap(Types::LLVMOpaqueBasicBlock->getPointerTo()), 8);
	BuiltinTypes::LLVMBuilderRef = new BuiltinType("LLVMBuilderRef", wrap(Types::LLVMOpaqueBuilder->getPointerTo()), 8);
	BuiltinTypes::LLVMContextRef = new BuiltinType("LLVMContextRef", wrap(Types::LLVMOpaqueContext->getPointerTo()), 8);
	BuiltinTypes::LLVMDiagnosticInfoRef = new BuiltinType("LLVMDiagnosticInfoRef", wrap(Types::LLVMOpaqueDiagnosticInfo->getPointerTo()), 8);
	BuiltinTypes::LLVMDIBuilderRef = new BuiltinType("LLVMDIBuilderRef", wrap(Types::LLVMOpaqueDIBuilder->getPointerTo()), 8);
	BuiltinTypes::LLVMMemoryBufferRef = new BuiltinType("LLVMMemoryBufferRef", wrap(Types::LLVMOpaqueMemoryBuffer->getPointerTo()), 8);
	BuiltinTypes::LLVMMetadataRef = new BuiltinType("LLVMMetadataRef", wrap(Types::LLVMOpaqueMetadata->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleRef = new BuiltinType("LLVMModuleRef", wrap(Types::LLVMOpaqueModule->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleFlagEntryRef = new BuiltinType("LLVMModuleFlagEntryRef", wrap(Types::LLVMOpaqueModuleFlagEntry->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleProviderRef = new BuiltinType("LLVMModuleProviderRef", wrap(Types::LLVMOpaqueModuleProvider->getPointerTo()), 8);
	BuiltinTypes::LLVMNamedMDNodeRef = new BuiltinType("LLVMNamedMDNodeRef", wrap(Types::LLVMOpaqueNamedMDNode->getPointerTo()), 8);
	BuiltinTypes::LLVMPassManagerRef = new BuiltinType("LLVMPassManagerRef", wrap(Types::LLVMOpaquePassManager->getPointerTo()), 8);
	BuiltinTypes::LLVMPassRegistryRef = new BuiltinType("LLVMPassRegistryRef", wrap(Types::LLVMOpaquePassRegistry->getPointerTo()), 8);
	BuiltinTypes::LLVMTypeRef = new BuiltinType("LLVMTypeRef", wrap(Types::LLVMOpaqueType->getPointerTo()), 8);
	BuiltinTypes::LLVMUseRef = new BuiltinType("LLVMUseRef", wrap(Types::LLVMOpaqueUse->getPointerTo()), 8);
	BuiltinTypes::LLVMValueRef = new BuiltinType("LLVMValueRef", wrap(Types::LLVMOpaqueValue->getPointerTo()), 8);
	BuiltinTypes::LLVMValueMetadataEntryRef = new BuiltinType("LLVMValueRef", wrap(Types::LLVMOpaqueValueMetadataEntry->getPointerTo()), 8);

	// AST types
	BuiltinTypes::ExprAST = new BuiltinType("ExprAST", wrap(Types::ExprAST->getPointerTo()), 8);
	BuiltinTypes::LiteralExprAST = new BuiltinType("LiteralExprAST", wrap(Types::LiteralExprAST->getPointerTo()), 8);
	BuiltinTypes::IdExprAST = new BuiltinType("IdExprAST", wrap(Types::IdExprAST->getPointerTo()), 8);
	BuiltinTypes::BlockExprAST = new BuiltinType("BlockExprAST", wrap(Types::BlockExprAST->getPointerTo()), 8);

	// Misc. types
	BuiltinTypes::Function = new BuiltinType("func", nullptr, 8);

	/*printf("builtinType ...       0x%lx\n", (uint64_t)BuiltinTypes::Builtin);
	printf("int ...               0x%lx\n", (uint64_t)BuiltinTypes::Int32);
	printf("double ...            0x%lx\n", (uint64_t)BuiltinTypes::Double);
	printf("string ...            0x%lx\n", (uint64_t)BuiltinTypes::Int8Ptr);
	printf("func ...              0x%lx\n", (uint64_t)BuiltinTypes::Function);
	printf("ExprAST ...           0x%lx\n", (uint64_t)BuiltinTypes::ExprAST);
	printf("LiteralExprAST ...    0x%lx\n", (uint64_t)BuiltinTypes::LiteralExprAST);
	printf("IdExprAST ...         0x%lx\n", (uint64_t)BuiltinTypes::IdExprAST);
	printf("BlockExprAST ...      0x%lx\n", (uint64_t)BuiltinTypes::BlockExprAST);
	printf("LLVMValueRef ...      0x%lx\n", (uint64_t)BuiltinTypes::LLVMValueRef);
	printf("LLVMBasicBlockRef ... 0x%lx\n", (uint64_t)BuiltinTypes::LLVMBasicBlockRef);
	printf("LLVMTypeRef ...       0x%lx\n", (uint64_t)BuiltinTypes::LLVMTypeRef);*/


	// Create LLVM-c extern functions
	create_llvm_c_functions(*context, llvm_c_functions);
}

class XXXModule : public IModule
{
protected:
	Module* const prevModule;
	DIBuilder* const prevDbuilder;
	DIFile* const prevDfile;
	Function* const prevFunc;
	BasicBlock* const prevBB;
	const std::string filename;

	std::unique_ptr<Module> module;
	legacy::FunctionPassManager* jitPassManager;

public:
	XXXModule(const std::string& moduleName, const std::string& sourcePath, bool outputDebugSymbols, bool optimizeCode)
		: prevModule(currentModule), prevDbuilder(dbuilder), prevDfile(dfile), prevFunc(currentFunc), prevBB(currentBB), filename(sourcePath)
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
			if (sourcePath != "-")
			{
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
			jitPassManager->add(createInstructionCombiningPass());
			jitPassManager->add(createReassociatePass());
			jitPassManager->add(createGVNPass());
			jitPassManager->add(createCFGSimplificationPass());
			jitPassManager->doInitialization();
		}
		else
			jitPassManager = nullptr;
	}

	virtual void finalize()
	{
		if (currentBB != prevBB)
			builder->SetInsertPoint(currentBB = prevBB);
		currentFunc = prevFunc; // Switch back to parent function
		currentModule = prevModule; // Switch back to file module
		dbuilder = prevDbuilder; // Reenable debug symbol generation
		dfile = prevDfile;
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

void createBuiltinStatements(BlockExprAST* block);
class FileModule : public XXXModule
{
private:
	const std::string prevSourcePath;
	Function* mainFunc;

public:
	FileModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, bool optimizeCode)
		: XXXModule(sourcePath == "-" ? "main" : sourcePath, sourcePath, outputDebugSymbols, optimizeCode), prevSourcePath(currentSourcePath = sourcePath)
	{
for (Func& func: llvm_c_functions)
{
	moduleBlock->addToScope(func.type.name, &func.type, &func);
}

		// Generate main function
		FunctionType *mainType = FunctionType::get(Types::Int32, {}, false);
		mainFunc = currentFunc = Function::Create(mainType, Function::ExternalLinkage, "main", currentModule);
		mainFunc->setDSOLocal(true);
		mainFunc->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::NoInline);
		mainFunc->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::OptimizeNone);

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

		// Close main function
		if (dbuilder)
		{
			dbuilder->finalizeSubprogram(mainFunc->getSubprogram());
			builder->SetCurrentDebugLocation(DebugLoc());
		}
		std::string errstr;
		raw_string_ostream errstream(errstr);
		bool haserr = verifyFunction(*mainFunc, &errstream);
		if (haserr && errstr[0] != '\0')
			throw CompileError("error compiling module\n" + errstr, { filename.c_str(), 1, 1, 1, 1 });

		// Close module
		if (dbuilder)
			dbuilder->finalize();

		currentSourcePath = prevSourcePath;
		XXXModule::finalize();
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
	Location loc;

	llvm::orc::VModuleKey jitModuleKey;

public:
	StructType* closureType;

	JitFunction(BlockExprAST* parentBlock, BlockExprAST* blockAST, Type *returnType, std::vector<ExprAST*>& params)
		: XXXModule("module", currentSourcePath, ENABLE_JIT_CODE_DEBUG_SYMBOLS, OPTIMIZE_JIT_CODE), loc(blockAST->loc), jitModuleKey(0), closureType(StructType::create(*context, "closureType"))
	{
		closureType->setBody(ArrayRef<Type*>());

//capturedScope.clear();

for (Func& func: llvm_c_functions)
{
	blockAST->addToScope(func.type.name, &func.type, &func);
}
Func* LLVMPositionBuilderFunc = new Func("LLVMPositionBuilder", BuiltinTypes::Void, { BuiltinTypes::LLVMBasicBlockRef }, false, "_LLVMPositionBuilder");
blockAST->addToScope(LLVMPositionBuilderFunc->type.name, &LLVMPositionBuilderFunc->type, LLVMPositionBuilderFunc);
Func* LLVMBuildInBoundsGEP1Func = new Func("LLVMBuildInBoundsGEP1", BuiltinTypes::LLVMValueRef, { BuiltinTypes::LLVMValueRef, BuiltinTypes::LLVMValueRef, BuiltinTypes::Int8Ptr }, false, "_LLVMBuildInBoundsGEP1");
blockAST->addToScope(LLVMBuildInBoundsGEP1Func->type.name, &LLVMBuildInBoundsGEP1Func->type, LLVMBuildInBoundsGEP1Func);
Func* LLVMBuildInBoundsGEP2Func = new Func("LLVMBuildInBoundsGEP2", BuiltinTypes::LLVMValueRef, { BuiltinTypes::LLVMValueRef, BuiltinTypes::LLVMValueRef, BuiltinTypes::LLVMValueRef, BuiltinTypes::Int8Ptr }, false, "_LLVMBuildInBoundsGEP2");
blockAST->addToScope(LLVMBuildInBoundsGEP2Func->type.name, &LLVMBuildInBoundsGEP2Func->type, LLVMBuildInBoundsGEP2Func);

Function::Create(
	FunctionType::get(Types::LLVMOpaqueValue->getPointerTo(), { Types::ExprAST->getPointerTo(), Types::BlockExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "codegenExprValue",
	*module
);
Function::Create(
	FunctionType::get(Types::Void, { Types::StmtAST->getPointerTo(), Types::BlockExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "codegenStmt",
	*module
);
Function::Create(
	FunctionType::get(Type::getVoidTy(*context), { Types::BlockExprAST->getPointerTo(), Types::IdExprAST->getPointerTo(), Types::BaseType->getPointerTo(), Types::LLVMOpaqueValue->getPointerTo() }, false),
	Function::ExternalLinkage, "AddToScope",
	*module
);
Function::Create(
	FunctionType::get(Type::getVoidTy(*context), { Types::ExprAST->getPointerTo(), Types::BaseType->getPointerTo(), Types::LLVMOpaqueValue->getPointerTo() }, false),
	Function::ExternalLinkage, "AddToFileScope",
	*module
);
Function::Create(
	FunctionType::get(Type::getVoidTy(*context), { Types::BlockExprAST->getPointerTo(), Types::ExprAST->getPointerTo()->getPointerTo(), Type::getInt32Ty(*context), Type::getInt64Ty(*context), Type::getInt8PtrTy(*context) }, false),
	Function::ExternalLinkage, "DefineStatement",
	*module
);
Function::Create(
	FunctionType::get(Type::getInt8PtrTy(*context), { Types::IdExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "getIdExprASTName",
	*module
);
Function::Create(
	FunctionType::get(Type::getInt8PtrTy(*context), { Types::LiteralExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "getLiteralExprASTValue",
	*module
);
Function::Create(
	FunctionType::get(Types::BlockExprAST->getPointerTo(), { Types::BlockExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "getBlockExprASTParent",
	*module
);
Function::Create(
	FunctionType::get(Types::Void, { Types::BlockExprAST->getPointerTo(), Types::BlockExprAST->getPointerTo() }, false),
	Function::ExternalLinkage, "setBlockExprASTParent",
	*module
);
Function::Create(
	FunctionType::get(Types::BaseType->getPointerTo(), { Types::Int8Ptr, Types::Int8, Types::BaseType->getPointerTo(), Types::BaseType->getPointerTo()->getPointerTo(), Types::Int32 }, false),
	Function::ExternalLinkage, "createFuncType",
	*module
);

/*Function::Create(
	FunctionType::get(Type::getVoidTy(*context), {}, false),
	Function::ExternalLinkage, "TestFunc",
	*module
);*/
Func* testFunc = new Func("TestFunc", BuiltinTypes::Void, { }, false);
blockAST->addToScope(testFunc->type.name, &testFunc->type, testFunc);

blockAST->lookupScope("printf")->value->getFunction(module.get()); //DELETE

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
	}

	uint64_t compile(const std::string& outputPath)
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

		// Close currentFunc
		if (dbuilder)
		{
			dbuilder->finalizeSubprogram(currentFunc->getSubprogram());
			builder->SetCurrentDebugLocation(DebugLoc());
		}
		builder->SetInsertPoint(currentBB = prevBB);
		std::string errstr;
		raw_string_ostream errstream(errstr);
		bool haserr = verifyFunction(*currentFunc, &errstream);

		// Close module
		if (dbuilder)
			dbuilder->finalize();

		if (jitPassManager && !haserr)
			jitPassManager->run(*currentFunc);

#ifdef OUTPUT_JIT_CODE
		if (outputPath[0] != '\0')
			print(outputPath);
#endif

		currentFunc = prevFunc; // Switch back to parent function
		currentModule = prevModule; // Switch back to file module
		dbuilder = prevDbuilder; // Reenable debug symbol generation
		dfile = prevDfile;
		closure = nullptr;

//if (haserr) assert(0); //TODO: Raise exception
		if (haserr)
		{
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			throw CompileError("error compiling module\n" + errstr, loc);
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
	FileModule* module = new FileModule(sourcePath, moduleBlock, outputDebugSymbols, true);
	createBuiltinStatements(moduleBlock);
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

void PlchldExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
{
	if (p1 == '\0')
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType != tpltType)
		{
//printf("implicit cast from %s to %s in %s:%i\n", ((BuiltinType*)exprType)->name, ((BuiltinType*)tpltType)->name, expr->loc.filename, expr->loc.begin_line);
			IExprContext* castContext = block->lookupCast(exprType, tpltType);
			assert(castContext != nullptr);
			ExprAST* castExpr = new PlchldExprAST(expr->loc, this->p2);
			castExpr->resolvedContext = castContext;
			castExpr->resolvedParams.push_back(expr);
			params.push_back(castExpr);
			return;
		}
	}
	params.push_back(expr);
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