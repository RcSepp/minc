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

struct StaticStmtKernel : public MincKernel
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtKernel(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		cbk(parentBlock, params, stmtArgs);
		return getVoid();
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticExprKernel : public MincKernel
{
private:
	ExprBlock cbk;
	MincObject* const type;
	void* exprArgs;
public:
	StaticExprKernel(ExprBlock cbk, MincObject* type, void* exprArgs = nullptr) : cbk(cbk), type(type), exprArgs(exprArgs) {}
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel2 : public MincKernel
{
private:
	ExprBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprKernel2(ExprBlock cbk, ExprTypeBlock typecbk, void* exprArgs = nullptr) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs) {}
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct OpaqueExprKernel : public MincKernel
{
private:
	MincObject* const type;
public:
	OpaqueExprKernel(MincObject* type) : type(type) {}
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return MincSymbol(type, params[0]->codegen(parentBlock).value);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};

extern "C"
{
	MincSymbol codegenExpr(MincExpr* expr, MincBlockExpr* scope)
	{
		return expr->codegen(scope);
	}

	void codegenStmt(MincStmt* stmt, MincBlockExpr* scope)
	{
		stmt->codegen(scope);
	}

	MincObject* getType(const MincExpr* expr, const MincBlockExpr* scope)
	{
		return expr->getType(scope);
	}

	const MincLocation& getLocation(const MincExpr* expr)
	{
		return expr->loc;
	}

	void importBlock(MincBlockExpr* scope, MincBlockExpr* block)
	{
		scope->import(block);
	}

	void collectParams(const MincBlockExpr* scope, const MincExpr* tplt, MincExpr* expr, std::vector<MincExpr*>& params)
	{
		size_t paramIdx = params.size();
		tplt->collectParams(scope, expr, params, paramIdx);
	}

	char* ExprToString(const MincExpr* expr)
	{
		const std::string str = expr->str();
		char* cstr = new char[str.size() + 1];
		memcpy(cstr, str.c_str(), (str.size() + 1) * sizeof(char));
		return cstr;
	}
	char* ExprToShortString(const MincExpr* expr)
	{
		const std::string str = expr->shortStr();
		char* cstr = new char[str.size() + 1];
		memcpy(cstr, str.c_str(), (str.size() + 1) * sizeof(char));
		return cstr;
	}

	bool ExprIsId(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::ID;
	}
	bool ExprIsCast(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::CAST;
	}
	bool ExprIsParam(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::PARAM;
	}
	bool ExprIsBlock(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::BLOCK;
	}
	bool ExprIsStmt(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::STMT;
	}
	bool ExprIsList(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::LIST;
	}
	bool ExprIsPlchld(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::PLCHLD;
	}
	bool ExprIsEllipsis(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::ELLIPSIS;
	}

	void resolveExpr(MincExpr* expr, MincBlockExpr* scope)
	{
		expr->resolve(scope);
	}

	void forgetExpr(MincExpr* expr)
	{
		expr->forget();
	}

	MincBlockExpr* wrapExpr(MincExpr* expr)
	{
		return new MincBlockExpr(expr->loc, new std::vector<MincExpr*>(1, expr));
	}

	MincBlockExpr* createEmptyBlockExpr()
	{
		return new MincBlockExpr({0}, {});
	}

	MincBlockExpr* cloneBlockExpr(MincBlockExpr* expr)
	{
		return (MincBlockExpr*)expr->clone();
	}

	void resetBlockExpr(MincBlockExpr* expr)
	{
		expr->reset();
	}

	size_t getBlockExprCacheState(MincBlockExpr* block)
	{
		return block->resultCacheIdx;
	}
	void resetBlockExprCache(MincBlockExpr* block, size_t targetState)
	{
		block->clearCache(targetState);
	}

	bool isBlockExprBusy(MincBlockExpr* block)
	{
		return block->isBusy;
	}

	void removeBlockExpr(MincBlockExpr* expr)
	{
		delete expr;
	}

	std::vector<MincExpr*>& getListExprExprs(MincListExpr* expr)
	{
		return expr->exprs;
	}
	MincExpr* getListExprExpr(MincListExpr* expr, size_t index)
	{
		return expr->exprs[index];
	}
	size_t getListExprSize(MincListExpr* expr)
	{
		return expr->exprs.size();
	}
	const char* getIdExprName(const MincIdExpr* expr)
	{
		return expr->name.c_str();
	}
	const char* getLiteralExprValue(const MincLiteralExpr* expr)
	{
		return expr->value.c_str();
	}
	const std::vector<MincBlockExpr*>& getBlockExprReferences(const MincBlockExpr* expr)
	{
		return expr->references;
	}
	size_t countBlockExprStmts(const MincBlockExpr* expr)
	{
		return expr->countStmts();
	}
	void iterateBlockExprStmts(const MincBlockExpr* expr, std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk)
	{
		return expr->iterateStmts(cbk);
	}
	size_t countBlockExprExprs(const MincBlockExpr* expr)
	{
		return expr->countExprs();
	}
	void iterateBlockExprExprs(const MincBlockExpr* expr, std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk)
	{
		return expr->iterateExprs(cbk);
	}
	size_t countBlockExprCasts(const MincBlockExpr* expr)
	{
		return expr->countCasts();
	}
	void iterateBlockExprCasts(const MincBlockExpr* expr, std::function<void(const MincCast* cast)> cbk)
	{
		return expr->iterateCasts(cbk);
	}
	size_t countBlockExprSymbols(const MincBlockExpr* expr)
	{
		return expr->countSymbols();
	}
	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk)
	{
		return expr->iterateSymbols(cbk);
	}
	MincBlockExpr* getBlockExprParent(const MincBlockExpr* expr)
	{
		return expr->parent;
	}
	void setBlockExprParent(MincBlockExpr* expr, MincBlockExpr* parent)
	{
		if (parent == expr)
			throw CompileError("a scope cannot be it's own parent", expr->loc);
		expr->parent = parent;
	}
	void setBlockExprParams(MincBlockExpr* expr, std::vector<MincSymbol>& blockParams)
	{
		expr->blockParams = blockParams;
	}
	const char* getBlockExprName(const MincBlockExpr* expr)
	{
		return expr->name.c_str();
	}
	void setBlockExprName(MincBlockExpr* expr, const char* name)
	{
		expr->name = name;
	}
	void* getBlockExprUser(const MincBlockExpr* expr)
	{
		return expr->user;
	}
	void setBlockExprUser(MincBlockExpr* expr, void* user)
	{
		expr->user = user;
	}
	void* getBlockExprUserType(const MincBlockExpr* expr)
	{
		return expr->userType;
	}
	void setBlockExprUserType(MincBlockExpr* expr, void* userType)
	{
		expr->userType = userType;
	}
	const MincStmt* getCurrentBlockExprStmt(const MincBlockExpr* expr)
	{
		return expr->getCurrentStmt();
	}
	MincExpr* getCastExprSource(const MincCastExpr* expr)
	{
		return expr->resolvedParams[0];
	}
	char getPlchldExprLabel(const MincPlchldExpr* expr)
	{
		return expr->p1;
	}
	const char* getPlchldExprSublabel(const MincPlchldExpr* expr)
	{
		return expr->p2;
	}

	const MincLocation* getExprLoc(const MincExpr* expr) { return &expr->loc; }
	const char* getExprFilename(const MincExpr* expr) { return expr->loc.filename; }
	unsigned getExprLine(const MincExpr* expr) { return expr->loc.begin_line; }
	unsigned getExprColumn(const MincExpr* expr) { return expr->loc.begin_column; }
	unsigned getExprEndLine(const MincExpr* expr) { return expr->loc.end_line; }
	unsigned getExprEndColumn(const MincExpr* expr) { return expr->loc.end_column; }

	MincExpr* getDerivedExpr(MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::CAST ? ((MincCastExpr*)expr)->getDerivedExpr() : expr;
	}

	MincScopeType* getScopeType(const MincBlockExpr* scope)
	{
		return scope->scopeType;
	}
	void setScopeType(MincBlockExpr* scope, MincScopeType* scopeType)
	{
		scope->scopeType = scopeType;
	}

	void defineSymbol(MincBlockExpr* scope, const char* name, MincObject* type, MincObject* value)
	{
		scope->defineSymbol(name, type, value);
	}

	void defineStmt1(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineStmt(tplt, new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineStmt2(MincBlockExpr* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == MincExpr::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const MincPlchldExpr* lastExpr = (const MincPlchldExpr*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if (lastExpr->exprtype == MincExpr::ExprType::PLCHLD && lastExpr->p1 == 'B')
				tpltBlock->exprs->pop_back();
		}
	
		scope->defineStmt(*tpltBlock->exprs, new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineStmt3(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, MincKernel* stmt)
	{
		if (!tplt.empty() && ((tplt.back()->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)tplt.back())->p1 == 'B')
						   || (tplt.back()->exprtype == MincExpr::ExprType::LIST && ((MincListExpr*)tplt.back())->size() == 1
							   && ((MincListExpr*)tplt.back())->at(0)->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)((MincListExpr*)tplt.back())->at(0))->p1 == 'B')))
			scope->defineStmt(tplt, stmt);
		else
		{
			std::vector<MincExpr*> stoppedTplt(tplt);
			stoppedTplt.push_back(new MincStopExpr(MincLocation{}));
			scope->defineStmt(stoppedTplt, stmt);
		}
	}

	void defineStmt4(MincBlockExpr* scope, const char* tpltStr, MincKernel* stmt)
	{
		// Append STOP expr to make tpltStr a valid statement
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';

		// Parse tpltStr into tpltBlock
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);

		// Remove appended STOP expr if last expr is $B
		assert(tpltBlock->exprs->back()->exprtype == MincExpr::ExprType::STOP);
		if (tpltBlock->exprs->size() >= 2)
		{
			const MincPlchldExpr* lastExpr = (const MincPlchldExpr*)tpltBlock->exprs->at(tpltBlock->exprs->size() - 2);
			if (lastExpr->exprtype == MincExpr::ExprType::PLCHLD && lastExpr->p1 == 'B')
				tpltBlock->exprs->pop_back();
		}
	
		scope->defineStmt(*tpltBlock->exprs, stmt);
	}

	void defineDefaultStmt2(MincBlockExpr* scope, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(codeBlock == nullptr ? nullptr : new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt)
	{
		scope->defineDefaultStmt(stmt);
	}

	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, MincObject* type, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		MincExpr* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprKernel(codeBlock, type, exprArgs));
	}

	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		MincExpr* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, new StaticExprKernel2(codeBlock, typeBlock, exprArgs));
	}

	void defineExpr5(MincBlockExpr* scope, MincExpr* tplt, MincKernel* expr)
	{
		scope->defineExpr(tplt, expr);
	}

	void defineExpr6(MincBlockExpr* scope, const char* tpltStr, MincKernel* expr)
	{
		std::stringstream ss(tpltStr);
		ss << tpltStr << ';';
		CLexer lexer(ss, std::cout);
		MincBlockExpr* tpltBlock;
		yy::CParser parser(lexer, nullptr, &tpltBlock);
		if (parser.parse())
			throw CompileError("error parsing template " + std::string(tpltStr), scope->loc);
		MincExpr* tplt = tpltBlock->exprs->at(0);
		scope->defineExpr(tplt, expr);
	}

	void defineDefaultExpr2(MincBlockExpr* scope, ExprBlock codeBlock, MincObject* type, void* exprArgs)
	{
		scope->defineDefaultExpr(codeBlock == nullptr ? nullptr : new StaticExprKernel(codeBlock, type, exprArgs));
	}

	void defineDefaultExpr3(MincBlockExpr* scope, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineDefaultExpr(codeBlock == nullptr ? nullptr : new StaticExprKernel2(codeBlock, typeBlock, exprArgs));
	}

	void defineDefaultExpr5(MincBlockExpr* scope, MincKernel* expr)
	{
		scope->defineDefaultExpr(expr);
	}

	void defineTypeCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new TypeCast(fromType, toType, new StaticExprKernel(codeBlock, toType, castArgs)));
	}
	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprKernel(codeBlock, toType, castArgs)));
	}

	void defineTypeCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast)
	{
		scope->defineCast(new TypeCast(fromType, toType, cast));
	}
	void defineInheritanceCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, cast));
	}

	void defineOpaqueTypeCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new TypeCast(fromType, toType, new OpaqueExprKernel(toType)));
	}
	void defineOpaqueInheritanceCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new OpaqueExprKernel(toType)));
	}

	const MincSymbol* lookupSymbol(const MincBlockExpr* scope, const char* name)
	{
		return scope->lookupSymbol(name);
	}

	const std::string* lookupSymbolName1(const MincBlockExpr* scope, const MincObject* value)
	{
		return scope->lookupSymbolName(value);
	}

	const std::string& lookupSymbolName2(const MincBlockExpr* scope, const MincObject* value, const std::string& defaultName)
	{
		return scope->lookupSymbolName(value, defaultName);
	}


	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name)
	{
		return scope->importSymbol(name);
	}

	MincExpr* lookupCast(const MincBlockExpr* scope, MincExpr* expr, MincObject* toType)
	{
		MincObject* fromType = expr->getType(scope);
		if (fromType == toType)
			return expr;

		const MincCast* cast = scope->lookupCast(fromType, toType);
		return cast == nullptr ? nullptr : new MincCastExpr(cast, expr);
	}

	bool isInstance(const MincBlockExpr* scope, MincObject* fromType, MincObject* toType)
	{
		return fromType == toType || scope->isInstance(fromType, toType);
	}

	void lookupStmtCandidates(const MincBlockExpr* scope, const MincStmt* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates)
	{
		MincListExpr stmtExprs('\0', std::vector<MincExpr*>(stmt->begin, stmt->end));
		scope->lookupStmtCandidates(&stmtExprs, candidates);
	}
	void lookupExprCandidates(const MincBlockExpr* scope, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates)
	{
		scope->lookupExprCandidates(expr, candidates);
	}

	std::string reportExprCandidates(const MincBlockExpr* scope, const MincExpr* expr)
	{
		std::string report = "";
		std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>> candidates;
		std::vector<MincExpr*> resolvedParams;
		scope->lookupExprCandidates(expr, candidates);
		for (auto& candidate: candidates)
		{
			const MatchScore score = candidate.first;
			const std::pair<const MincExpr*, MincKernel*>& kernel = candidate.second;
			size_t paramIdx = 0;
			resolvedParams.clear();
			kernel.first->collectParams(scope, const_cast<MincExpr*>(expr), resolvedParams, paramIdx);
			const std::string& typeName = scope->lookupSymbolName(kernel.second->getType(scope, resolvedParams), "UNKNOWN_TYPE");
			report += "\tcandidate(score=" + std::to_string(score) + "): " +  kernel.first->str() + "<" + typeName + ">\n";
		}
		return report;
	}

	std::string reportCasts(const MincBlockExpr* scope)
	{
		std::string report = "";
		std::list<std::pair<MincObject*, MincObject*>> casts;
		scope->listAllCasts(casts);
		for (auto& cast: casts)
			report += "\t" + scope->lookupSymbolName(cast.first, "UNKNOWN_TYPE") + " -> " + scope->lookupSymbolName(cast.second, "UNKNOWN_TYPE") + "\n";
		return report;
	}

	void raiseCompileError(const char* msg, const MincExpr* loc)
	{
		throw CompileError(msg, loc ? loc->loc : MincLocation({0}));
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

void raiseStepEvent(const MincExpr* loc, StepEventType type)
{
	for (const std::pair<StepEvent, void*>& listener: stepEventListeners)
		listener.first(loc, type, listener.second);
}