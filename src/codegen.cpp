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
#include "module.h"

using namespace llvm;

class KaleidoscopeJIT;
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
Module* currentModule;
Function *currentFunc;
BasicBlock *currentBB;
DIBuilder *dbuilder;
DIFile *dfile;

// Misc
Value* closure;
BlockExprAST* rootBlock = nullptr;
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
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
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
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typecbk(parentBlock, params);
	}
};
struct OpaqueExprContext : public IExprContext
{
private:
	BaseType* const type;
public:
	OpaqueExprContext(BaseType* type) : type(type) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return Variable(type, params[0]->codegen(parentBlock).value);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};


struct DynamicStmtContext : public IStmtContext
{
private:
	typedef void (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params, void* closure);
	funcPtr cbk;
	void* closure;
public:
	DynamicStmtContext(JitFunction* func, void* closure = nullptr) : cbk(reinterpret_cast<funcPtr>(func->compile())), closure(closure) {}
	void codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
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
	DynamicExprContext(JitFunction* func, BaseType* type) : cbk(reinterpret_cast<funcPtr>(func->compile())), type(type) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//BasicBlock* _currentBB = currentBB;
		Value* foo = cbk(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, params.data());
		//if (currentBB != _currentBB)
		//	builder->SetInsertPoint(currentBB = _currentBB);
		return Variable(type, new XXXValue(foo));
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
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
	bool ExprASTIsCast(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::CAST;
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
	ExprAST* getCastExprASTSource(const CastExprAST* expr)
	{
		return expr->resolvedParams[0];
	}

	unsigned getExprLine(const ExprAST* expr)
	{
		return expr->loc.begin_line;
	}
	unsigned getExprColumn(const ExprAST* expr)
	{
		return expr->loc.begin_col;
	}

	BlockExprAST* getRootScope()
	{
		return rootBlock;
	}

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, XXXValue* value)
	{
		scope->addToScope(name, type, value);
	}

	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, JitFunction* func, void* closure)
	{
		scope->defineStatement(tplt, new DynamicStmtContext(func, closure));
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

	void defineExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type)
	{
		scope->defineExpr(tplt, new DynamicExprContext(func, type));
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

	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func)
	{
		scope->defineCast(fromType, toType, new DynamicExprContext(func, toType));
	}

	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock)
	{
		scope->defineCast(fromType, toType, new StaticExprContext(codeBlock, toType));
	}

	void defineOpaqueCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType)
	{
		scope->defineCast(fromType, toType, new OpaqueExprContext(toType));
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

		ExprAST* castExpr = new CastExprAST(expr->loc);
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

	/*void DefineStatement(BlockExprAST* targetBlock, ExprAST** params, int numParams, JitFunction* func, void* closure)
	{
		targetBlock->defineStatement(std::vector<ExprAST*>(params, params + numParams), new DynamicStmtContext(func, closure));
	}*/

	Value* getValueFunction(XXXValue* value)
	{
		return value->getFunction(currentModule);
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
		FileModule* importedModule = new FileModule(realPath, importedBlock, dbuilder != nullptr, dbuilder == nullptr);
		importedBlock->codegen(scope);
		importedModule->finalize();

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

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params, std::string& name)
	{
		return new JitFunction(scope, blockAST, unwrap(((BuiltinType*)returnType)->llvmtype), params, name);
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

	JitFunction::init();

	// Declare types
	Types::create(*context);

	// Initialize builtin symbols
	initBuiltinSymbols();
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
	{
		rootBlock = this;
		fileBlock = this;
	}

	for (auto stmt: *stmts)
		stmt->codegen(this);

	if (dbuilder)
		builder->SetCurrentDebugLocation(DebugLoc());

	if (fileBlock == this)
	{
		rootBlock = nullptr;
		fileBlock = nullptr;
	}

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
				PlchldExprAST* blockParamPlchldExpr = (PlchldExprAST*)blockParamExpr;
				const Variable* var;
				switch (blockParamPlchldExpr->p1)
				{
				case 'L': return Variable(BuiltinTypes::LiteralExprAST, new XXXValue(builder->CreateBitCast(param, Types::LiteralExprAST->getPointerTo())));
				case 'I': return Variable(BuiltinTypes::IdExprAST, new XXXValue(builder->CreateBitCast(param, Types::IdExprAST->getPointerTo())));
				case 'B': return Variable(BuiltinTypes::BlockExprAST, new XXXValue(builder->CreateBitCast(param, Types::BlockExprAST->getPointerTo())));
				/*case '\0':
					var = parentBlock->lookupScope(blockParamPlchldExpr->p2);
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
				PlchldExprAST* blockParamPlchldExpr = (PlchldExprAST*)blockParamExpr;
				//return blockParamExpr->getType(parentBlock);
				/*switch (blockParamPlchldExpr->p1)
				{
				case 'L': return BuiltinTypes::LiteralExprAST;
				case 'I': return BuiltinTypes::IdExprAST;
				case 'B': return BuiltinTypes::BlockExprAST;
				}*/
				switch(blockParamPlchldExpr->p1)
				{
				case 'L': return BuiltinTypes::LiteralExprAST;
				case 'I': return BuiltinTypes::IdExprAST;
				case 'B': return BuiltinTypes::BlockExprAST;
				case '\0':
					{
						BaseType* codegenType = (BaseType*)parentBlock->lookupScope(blockParamPlchldExpr->p2)->value->getConstantValue();
						//return TpltType::get("ExprAST", wrap(Types::ExprAST->getPointerTo()), 8, (BuiltinType*)codegenType);
					}
				}
			}
			else
				assert(0); //TODO: In what scenarios do we hit this? How should it be handled?
		}
	}
	return BuiltinTypes::ExprAST;
}