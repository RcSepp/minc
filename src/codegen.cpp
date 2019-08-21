#define OUTPUT_JIT_CODE
const bool ENABLE_JIT_CODE_DEBUG_SYMBOLS = false;
const bool OPTIMIZE_JIT_CODE = true;

// STD
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>
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
BaseType BASE_TYPE;
Variable VOID;

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
std::map<const BaseType*, TypeDescription> typereg;
const std::string NULL_TYPE = "NULL";
const std::string UNKNOWN_TYPE = "UNKNOWN_TYPE";

struct StaticStmtContext : public CodegenContext
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtContext(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		cbk(parentBlock, params, stmtArgs);
		return Variable(nullptr, new XXXValue(Constant::getNullValue(Type::getVoidTy(*context)->getPointerTo())));
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return nullptr;
	}
};
struct StaticExprContext : public CodegenContext
{
private:
	ExprBlock cbk;
	BaseType* const type;
	void* exprArgs;
public:
	StaticExprContext(ExprBlock cbk, BaseType* type, void* exprArgs = nullptr) : cbk(cbk), type(type), exprArgs(exprArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params, exprArgs);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};
struct StaticExprContext2 : public CodegenContext
{
private:
	ExprBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprContext2(ExprBlock cbk, ExprTypeBlock typecbk, void* exprArgs = nullptr) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		//if (dbuilder)
		//	builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
		return cbk(parentBlock, params, exprArgs);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct OpaqueExprContext : public CodegenContext
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
		return VOID;
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
	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope)
	{
		return expr->codegen(scope);
	}

	LLVMValueRef codegenExprValue(ExprAST* expr, BlockExprAST* scope)
	{
		return wrap(((XXXValue*)expr->codegen(scope).value)->val);
	}

	uint64_t codegenExprConstant(ExprAST* expr, BlockExprAST* scope)
	{
		return expr->codegen(scope).value->getConstantValue();
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
		size_t paramIdx = params.size();
		tplt->collectParams(scope, expr, params, paramIdx);
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

	void resolveExprAST(BlockExprAST* scope, ExprAST* expr)
	{
		scope->lookupExpr(expr);
	}

	BlockExprAST* wrapExprAST(ExprAST* expr)
	{
		return new BlockExprAST(expr->loc, new std::vector<ExprAST*>(1, expr));
	}

	std::vector<ExprAST*>& getExprListASTExpressions(ExprListAST* expr)
	{
		return expr->exprs;
	}
	ExprAST* getExprListASTExpression(ExprListAST* expr, size_t index)
	{
		return expr->exprs[index];
	}
	size_t getExprListASTSize(ExprListAST* expr)
	{
		return expr->exprs.size();
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

	const Location* getExprLoc(const ExprAST* expr) { return &expr->loc; }
	const char* getExprFilename(const ExprAST* expr) { return expr->loc.filename; }
	unsigned getExprLine(const ExprAST* expr) { return expr->loc.begin_line; }
	unsigned getExprColumn(const ExprAST* expr) { return expr->loc.begin_col; }
	unsigned getExprEndLine(const ExprAST* expr) { return expr->loc.end_line; }
	unsigned getExprEndColumn(const ExprAST* expr) { return expr->loc.end_col; }

	BlockExprAST* getRootScope()
	{
		return rootBlock;
	}

	const std::string& getTypeName(const BaseType* type)
	{
		if (type == nullptr)
			return NULL_TYPE;
		const auto typeDesc = typereg.find(type);
		if (typeDesc == typereg.cend())
			return UNKNOWN_TYPE;
		else
			return typeDesc->second.name;
	}
	const char* getTypeName2(const BaseType* type)
	{
		if (type == nullptr)
			return NULL_TYPE.c_str();
		const auto typeDesc = typereg.find(type);
		if (typeDesc == typereg.cend())
			return UNKNOWN_TYPE.c_str();
		else
			return typeDesc->second.name.c_str();
	}

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, BaseValue* value)
	{
		scope->addToScope(name, type, value);
	}

	void defineType(const char* name, BaseType* type)
	{
		typereg[type] = TypeDescription{name};
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

	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse() || tpltBlock->exprs->size() < 2)
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == ExprAST::ExprType::STOP);
		const PlchldExprAST* lastExpr = (const PlchldExprAST*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
		if (lastExpr->exprtype == ExprAST::ExprType::PLCHLD && lastExpr->p1 == 'B')
			tpltBlock->exprs->pop_back();
	
		scope->defineStatement(*tpltBlock->exprs, new StaticStmtContext(codeBlock, stmtArgs));
	}

	void defineExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type)
	{
		scope->defineExpr(tplt, new DynamicExprContext(func, type));
	}

	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext(codeBlock, type, exprArgs));
	}

	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprContext2(codeBlock, typeBlock, exprArgs));
	}

	void defineExpr4(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, JitFunction* typeFunc)
	{
		scope->defineExpr(tplt, new DynamicExprContext2(func, typeFunc));
	}

	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func)
	{
		scope->defineCast(fromType, toType, new DynamicExprContext(func, toType));
	}

	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(fromType, toType, new StaticExprContext(codeBlock, toType, castArgs));
	}

	void defineOpaqueCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType)
	{
		scope->defineCast(fromType, toType, new OpaqueExprContext(toType));
	}

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured)
	{
		return scope->lookupScope(name, isCaptured);
	}

	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType)
	{
		BaseType* fromType = expr->getType(scope);
		if (fromType == toType)
			return expr;

		CodegenContext* castContext = scope->lookupCast(fromType, toType);
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
		std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>&> candidates;
		std::vector<ExprAST*> resolvedParams;
		scope->lookupExprCandidates(expr, candidates);
		for (auto& candidate: candidates)
		{
			const MatchScore score = candidate.first;
			const std::pair<const ExprAST*, CodegenContext*>& context = candidate.second;
			size_t paramIdx = 0;
			resolvedParams.clear();
			context.first->collectParams(scope, const_cast<ExprAST*>(expr), resolvedParams, paramIdx);
			const std::string& typeName = getTypeName(context.second->getType(scope, resolvedParams));
			report += "\tcandidate(score=" + std::to_string(score) + "): " +  context.first->str() + "<" + typeName + ">\n";
		}
		return report;
	}

	std::string reportCasts(const BlockExprAST* scope)
	{
		std::string report = "";
		std::list<std::pair<BaseType*, BaseType*>> casts;
		scope->listAllCasts(casts);
		for (auto& cast: casts)
			report += "\t" +  getTypeName(cast.first) + " -> " + getTypeName(cast.second) + "\n";
		return report;
	}

	BaseType* getBaseType()
	{
		return &BASE_TYPE;
	}

	BaseType* getVoidType()
	{
		return VOID.type;
	}

	void raiseCompileError(const char* msg, const ExprAST* loc)
	{
		throw CompileError(msg, loc->loc);
	}

	void AddToScope(BlockExprAST* targetBlock, IdExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		targetBlock->addToScope(nameAST->name, type, new XXXValue(unwrap(val)));
	}

	void AddToFileScope(IdExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		fileBlock->addToScope(nameAST->name, type, new XXXValue(unwrap(val)));
	}

	LLVMValueRef LookupScope(BlockExprAST* scope, IdExprAST* nameAST)
	{
		const Variable* var = scope->lookupScope(nameAST->name);
		return var == nullptr ? nullptr : wrap(((XXXValue*)var->value)->val);
	}
	BaseType* LookupScopeType(BlockExprAST* scope, IdExprAST* nameAST)
	{
		const Variable* var = scope->lookupScope(nameAST->name);
		return var == nullptr ? nullptr : var->type;
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
}

void init()
{
	context = unwrap(LLVMGetGlobalContext());//new LLVMContext();
	builder = new IRBuilder<>(*context);

	JitFunction::init();

	// Declare types
	Types::create(*context);
	VOID.type = BuiltinType::get("void", LLVMVoidType(), 0);
	VOID.value = new XXXValue(Constant::getNullValue(Type::getVoidTy(*context)));
}

IModule* createModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols)
{
	return new FileModule(sourcePath, moduleBlock, outputDebugSymbols, !outputDebugSymbols);
}

StmtAST::StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context)
	: ExprAST(Location{ exprBegin[0]->loc.filename, exprBegin[0]->loc.begin_line, exprBegin[0]->loc.begin_col, exprEnd[-1]->loc.end_line, exprEnd[-1]->loc.end_col }, ExprAST::ExprType::STMT),
	begin(exprBegin), end(exprEnd)
{
	resolvedContext = context;
}

Variable BlockExprAST::codegen(BlockExprAST* parentBlock)
{
	parent = parentBlock;

	if (fileBlock == nullptr)
	{
		rootBlock = this;
		fileBlock = this;
	}

	for (ExprASTIter iter = exprs->cbegin(); iter != exprs->cend();)
	{
		const ExprASTIter beginExpr = iter;
		const std::pair<const std::vector<ExprAST*>, CodegenContext*>* stmtContext = lookupStatement(iter, exprs->cend());
		const ExprASTIter endExpr = iter;

		StmtAST stmt(beginExpr, endExpr, stmtContext ? stmtContext->second : nullptr);

		if (stmtContext)
		{
			stmt.collectParams(this, stmtContext->first);
			stmt.codegen(this);
		}
		else
			throw UndefinedStmtException(&stmt);
	}

	if (dbuilder)
		builder->SetCurrentDebugLocation(DebugLoc());

	if (fileBlock == this)
	{
		rootBlock = nullptr;
		fileBlock = nullptr;
	}

	//parent = nullptr;
	return VOID;
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
		const BaseType *expectedType = resolvedContext->getType(parentBlock, resolvedParams), *gotType = var.type;
		if (expectedType != gotType)
		{
			throw CompileError(
				("invalid expression return type: " + ExprASTToString(this) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">").c_str(),
				this->loc
			);
		}
		return var;
	}
	else
		throw UndefinedExprException{this};
}

Variable StmtAST::codegen(BlockExprAST* parentBlock)
{
	if (dbuilder)
		builder->SetCurrentDebugLocation(DebugLoc::get(loc.begin_line, loc.begin_col, currentFunc->getSubprogram()));
	resolvedContext->codegen(parentBlock, resolvedParams);
	return Variable(BuiltinTypes::Void, new XXXValue(nullptr));
}

void ExprAST::resolveTypes(BlockExprAST* block)
{
	block->lookupExpr(this);
}

BaseType* PlchldExprAST::getType(const BlockExprAST* parentBlock) const
{
	if (p2 == nullptr)
		return nullptr;
	const Variable* var = parentBlock->lookupScope(p2);
	if (var == nullptr)
		throw UndefinedIdentifierException(new IdExprAST(loc, p2));
	return (BaseType*)var->value->getConstantValue();
}

Variable ParamExprAST::codegen(BlockExprAST* parentBlock)
{
	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr)
		throw CompileError("invalid use of parameter expression in parameterless scope", loc);
	if (staticIdx >= blockParams->size())
		throw CompileError("parameter index out of bounds", loc);
	return blockParams->at(staticIdx);
}

BaseType* ParamExprAST::getType(const BlockExprAST* parentBlock) const
{
	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr || staticIdx >= blockParams->size())
		return nullptr;
	return blockParams->at(staticIdx).type;
}