#include <sstream>
#include "minc_api.h"
#include "minc_api.hpp"

#define DETECT_UNDEFINED_TYPE_CASTS

const MincSymbol VOID = MincSymbol(new MincObject(), nullptr);
MincBlockExpr* const rootBlock = new MincBlockExpr({0}, {});
MincBlockExpr* fileBlock = nullptr;
MincBlockExpr* topLevelBlock = nullptr;
std::map<std::pair<MincScopeType*, MincScopeType*>, std::map<MincObject*, ImptBlock>> importRules;
std::map<StepEvent, void*> stepEventListeners;

const MincSymbolId MincSymbolId::NONE = { 0, 0, 0 };

bool operator==(const MincSymbolId& left, const MincSymbolId& right)
{
	return left.i == right.i && left.p == right.p && left.r == right.r;
}

bool operator!=(const MincSymbolId& left, const MincSymbolId& right)
{
	return left.i != right.i || left.p != right.p || left.r != right.r;
}

struct StaticStmtKernel : public MincKernel
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtKernel(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		cbk(parentBlock, params, stmtArgs);
		return getVoid();
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticStmtKernel2 : public MincKernel
{
private:
	StmtBlock cbk;
	void* stmtArgs;
public:
	StaticStmtKernel2(StmtBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		cbk(parentBlock, params, stmtArgs);
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return getVoid();
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticStmtKernel3 : public MincKernel
{
private:
	StmtBlock buildCbk;
	StmtBlock runCbk;
	void* stmtArgs;
public:
	StaticStmtKernel3(StmtBlock buildCbk, StmtBlock runCbk, void* stmtArgs = nullptr) : buildCbk(buildCbk), runCbk(runCbk), stmtArgs(stmtArgs) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		buildCbk(parentBlock, params, stmtArgs);
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		runCbk(parentBlock, params, stmtArgs);
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
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
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
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return cbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct StaticExprKernel3 : public MincKernel
{
private:
	ExprBlock cbk;
	MincObject* const type;
	MincObject* value;
	void* exprArgs;
public:
	StaticExprKernel3(ExprBlock cbk, MincObject* type, void* exprArgs = nullptr) : cbk(cbk), type(type), value(nullptr), exprArgs(exprArgs) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		value = cbk(parentBlock, params, exprArgs).value;
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return MincSymbol(type, value);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel4 : public MincKernel
{
private:
	ExprBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
	MincSymbol symbol;
public:
	StaticExprKernel4(ExprBlock cbk, ExprTypeBlock typecbk, void* exprArgs) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs) {}
	StaticExprKernel4(ExprBlock cbk, ExprTypeBlock typecbk, void* exprArgs, MincSymbol symbol) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs), symbol(symbol) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		MincSymbol symbol = cbk(parentBlock, params, exprArgs);
		return new StaticExprKernel4(cbk, typecbk, exprArgs, symbol);
	}
	void dispose(MincKernel* kernel)
	{
		delete kernel;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return symbol;
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct StaticExprKernel5 : public MincKernel
{
private:
	StmtBlock buildCbk;
	ExprBlock runCbk;
	MincObject* const type;
	void* exprArgs;
public:
	StaticExprKernel5(StmtBlock buildCbk, ExprBlock runCbk, MincObject* type, void* exprArgs = nullptr) : buildCbk(buildCbk), runCbk(runCbk), type(type), exprArgs(exprArgs) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		buildCbk(parentBlock, params, exprArgs);
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return runCbk(parentBlock, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel6 : public MincKernel
{
private:
	StmtBlock buildCbk;
	ExprBlock runCbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprKernel6(StmtBlock buildCbk, ExprBlock runCbk, ExprTypeBlock typecbk, void* exprArgs) : buildCbk(buildCbk), runCbk(runCbk), typecbk(typecbk), exprArgs(exprArgs) {}
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		buildCbk(parentBlock, params, exprArgs);
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return runCbk(parentBlock, params, exprArgs);
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
	MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		params[0]->build(parentBlock);
		return this;
	}
	MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		return MincSymbol(type, params[0]->run(parentBlock).value);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};

void raiseStepEvent(const MincExpr* loc, StepEventType type)
{
	for (const std::pair<StepEvent, void*>& listener: stepEventListeners)
		listener.first(loc, type, listener.second);
}

MincBlockExpr::MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs, std::vector<MincStmt>* builtStmts)
	: MincExpr(loc, MincExpr::ExprType::BLOCK), defaultStmtKernel(nullptr), defaultExprKernel(nullptr), castreg(this), builtStmts(builtStmts), ownesResolvedStmts(false), parent(nullptr), exprs(exprs),
	  stmtIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isResuming(false), isBusy(false), user(nullptr), userType(nullptr)
{
}

MincBlockExpr::MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs)
	: MincExpr(loc, MincExpr::ExprType::BLOCK), defaultStmtKernel(nullptr), defaultExprKernel(nullptr), castreg(this), builtStmts(new std::vector<MincStmt>()), ownesResolvedStmts(true), parent(nullptr), exprs(exprs),
	  stmtIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isResuming(false), isBusy(false), user(nullptr), userType(nullptr)
{
}

MincBlockExpr::~MincBlockExpr()
{
	if (ownesResolvedStmts)
		delete builtStmts;
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, MincKernel* stmt)
{
	if (isBuilt())
		throw CompileError("defining statement after block has been built", loc);

	for (MincExpr* tpltExpr: tplt)
		tpltExpr->resolve(this);
	stmtreg.defineStmt(new MincListExpr('\0', tplt), stmt);
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> run)
{
	if (isBuilt())
		throw CompileError("defining statement after block has been built", loc);

	struct StmtKernel : public MincKernel
	{
		const std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> runCbk;
		StmtKernel(std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> run)
			: runCbk(run) {}
		virtual ~StmtKernel() {}
		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { runCbk(parentBlock, params); return VOID; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return VOID.type; }
	};
	defineStmt(tplt, new StmtKernel(run));
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt,
							   std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> build,
							   std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> run)
{
	if (isBuilt())
		throw CompileError("defining statement after block has been built", loc);

	struct StmtKernel : public MincKernel
	{
		const std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> buildCbk;
		const std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> runCbk;
		StmtKernel(std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> build,
				   std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> run)
			: buildCbk(build), runCbk(run) {}
		virtual ~StmtKernel() {}
		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { if (buildCbk != nullptr) buildCbk(parentBlock, params); return this; }
		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { if (runCbk != nullptr) runCbk(parentBlock, params); return VOID; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return VOID.type; }
	};
	defineStmt(tplt, new StmtKernel(build, run));
}

void MincBlockExpr::lookupStmtCandidates(const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupStmtCandidates(this, stmt, candidates);
		for (const MincBlockExpr* ref: block->references)
			ref->stmtreg.lookupStmtCandidates(this, stmt, candidates);
	}
}

size_t MincBlockExpr::countStmts() const
{
	return stmtreg.countStmts();
}

void MincBlockExpr::iterateStmts(std::function<void(const MincListExpr* tplt, MincKernel* stmt)> cbk) const
{
	stmtreg.iterateStmts(cbk);
}

void MincBlockExpr::defineDefaultStmt(MincKernel* stmt)
{
	if (isBuilt())
		throw CompileError("defining statement after block has been built", loc);

	defaultStmtKernel = stmt;
}

void MincBlockExpr::defineExpr(MincExpr* tplt, MincKernel* expr)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	tplt->resolve(this);
	stmtreg.defineExpr(tplt, expr);

	// // Forget future expressions
	// if (stmtIdx + 1 == builtStmts->size()) // If the current statement is the last statement
	// 									   // This avoids forgetting statements during consecutive iterations of already resolved blocks.
	// 	// Forget expressions beyond the current statement
	// 	for (MincExprIter expr = builtStmts->back().end; expr != exprs->end() && (*expr)->isResolved(); ++expr)
	// 		(*expr)->forget();
	//TODO: This doesn't forget future expressions for child blocks of the define target
	// For example test_mutable_exprs() will fail, because an expression is defined in file scope, while an already resolved expression in
	// test_mutable_exprs-scope is affected by the redefine.
	// A possible solution would be to remember the last define in the target block and check if parent blocks had recently been updated
	// during build.
	// Until such a solution has been implemented, MincBlockExpr::lookupStmt() forgets future expressions after every resolved statement.
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> run, MincObject* type)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> runCbk;
		MincObject* const type;
		ExprKernel(std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> run, MincObject* type)
			: runCbk(run), type(type) {}
		virtual ~ExprKernel() {}
		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return runCbk(parentBlock, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return type; }
	};
	defineExpr(tplt, new ExprKernel(run, type));
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> runCbk;
		const std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getTypeCbk;
		ExprKernel(std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
			: runCbk(run), getTypeCbk(getType) {}
		virtual ~ExprKernel() {}
		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return runCbk(parentBlock, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getTypeCbk(parentBlock, params); }
	};
	defineExpr(tplt, new ExprKernel(run, getType));
}

void MincBlockExpr::lookupExprCandidates(const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->stmtreg.lookupExprCandidates(this, expr, candidates);
		for (const MincBlockExpr* ref: block->references)
			ref->stmtreg.lookupExprCandidates(this, expr, candidates);
	}
}

size_t MincBlockExpr::countExprs() const
{
	return stmtreg.countExprs();
}

void MincBlockExpr::iterateExprs(std::function<void(const MincExpr* tplt, MincKernel* expr)> cbk) const
{
	stmtreg.iterateExprs(cbk);
}

void MincBlockExpr::defineDefaultExpr(MincKernel* expr)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	defaultExprKernel = expr;
}

void MincBlockExpr::defineCast(MincCast* cast)
{
	if (isBuilt())
		throw CompileError("defining cast after block has been built", loc);

	// Skip if one of the following is true
	// 1. fromType == toType
	// 2. A cast exists from fromType to toType with a lower or equal cost
	// 3. Cast is an inheritance and another inheritance cast exists from toType to fromType (inheritance loop avoidance)
	const MincCast* existingCast;
	if (cast->fromType == cast->toType
		|| ((existingCast = lookupCast(cast->fromType, cast->toType)) != nullptr && existingCast->getCost() <= cast->getCost())
		|| ((existingCast = lookupCast(cast->toType, cast->fromType)) != nullptr && existingCast->getCost() == 0 && cast->getCost() == 0))
		return;

#ifdef DETECT_UNDEFINED_TYPE_CASTS
	if (lookupSymbolName1(this, cast->fromType) == nullptr || lookupSymbolName1(this, cast->toType) == nullptr)
		throw CompileError("type-cast defined from " + lookupSymbolName2(this, cast->fromType, "UNKNOWN_TYPE") + " to " + lookupSymbolName2(this, cast->toType, "UNKNOWN_TYPE"));
#endif

	castreg.defineDirectCast(cast);

	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		castreg.defineIndirectCast(block->castreg, cast);
		for (const MincBlockExpr* ref: block->references)
			castreg.defineIndirectCast(ref->castreg, cast);
	}
}

const MincCast* MincBlockExpr::lookupCast(MincObject* fromType, MincObject* toType) const
{
	const MincCast* cast;
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if ((cast = block->castreg.lookupCast(fromType, toType)) != nullptr)
			return cast;
		for (const MincBlockExpr* ref: block->references)
			if ((cast = ref->castreg.lookupCast(fromType, toType)) != nullptr)
				return cast;
	}
	return nullptr;
}

bool MincBlockExpr::isInstance(MincObject* derivedType, MincObject* baseType) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if (block->castreg.isInstance(derivedType, baseType))
			return true;
		for (const MincBlockExpr* ref: block->references)
			if (ref->castreg.isInstance(derivedType, baseType))
				return true;
	}
	return false;
}

void MincBlockExpr::listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->castreg.listAllCasts(casts);
		for (const MincBlockExpr* ref: block->references)
			ref->castreg.listAllCasts(casts);
	}
}

size_t MincBlockExpr::countCasts() const
{
	return castreg.countCasts();
}

void MincBlockExpr::iterateCasts(std::function<void(const MincCast* cast)> cbk) const
{
	castreg.iterateCasts(cbk);
}

void MincBlockExpr::import(MincBlockExpr* importBlock)
{
	const MincBlockExpr* block;

	// Import all references of importBlock
	for (MincBlockExpr* importRef: importBlock->references)
	{
		for (block = this; block; block = block->parent)
			if (importRef == block || std::find(block->references.begin(), block->references.end(), importRef) != block->references.end())
				break;
		if (block == nullptr)
			references.insert(references.begin(), importRef);
	}

	// Import importBlock
	for (block = this; block; block = block->parent)
		if (importBlock == block || std::find(block->references.begin(), block->references.end(), importBlock) != block->references.end())
			break;
	if (block == nullptr)
		references.insert(references.begin(), importBlock);
}

void MincBlockExpr::defineSymbol(std::string name, MincObject* type, MincObject* value)
{
	if (isBuilt())
		throw CompileError("defining symbol after block has been built", loc);

	auto inserted = symbolMap.insert(std::make_pair(name, symbols.size())); // Insert forward mapping if not already present
	if (inserted.second) // If name was already present
		symbols.push_back(new MincSymbol(type, value)); // Insert symbol
	else
	{
		MincSymbol* symbol = symbols[inserted.first->second];
		symbol->type = type; // Update symbol type
		symbol->value = value; // Update symbol value
	}
	symbolNameMap[value] = name; // Insert or replace backward mapping
}

const MincSymbol* MincBlockExpr::lookupSymbol(const std::string& name) const
{
	for (const MincBlockExpr* block = this; block != nullptr; block = block->parent)
	{
		std::map<std::string, size_t>::const_iterator symbolIter;
		if ((symbolIter = block->symbolMap.find(name)) != block->symbolMap.cend())
			return block->symbols[symbolIter->second]; // Symbol found in local scope

		for (const MincBlockExpr* ref: block->references)
			if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.cend())
				return ref->symbols[symbolIter->second]; // Symbol found in ref scope
	}
	return nullptr; // Symbol not found
}

const std::string* MincBlockExpr::lookupSymbolName(const MincObject* value) const
{
	for (const MincBlockExpr* block = this; block != nullptr; block = block->parent)
	{
		std::map<const MincObject*, std::string>::const_iterator symbolIter;
		if ((symbolIter = block->symbolNameMap.find(value)) != block->symbolNameMap.cend())
			return &symbolIter->second; // Symbol found in local scope

		for (const MincBlockExpr* ref: block->references)
			if ((symbolIter = ref->symbolNameMap.find(value)) != ref->symbolNameMap.cend())
				return &symbolIter->second; // Symbol found in ref scope
	}
	return nullptr; // Symbol not found
}

const std::string& MincBlockExpr::lookupSymbolName(const MincObject* value, const std::string& defaultName) const
{
	for (const MincBlockExpr* block = this; block != nullptr; block = block->parent)
	{
		std::map<const MincObject*, std::string>::const_iterator symbolIter;
		if ((symbolIter = block->symbolNameMap.find(value)) != block->symbolNameMap.cend())
			return symbolIter->second; // Symbol found in local scope

		for (const MincBlockExpr* ref: block->references)
			if ((symbolIter = ref->symbolNameMap.find(value)) != ref->symbolNameMap.cend())
				return symbolIter->second; // Symbol found in ref scope
	}
	return defaultName; // Symbol not found
}

MincSymbolId MincBlockExpr::lookupSymbolId(const std::string& name) const
{
	MincSymbolId symbolId = { 0, 0, 0 };
	const MincBlockExpr* block = this;

	do
	{
		std::map<std::string, size_t>::const_iterator symbolIter;
		if ((symbolIter = block->symbolMap.find(name)) != block->symbolMap.cend())
		{
			symbolId.i = symbolIter->second + 1;
			return symbolId; // Symbol found in local scope
		}

		for (const MincBlockExpr* ref: block->references)
		{
			++symbolId.r;
			if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.cend())
			{
				symbolId.i = symbolIter->second + 1;
				return symbolId; // Symbol found in ref scope
			}
		}
		symbolId.r = 0;

		++symbolId.p;
		block = block->parent;
	} while (block != nullptr);

	return MincSymbolId::NONE; // Symbol not found
}
MincSymbol* MincBlockExpr::getSymbol(MincSymbolId id) const
{
	if (id.i == 0)
		return nullptr;

	const MincBlockExpr* block = this;

	while (id.p-- != 0)
		block = block->parent;

	if (id.r != 0)
		block = block->references[id.r - 1];

	return block->symbols[id.i - 1];
}

size_t MincBlockExpr::countSymbols() const
{
	return symbolMap.size();
}

void MincBlockExpr::iterateSymbols(std::function<void(const std::string& name, const MincSymbol& symbol)> cbk) const
{
	for (const std::pair<std::string, size_t>& iter: symbolMap)
		cbk(iter.first, *symbols[iter.second]);
}

MincSymbol* MincBlockExpr::importSymbol(const std::string& name)
{
	std::map<std::string, size_t>::iterator symbolIter;
	if ((symbolIter = symbolMap.find(name)) != symbolMap.end())
		return symbols[symbolIter->second]; // Symbol found in local scope

	MincSymbol* symbol;
	for (MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.end())
		{
			symbol = ref->symbols[symbolIter->second]; // Symbol found in ref scope

			if (ref->scopeType == nullptr || scopeType == nullptr)
				return symbol; // Scope type undefined for either ref scope or local scope

			const auto& key = std::pair<MincScopeType*, MincScopeType*>(ref->scopeType, scopeType);
			const auto rules = importRules.find(key);
			if (rules == importRules.end())
				return symbol; // No import rules defined from ref scope to local scope

			// Search for import rule on symbol type
			const std::map<MincObject*, ImptBlock>::iterator rule = rules->second.find(symbol->type);
			if (rule != rules->second.end())
			{
				rule->second(*symbol, ref->scopeType, scopeType); // Execute import rule
				defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
				return symbol; // Symbol and import rule found in ref scope
			}

			// Search for import rule on any base type of symbol type
			for (std::pair<MincObject* const, ImptBlock> rule: rules->second)
				if (isInstance(symbol->type, rule.first))
				{
					//TODO: Should we cast symbol to rule.first?
					rule.second(*symbol, ref->scopeType, scopeType); // Execute import rule
					defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
					return symbol; // Symbol and import rule found in ref scope
				}

			return symbol; // No import rules on symbol type defined from ref scope to local scope
		}
	
	if (parent != nullptr && (symbol = parent->importSymbol(name)))
	{
		// Symbol found in parent scope

		if (parent->scopeType == nullptr || scopeType == nullptr)
			return symbol; // Scope type undefined for either parent scope or local scope

		const auto& key = std::pair<MincScopeType*, MincScopeType*>(parent->scopeType, scopeType);
		const auto rules = importRules.find(key);
		if (rules == importRules.end())
		{
			defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
			return symbol; // No import rules defined from parent scope to local scope
		}

		// Search for import rule on symbol type
		const std::map<MincObject*, ImptBlock>::const_iterator rule = rules->second.find(symbol->type);
		if (rule != rules->second.end())
		{
			rule->second(*symbol, parent->scopeType, scopeType); // Execute import rule
			defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
			return symbol; // Symbol and import rule found in parent scope
		}

		// Search for import rule on any base type of symbol type
		for (std::pair<MincObject* const, ImptBlock> rule: rules->second)
			if (isInstance(symbol->type, rule.first))
			{
				//TODO: Should we cast symbol to rule.first?
				rule.second(*symbol, parent->scopeType, scopeType); // Execute import rule
				defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
				return symbol; // Symbol and import rule found in parent scope
			}

		defineSymbol(name, symbol->type, symbol->value); // Import symbol into local scope
		return symbol; // No import rules on symbol type defined from parent scope to local scope
	}

	return nullptr; // Symbol not found
}

const std::vector<MincSymbol>* MincBlockExpr::getBlockParams() const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		if (block->blockParams.size())
			return &block->blockParams;
		for (const MincBlockExpr* ref: block->references)
			if (ref->blockParams.size())
				return &ref->blockParams;
	}
	return nullptr;
}

MincSymbol MincBlockExpr::run(MincBlockExpr* parentBlock, bool resume)
{
	if (parentBlock == this)
		throw CompileError("block expression can't be it's own parent", this->loc);
	if (isBusy)
		throw CompileError("block expression already executing. Use MincBlockExpr::clone() when executing blocks recursively", this->loc);
	if (builtStmts->size() == 0 && exprs->size() != 0)
		throw CompileError(parentBlock, loc, "expression not built: %e", this);
	isBusy = true;
	isResuming = isBlockSuspended && (resume || parentBlock->isResuming);

	try
	{
		raiseStepEvent(this, isResuming ? STEP_RESUME : STEP_IN);
	}
	catch (...)
	{
		isBlockSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		isBusy = false;
		isResuming = false;
		throw;
	}
	if (isBlockSuspended && !isResuming)
		reset();
	isBlockSuspended = false;

	parent = parentBlock;

	MincBlockExpr* oldTopLevelBlock = topLevelBlock;
	if (parentBlock == nullptr)
	{
		parent = rootBlock;
		topLevelBlock = this;
	}

	MincBlockExpr* oldFileBlock = fileBlock;
	if (fileBlock == nullptr)
		fileBlock = this;

	try
	{
		for (; stmtIdx < builtStmts->size(); ++stmtIdx)
		{
			MincStmt& currentStmt = builtStmts->at(stmtIdx);
			if (!currentStmt.isResolved() && !lookupStmt(currentStmt.begin, exprs->end(), currentStmt))
				throw UndefinedStmtException(&currentStmt);
			currentStmt.run(this);

			// Clear cached expressions
			// Coroutines exit run() without clearing resultCache by throwing an exception
			// They use the resultCache on reentry to avoid reexecuting expressions
			resultCache.clear();
			resultCacheIdx = 0;
		}
		stmtIdx = builtStmts->size(); // Handle case `stmtIdx > builtStmts->size()`
	}
	catch (...)
	{
		resultCacheIdx = 0;

		if (topLevelBlock == this)
			topLevelBlock = oldTopLevelBlock;

		if (fileBlock == this)
			fileBlock = oldFileBlock;

		isBlockSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);

		if (isVolatile)
			forget();

		isBusy = false;
		isResuming = false;
		throw;
	}

	stmtIdx = 0;

	if (topLevelBlock == this)
		topLevelBlock = oldTopLevelBlock;

	if (fileBlock == this)
		fileBlock = oldFileBlock;

	raiseStepEvent(this, STEP_OUT);

	if (isVolatile)
		forget();

	isBusy = false;
	isResuming = false;
	return VOID;
}

MincKernel* MincBlockExpr::build(MincBlockExpr* parentBlock)
{
	//TODO: rename builtStmts to builtStmts
	if (builtStmts->size() != 0)
		return builtKernel; // Already built

	parent = parentBlock;

	MincBlockExpr* oldTopLevelBlock = topLevelBlock;
	if (parentBlock == nullptr)
	{
		parent = rootBlock;
		topLevelBlock = this;
	}

	MincBlockExpr* oldFileBlock = fileBlock;
	if (fileBlock == nullptr)
		fileBlock = this;

	// Resolve and build statements from expressions
	for (MincExprIter stmtBeginExpr = exprs->cbegin(); stmtBeginExpr != exprs->cend();)
	{
		builtStmts->push_back(MincStmt());
		MincStmt& currentStmt = builtStmts->back();
		if (!lookupStmt(stmtBeginExpr, exprs->end(), currentStmt))
			throw UndefinedStmtException(&currentStmt);
		currentStmt.build(this);

		// Advance beginning of next statement to end of current statement
		stmtBeginExpr = currentStmt.end;
	}

	if (topLevelBlock == this)
		topLevelBlock = oldTopLevelBlock;

	if (fileBlock == this)
		fileBlock = oldFileBlock;

	return builtKernel;
}

bool MincBlockExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void MincBlockExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
}

std::string MincBlockExpr::str() const
{
	if (exprs->empty())
		return "{}";

	std::string result = "{\n";
	for (auto expr: *exprs)
	{
		if (expr->exprtype == MincExpr::ExprType::STOP)
		{
			if (expr->exprtype == MincExpr::ExprType::STOP)
				result.pop_back(); // Remove ' ' before STOP string
			result += expr->str() + '\n';
		}
		else
			result += expr->str() + ' ';
	}

	size_t start_pos = 0;
	while((start_pos = result.find("\n", start_pos)) != std::string::npos && start_pos + 1 != result.size()) {
		result.replace(start_pos, 1, "\n\t");
		start_pos += 2;
	}

	return result + '}';
}

std::string MincBlockExpr::shortStr() const
{
	return "{ ... }";
}

int MincBlockExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincBlockExpr* _other = (const MincBlockExpr*)other;
	c = (int)this->exprs->size() - (int)_other->exprs->size();
	if (c) return c;
	for (std::vector<MincExpr*>::const_iterator t = this->exprs->cbegin(), o = _other->exprs->cbegin(); t != this->exprs->cend(); ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

MincExpr* MincBlockExpr::clone() const
{
	MincBlockExpr* clone = new MincBlockExpr(this->loc, this->exprs, this->builtStmts);
	clone->parent = this->parent;
	clone->references = this->references;
	clone->name = this->name;
	clone->scopeType = this->scopeType;
	clone->blockParams = this->blockParams;
	clone->symbols.reserve(this->symbols.size());
	for (MincSymbol* symbol: this->symbols)
		clone->symbols.push_back(new MincSymbol(symbol->type, symbol->value)); // Note: Cloning symbol->value is necessary to copy static
																			   //		symbols into instance blocks
	clone->symbolMap.insert(this->symbolMap.begin(), this->symbolMap.end());
	clone->symbolNameMap.insert(this->symbolNameMap.begin(), this->symbolNameMap.end());
	this->castreg.iterateCasts([&clone](const MincCast* cast) {
		clone->defineCast(const_cast<MincCast*>(cast)); //TODO: Remove const cast
	}); // Note: Cloning casts is necessary for run-time argument matching
	return clone;
}

void MincBlockExpr::reset()
{
	resultCache.clear();
	resultCacheIdx = 0;
	stmtIdx = 0;
	isBlockSuspended = false;
	isStmtSuspended = false;
	isExprSuspended = false;
}

void MincBlockExpr::clearCache(size_t targetSize)
{
	if (targetSize > resultCache.size())
		targetSize = resultCache.size();

	resultCacheIdx = targetSize;
	resultCache.erase(resultCache.begin() + targetSize, resultCache.end());
}

const MincStmt* MincBlockExpr::getCurrentStmt() const
{
	return stmtIdx < builtStmts->size() ? &builtStmts->at(stmtIdx) : nullptr;
}

MincBlockExpr* MincBlockExpr::parseCFile(const char* filename)
{
	return ::parseCFile(filename);
}

MincBlockExpr* MincBlockExpr::parseCCode(const char* code)
{
	return ::parseCCode(code);
}

const std::vector<MincExpr*> MincBlockExpr::parseCTplt(const char* tpltStr)
{
	return ::parseCTplt(tpltStr);
}

void MincBlockExpr::evalCCode(const char* code, MincBlockExpr* scope)
{
	::evalCBlock(code, scope);
}

MincBlockExpr* MincBlockExpr::parsePythonFile(const char* filename)
{
	return ::parsePythonFile(filename);
}

MincBlockExpr* MincBlockExpr::parsePythonCode(const char* code)
{
	return ::parsePythonCode(code);
}

const std::vector<MincExpr*> MincBlockExpr::parsePythonTplt(const char* tpltStr)
{
	return ::parsePythonTplt(tpltStr);
}

void MincBlockExpr::evalPythonCode(const char* code, MincBlockExpr* scope)
{
	::evalPythonBlock(code, scope);
}

extern "C"
{
	bool ExprIsBlock(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::BLOCK;
	}

	MincBlockExpr* createEmptyBlockExpr()
	{
		return new MincBlockExpr({0}, {});
	}

	MincBlockExpr* wrapExpr(MincExpr* expr)
	{
		return new MincBlockExpr(expr->loc, new std::vector<MincExpr*>(1, expr));
	}

	void defineStmt1(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineStmt(tplt, new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineStmt2(MincBlockExpr* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineStmt3(MincBlockExpr* scope, const std::vector<MincExpr*>& tplt, MincKernel* stmt)
	{
		if (!tplt.empty() && (tplt.back()->exprtype == MincExpr::ExprType::STOP
						   || (tplt.back()->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)tplt.back())->p1 == 'B')
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
		scope->defineStmt(parseCTplt(tpltStr), stmt);
	}

	void defineStmt5(MincBlockExpr* scope, const char* tpltStr, StmtBlock buildBlock, void* stmtArgs)
	{
		scope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel2(buildBlock, stmtArgs));
	}

	void defineStmt6(MincBlockExpr* scope, const char* tpltStr, StmtBlock buildBlock, StmtBlock runBlock, void* stmtArgs)
	{
		scope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel3(buildBlock, runBlock, stmtArgs));
	}

	void lookupStmtCandidates(const MincBlockExpr* scope, const MincStmt* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates)
	{
		MincListExpr stmtExprs('\0', std::vector<MincExpr*>(stmt->begin, stmt->end));
		scope->lookupStmtCandidates(&stmtExprs, candidates);
	}

	size_t countBlockExprStmts(const MincBlockExpr* expr)
	{
		return expr->countStmts();
	}

	void iterateBlockExprStmts(const MincBlockExpr* expr, std::function<void(const MincListExpr* tplt, MincKernel* stmt)> cbk)
	{
		return expr->iterateStmts(cbk);
	}

	void defineDefaultStmt2(MincBlockExpr* scope, StmtBlock codeBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(codeBlock == nullptr ? nullptr : new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt)
	{
		scope->defineDefaultStmt(stmt);
	}

	void defineDefaultStmt5(MincBlockExpr* scope, StmtBlock buildBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(buildBlock == nullptr ? nullptr : new StaticStmtKernel2(buildBlock, stmtArgs));
	}

	void defineDefaultStmt6(MincBlockExpr* scope, StmtBlock buildBlock, StmtBlock runBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(buildBlock == nullptr ? nullptr : new StaticStmtKernel3(buildBlock, runBlock, stmtArgs));
	}

	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel(codeBlock, type, exprArgs));
	}

	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel2(codeBlock, typeBlock, exprArgs));
	}

	void defineExpr5(MincBlockExpr* scope, MincExpr* tplt, MincKernel* expr)
	{
		scope->defineExpr(tplt, expr);
	}

	void defineExpr6(MincBlockExpr* scope, const char* tpltStr, MincKernel* expr)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], expr);
	}

	void defineExpr7(MincBlockExpr* scope, const char* tpltStr, ExprBlock buildBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel3(buildBlock, type, exprArgs));
	}

	void defineExpr8(MincBlockExpr* scope, const char* tpltStr, ExprBlock buildBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel4(buildBlock, typeBlock, exprArgs));
	}

	void defineExpr9(MincBlockExpr* scope, const char* tpltStr, StmtBlock buildBlock, ExprBlock runBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel5(buildBlock, runBlock, type, exprArgs));
	}

	void defineExpr10(MincBlockExpr* scope, const char* tpltStr, StmtBlock buildBlock, ExprBlock runBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel6(buildBlock, runBlock, typeBlock, exprArgs));
	}

	void lookupExprCandidates(const MincBlockExpr* scope, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates)
	{
		scope->lookupExprCandidates(expr, candidates);
	}

	size_t countBlockExprExprs(const MincBlockExpr* expr)
	{
		return expr->countExprs();
	}

	void iterateBlockExprExprs(const MincBlockExpr* expr, std::function<void(const MincExpr* tplt, MincKernel* expr)> cbk)
	{
		return expr->iterateExprs(cbk);
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

	void defineTypeCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, StmtBlock buildBlock, ExprBlock runBlock, void* castArgs)
	{
		scope->defineCast(new TypeCast(fromType, toType, new StaticExprKernel5(buildBlock, runBlock, toType, castArgs)));
	}

	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, ExprBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprKernel(codeBlock, toType, castArgs)));
	}

	void defineInheritanceCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, StmtBlock buildBlock, ExprBlock runBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprKernel5(buildBlock, runBlock, toType, castArgs)));
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

	std::string reportCasts(const MincBlockExpr* scope)
	{
		std::string report = "";
		std::list<std::pair<MincObject*, MincObject*>> casts;
		scope->listAllCasts(casts);
		for (auto& cast: casts)
			report += "\t" + scope->lookupSymbolName(cast.first, "UNKNOWN_TYPE") + " -> " + scope->lookupSymbolName(cast.second, "UNKNOWN_TYPE") + "\n";
		return report;
	}

	size_t countBlockExprCasts(const MincBlockExpr* expr)
	{
		return expr->countCasts();
	}

	void iterateBlockExprCasts(const MincBlockExpr* expr, std::function<void(const MincCast* cast)> cbk)
	{
		return expr->iterateCasts(cbk);
	}

	void importBlock(MincBlockExpr* scope, MincBlockExpr* block)
	{
		scope->import(block);
	}

	void defineSymbol(MincBlockExpr* scope, const char* name, MincObject* type, MincObject* value)
	{
		scope->defineSymbol(name, type, value);
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

	MincSymbolId lookupSymbolId(const MincBlockExpr* scope, const char* name)
	{
		return scope->lookupSymbolId(name);
	}

	MincSymbol* getSymbol(const MincBlockExpr* scope, MincSymbolId id)
	{
		return scope->getSymbol(id);
	}

	size_t countBlockExprSymbols(const MincBlockExpr* expr)
	{
		return expr->countSymbols();
	}

	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name)
	{
		return scope->importSymbol(name);
	}

	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk)
	{
		return expr->iterateSymbols(cbk);
	}

	MincBlockExpr* cloneBlockExpr(MincBlockExpr* expr)
	{
		return (MincBlockExpr*)expr->clone();
	}

	void resetBlockExpr(MincBlockExpr* expr)
	{
		expr->reset();
	}

	void resetBlockExprCache(MincBlockExpr* block, size_t targetState)
	{
		block->clearCache(targetState);
	}

	const MincStmt* getCurrentBlockExprStmt(const MincBlockExpr* expr)
	{
		return expr->getCurrentStmt();
	}

	const size_t getCurrentBlockExprStmtIndex(const MincBlockExpr* expr)
	{
		return expr->stmtIdx;
	}

	const void setCurrentBlockExprStmtIndex(MincBlockExpr* expr, size_t index)
	{
		expr->stmtIdx = index;
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

	const std::vector<MincBlockExpr*>& getBlockExprReferences(const MincBlockExpr* expr)
	{
		return expr->references;
	}

	void setBlockExprParams(MincBlockExpr* expr, std::vector<MincSymbol>& blockParams)
	{
		expr->blockParams = blockParams;
	}

	MincScopeType* getScopeType(const MincBlockExpr* scope)
	{
		return scope->scopeType;
	}

	void setScopeType(MincBlockExpr* scope, MincScopeType* scopeType)
	{
		scope->scopeType = scopeType;
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

	size_t getBlockExprCacheState(MincBlockExpr* block)
	{
		return block->resultCacheIdx;
	}

	bool isBlockExprBusy(MincBlockExpr* block)
	{
		return block->isBusy;
	}

	void removeBlockExpr(MincBlockExpr* expr)
	{
		delete expr;
	}

	const MincSymbol& getVoid()
	{
		return VOID;
	}

	MincBlockExpr* getRootScope()
	{
		return rootBlock;
	}

	MincBlockExpr* getFileScope()
	{
		return fileBlock;
	}

	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock)
	{
		const auto& key = std::pair<MincScopeType*, MincScopeType*>(fromScope, toScope);
		auto rules = importRules.find(key);
		if (rules == importRules.end())
			importRules[key] = { {symbolType, imptBlock } };
		else
			rules->second[symbolType] = imptBlock;
	}

	void registerStepEventListener(StepEvent listener, void* eventArgs)
	{
		stepEventListeners[listener] = eventArgs;
	}

	void deregisterStepEventListener(StepEvent listener)
	{
		stepEventListeners.erase(listener);
	}

	void evalCBlock(const char* code, MincBlockExpr* scope)
	{
		MincBlockExpr* block = ::parseCCode(code);
		block->run(scope);
		delete block;
	}

	void evalPythonBlock(const char* code, MincBlockExpr* scope)
	{
		MincBlockExpr* block = ::parsePythonCode(code);
		block->run(scope);
		delete block;
	}
}