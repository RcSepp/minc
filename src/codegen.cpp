// STD
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <fstream>

// Local includes
#include "minc_api.hpp"
#include "cparser.h"

std::map<StepEvent, void*> stepEventListeners;

struct StaticStmtContext : public CodegenContext
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtContext(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		cbk(parentBlock, params, stmtArgs);
		return getVoid();
	}
	MincObject* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticExprContext : public CodegenContext
{
private:
	ExprBlock cbk;
	MincObject* const type;
	void* exprArgs;
public:
	StaticExprContext(ExprBlock cbk, MincObject* type, void* exprArgs = nullptr) : cbk(cbk), type(type), exprArgs(exprArgs) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
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
		return cbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct OpaqueExprContext : public CodegenContext
{
private:
	MincObject* const type;
public:
	OpaqueExprContext(MincObject* type) : type(type) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		return Variable(type, params[0]->codegen(parentBlock).value);
	}
	MincObject* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
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

	void codegenStmt(StmtAST* stmt, BlockExprAST* scope)
	{
		stmt->codegen(scope);
	}

	MincObject* getType(const ExprAST* expr, const BlockExprAST* scope)
	{
		return expr->getType(scope);
	}

	const Location& getLocation(const ExprAST* expr)
	{
		return expr->loc;
	}

	void importBlock(BlockExprAST* scope, BlockExprAST* block)
	{
		scope->import(block);
	}

	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params)
	{
		size_t paramIdx = params.size();
		tplt->collectParams(scope, expr, params, paramIdx);
	}

	char* ExprASTToString(const ExprAST* expr)
	{
		const std::string str = expr->str();
		char* cstr = new char[str.size() + 1];
		memcpy(cstr, str.c_str(), (str.size() + 1) * sizeof(char));
		return cstr;
	}
	char* ExprASTToShortString(const ExprAST* expr)
	{
		const std::string str = expr->shortStr();
		char* cstr = new char[str.size() + 1];
		memcpy(cstr, str.c_str(), (str.size() + 1) * sizeof(char));
		return cstr;
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
	bool ExprASTIsStmt(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::STMT;
	}
	bool ExprASTIsList(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::LIST;
	}
	bool ExprASTIsPlchld(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::PLCHLD;
	}
	bool ExprASTIsEllipsis(const ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::ELLIPSIS;
	}

	void resolveExprAST(BlockExprAST* scope, ExprAST* expr)
	{
		expr->resolveTypes(scope);
	}

	BlockExprAST* wrapExprAST(ExprAST* expr)
	{
		return new BlockExprAST(expr->loc, new std::vector<ExprAST*>(1, expr));
	}

	BlockExprAST* createEmptyBlockExprAST()
	{
		return new BlockExprAST({0}, {});
	}

	BlockExprAST* cloneBlockExprAST(BlockExprAST* expr)
	{
		return (BlockExprAST*)expr->clone();
	}

	void resetBlockExprAST(BlockExprAST* expr)
	{
		expr->reset();
	}

	size_t getBlockExprASTCacheState(BlockExprAST* block)
	{
		return block->resultCacheIdx;
	}
	void resetBlockExprASTCache(BlockExprAST* block, size_t targetState)
	{
		block->clearCache(targetState);
	}

	bool isBlockExprASTBusy(BlockExprAST* block)
	{
		return block->isBusy;
	}

	void removeBlockExprAST(BlockExprAST* expr)
	{
		delete expr;
	}

	std::vector<ExprAST*>& getListExprASTExprs(ListExprAST* expr)
	{
		return expr->exprs;
	}
	ExprAST* getListExprASTExpr(ListExprAST* expr, size_t index)
	{
		return expr->exprs[index];
	}
	size_t getListExprASTSize(ListExprAST* expr)
	{
		return expr->exprs.size();
	}
	const char* getIdExprASTName(const IdExprAST* expr)
	{
		return expr->name.c_str();
	}
	const char* getLiteralExprASTValue(const LiteralExprAST* expr)
	{
		return expr->value.c_str();
	}
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr)
	{
		return expr->parent;
	}
	const std::vector<BlockExprAST*>& getBlockExprASTReferences(const BlockExprAST* expr)
	{
		return expr->references;
	}
	size_t countBlockExprASTStmts(const BlockExprAST* expr)
	{
		return expr->countStmts();
	}
	void iterateBlockExprASTStmts(const BlockExprAST* expr, std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk)
	{
		return expr->iterateStmts(cbk);
	}
	size_t countBlockExprASTExprs(const BlockExprAST* expr)
	{
		return expr->countExprs();
	}
	void iterateBlockExprASTExprs(const BlockExprAST* expr, std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk)
	{
		return expr->iterateExprs(cbk);
	}
	size_t countBlockExprASTCasts(const BlockExprAST* expr)
	{
		return expr->countCasts();
	}
	void iterateBlockExprASTCasts(const BlockExprAST* expr, std::function<void(const Cast* cast)> cbk)
	{
		return expr->iterateCasts(cbk);
	}
	size_t countBlockExprASTSymbols(const BlockExprAST* expr)
	{
		return expr->countSymbols();
	}
	void iterateBlockExprASTSymbols(const BlockExprAST* expr, std::function<void(const std::string& name, const Variable& symbol)> cbk)
	{
		return expr->iterateSymbols(cbk);
	}
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent)
	{
		if (parent == expr)
			throw CompileError("a scope cannot be it's own parent", expr->loc);
		expr->parent = parent;
	}
	void setBlockExprASTParams(BlockExprAST* expr, std::vector<Variable>& blockParams)
	{
		expr->blockParams = blockParams;
	}
	const char* getBlockExprASTName(const BlockExprAST* expr)
	{
		return expr->name.c_str();
	}
	void setBlockExprASTName(BlockExprAST* expr, const char* name)
	{
		expr->name = name;
	}
	const StmtAST* getCurrentBlockExprASTStmt(const BlockExprAST* expr)
	{
		return expr->getCurrentStmt();
	}
	ExprAST* getCastExprASTSource(const CastExprAST* expr)
	{
		return expr->resolvedParams[0];
	}
	char getPlchldExprASTLabel(const PlchldExprAST* expr)
	{
		return expr->p1;
	}
	const char* getPlchldExprASTSublabel(const PlchldExprAST* expr)
	{
		return expr->p2;
	}

	const Location* getExprLoc(const ExprAST* expr) { return &expr->loc; }
	const char* getExprFilename(const ExprAST* expr) { return expr->loc.filename; }
	unsigned getExprLine(const ExprAST* expr) { return expr->loc.begin_line; }
	unsigned getExprColumn(const ExprAST* expr) { return expr->loc.begin_column; }
	unsigned getExprEndLine(const ExprAST* expr) { return expr->loc.end_line; }
	unsigned getExprEndColumn(const ExprAST* expr) { return expr->loc.end_column; }

	ExprAST* getDerivedExprAST(ExprAST* expr)
	{
		return expr->exprtype == ExprAST::ExprType::CAST ? ((CastExprAST*)expr)->getDerivedExpr() : expr;
	}

	BaseScopeType* getScopeType(const BlockExprAST* scope)
	{
		return scope->scopeType;
	}
	void setScopeType(BlockExprAST* scope, BaseScopeType* scopeType)
	{
		scope->scopeType = scopeType;
	}

	void defineSymbol(BlockExprAST* scope, const char* name, MincObject* type, MincObject* value)
	{
		scope->defineSymbol(name, type, value);
	}

	void defineStmt1(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineStmt(tplt, new StaticStmtContext(codeBlock, stmtArgs));
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
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == ExprAST::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const PlchldExprAST* lastExpr = (const PlchldExprAST*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if (lastExpr->exprtype == ExprAST::ExprType::PLCHLD && lastExpr->p1 == 'B')
				tpltBlock->exprs->pop_back();
		}
	
		scope->defineStmt(*tpltBlock->exprs, new StaticStmtContext(codeBlock, stmtArgs));
	}

	void defineStmt3(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, CodegenContext* stmt)
	{
		if (!tplt.empty() && ((tplt.back()->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)tplt.back())->p1 == 'B')
						   || (tplt.back()->exprtype == ExprAST::ExprType::LIST && ((ListExprAST*)tplt.back())->size() == 1
							   && ((ListExprAST*)tplt.back())->at(0)->exprtype == ExprAST::ExprType::PLCHLD && ((PlchldExprAST*)((ListExprAST*)tplt.back())->at(0))->p1 == 'B')))
			scope->defineStmt(tplt, stmt);
		else
		{
			std::vector<ExprAST*> stoppedTplt(tplt);
			stoppedTplt.push_back(new StopExprAST(Location{}));
			scope->defineStmt(stoppedTplt, stmt);
		}
	}

	void defineStmt4(BlockExprAST* scope, const char* tpltStr, CodegenContext* stmt)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == ExprAST::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const PlchldExprAST* lastExpr = (const PlchldExprAST*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if (lastExpr->exprtype == ExprAST::ExprType::PLCHLD && lastExpr->p1 == 'B')
				tpltBlock->exprs->pop_back();
		}
	
		scope->defineStmt(*tpltBlock->exprs, stmt);
	}

	void defineAntiStmt2(BlockExprAST* scope, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineAntiStmt(codeBlock == nullptr ? nullptr : new StaticStmtContext(codeBlock, stmtArgs));
	}

	void defineAntiStmt3(BlockExprAST* scope, CodegenContext* stmt)
	{
		scope->defineAntiStmt(stmt);
	}

	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, MincObject* type, void* exprArgs)
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

	void defineExpr5(BlockExprAST* scope, ExprAST* tplt, CodegenContext* expr)
	{
		scope->defineExpr(tplt, expr);
	}

	void defineExpr6(BlockExprAST* scope, const char* tpltStr, CodegenContext* expr)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		BlockExprAST* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		ExprAST* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, expr);
	}

	void defineAntiExpr2(BlockExprAST* scope, ExprBlock codeBlock, MincObject* type, void* exprArgs)
	{
		scope->defineAntiExpr(codeBlock == nullptr ? nullptr : new StaticExprContext(codeBlock, type, exprArgs));
	}

	void defineAntiExpr3(BlockExprAST* scope, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineAntiExpr(codeBlock == nullptr ? nullptr : new StaticExprContext2(codeBlock, typeBlock, exprArgs));
	}

	void defineAntiExpr5(BlockExprAST* scope, CodegenContext* expr)
	{
		scope->defineAntiExpr(expr);
	}

	void defineTypeCast2(BlockExprAST* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new TypeCast(fromType, toType, new StaticExprContext(codeBlock, toType, castArgs)));
	}
	void defineInheritanceCast2(BlockExprAST* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprContext(codeBlock, toType, castArgs)));
	}

	void defineTypeCast3(BlockExprAST* scope, MincObject* fromType, MincObject* toType, CodegenContext* cast)
	{
		scope->defineCast(new TypeCast(fromType, toType, cast));
	}
	void defineInheritanceCast3(BlockExprAST* scope, MincObject* fromType, MincObject* toType, CodegenContext* cast)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, cast));
	}

	void defineOpaqueTypeCast(BlockExprAST* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new TypeCast(fromType, toType, new OpaqueExprContext(toType)));
	}
	void defineOpaqueInheritanceCast(BlockExprAST* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new OpaqueExprContext(toType)));
	}

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name)
	{
		return scope->lookupSymbol(name);
	}

	const std::string* lookupSymbolName1(const BlockExprAST* scope, const MincObject* value)
	{
		return scope->lookupSymbolName(value);
	}

	const std::string& lookupSymbolName2(const BlockExprAST* scope, const MincObject* value, const std::string& defaultName)
	{
		return scope->lookupSymbolName(value, defaultName);
	}


	Variable* importSymbol(BlockExprAST* scope, const char* name)
	{
		return scope->importSymbol(name);
	}

	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, MincObject* toType)
	{
		MincObject* fromType = expr->getType(scope);
		if (fromType == toType)
			return expr;

		const Cast* cast = scope->lookupCast(fromType, toType);
		return cast == nullptr ? nullptr : new CastExprAST(cast, expr);
	}

	bool isInstance(const BlockExprAST* scope, MincObject* fromType, MincObject* toType)
	{
		return fromType == toType || scope->isInstance(fromType, toType);
	}

	void lookupStmtCandidates(const BlockExprAST* scope, const StmtAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates)
	{
		ListExprAST stmtExprs('\0', std::vector<ExprAST*>(stmt->begin, stmt->end));
		scope->lookupStmtCandidates(&stmtExprs, candidates);
	}
	void lookupExprCandidates(const BlockExprAST* scope, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates)
	{
		scope->lookupExprCandidates(expr, candidates);
	}

	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr)
	{
		std::string report = "";
		std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>> candidates;
		std::vector<ExprAST*> resolvedParams;
		scope->lookupExprCandidates(expr, candidates);
		for (auto& candidate: candidates)
		{
			const MatchScore score = candidate.first;
			const std::pair<const ExprAST*, CodegenContext*>& context = candidate.second;
			size_t paramIdx = 0;
			resolvedParams.clear();
			context.first->collectParams(scope, const_cast<ExprAST*>(expr), resolvedParams, paramIdx);
			const std::string& typeName = scope->lookupSymbolName(context.second->getType(scope, resolvedParams), "UNKNOWN_TYPE");
			report += "\tcandidate(score=" + std::to_string(score) + "): " +  context.first->str() + "<" + typeName + ">\n";
		}
		return report;
	}

	std::string reportCasts(const BlockExprAST* scope)
	{
		std::string report = "";
		std::list<std::pair<MincObject*, MincObject*>> casts;
		scope->listAllCasts(casts);
		for (auto& cast: casts)
			report += "\t" + scope->lookupSymbolName(cast.first, "UNKNOWN_TYPE") + " -> " + scope->lookupSymbolName(cast.second, "UNKNOWN_TYPE") + "\n";
		return report;
	}

	void raiseCompileError(const char* msg, const ExprAST* loc)
	{
		throw CompileError(msg, loc ? loc->loc : Location({0}));
	}

	void registerStepEventListener(StepEvent listener, void* eventArgs)
	{
		stepEventListeners[listener] = eventArgs;
	}

	void deregisterStepEventListener(StepEvent listener)
	{
		stepEventListeners.erase(listener);
	}
}

void raiseStepEvent(const ExprAST* loc, StepEventType type)
{
	for (const std::pair<StepEvent, void*>& listener: stepEventListeners)
		listener.first(loc, type, listener.second);
}