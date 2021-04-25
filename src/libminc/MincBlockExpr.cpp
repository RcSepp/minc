#include <sstream>
#include "minc_api.h"
#include "minc_api.hpp"

#define DETECT_UNDEFINED_TYPE_CASTS

extern const MincSymbol VOID = MincSymbol(new MincObject(), nullptr);
MincBlockExpr* const rootBlock = new MincBlockExpr({0}, {});
MincBlockExpr* fileBlock = nullptr;
std::map<std::pair<MincScopeType*, MincScopeType*>, std::map<MincObject*, ImptBlock>> importRules;
std::map<StepEvent, void*> stepEventListeners;

struct StaticStmtKernel : public MincKernel
{
private:
	RunBlock cbk;
	void* stmtArgs;
public:
	StaticStmtKernel(RunBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return cbk(runtime, params, stmtArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticStmtKernel2 : public MincKernel
{
private:
	BuildBlock cbk;
	void* stmtArgs;
public:
	StaticStmtKernel2(BuildBlock cbk, void* stmtArgs = nullptr) : cbk(cbk), stmtArgs(stmtArgs) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		cbk(buildtime, params, stmtArgs);
		return this;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return false;
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticStmtKernel3 : public MincKernel
{
private:
	BuildBlock buildCbk;
	RunBlock runCbk;
	void* stmtArgs;
public:
	StaticStmtKernel3(BuildBlock buildCbk, RunBlock runCbk, void* stmtArgs = nullptr) : buildCbk(buildCbk), runCbk(runCbk), stmtArgs(stmtArgs) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		buildCbk(buildtime, params, stmtArgs);
		return this;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return runCbk(runtime, params, stmtArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return getVoid().type;
	}
};
struct StaticExprKernel : public MincKernel
{
private:
	RunBlock cbk;
	MincObject* const type;
	void* exprArgs;
public:
	StaticExprKernel(RunBlock cbk, MincObject* type, void* exprArgs = nullptr) : cbk(cbk), type(type), exprArgs(exprArgs) {}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return cbk(runtime, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel2 : public MincKernel
{
private:
	RunBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprKernel2(RunBlock cbk, ExprTypeBlock typecbk, void* exprArgs = nullptr) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs) {}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return cbk(runtime, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct StaticExprKernel3 : public MincKernel
{
private:
	BuildBlock cbk;
	MincObject* const type;
	MincObject* const value;
	void* exprArgs;
public:
	StaticExprKernel3(BuildBlock cbk, MincObject* type, MincObject* value, void* exprArgs = nullptr) : cbk(cbk), type(type), value(value), exprArgs(exprArgs) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		cbk(buildtime, params, exprArgs);
		return new StaticExprKernel3(cbk, buildtime.result.type = type, buildtime.result.value, exprArgs);
	}
	void dispose(MincKernel* kernel)
	{
		delete kernel;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		runtime.result = value;
		return false;
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel4 : public MincKernel
{
private:
	BuildBlock cbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
	MincObject* const value;
public:
	StaticExprKernel4(BuildBlock cbk, ExprTypeBlock typecbk, void* exprArgs, MincObject* value=nullptr) : cbk(cbk), typecbk(typecbk), exprArgs(exprArgs), value(value) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		cbk(buildtime, params, exprArgs);
		return new StaticExprKernel4(cbk, typecbk, exprArgs, buildtime.result.value);
	}
	void dispose(MincKernel* kernel)
	{
		delete kernel;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		runtime.result = value;
		return false;
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};
struct StaticExprKernel5 : public MincKernel
{
private:
	BuildBlock buildCbk;
	RunBlock runCbk;
	MincObject* const type;
	void* exprArgs;
public:
	StaticExprKernel5(BuildBlock buildCbk, RunBlock runCbk, MincObject* type, void* exprArgs = nullptr) : buildCbk(buildCbk), runCbk(runCbk), type(type), exprArgs(exprArgs) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		buildCbk(buildtime, params, exprArgs);
		buildtime.result.type = type;
		return this;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return runCbk(runtime, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return type;
	}
};
struct StaticExprKernel6 : public MincKernel
{
private:
	BuildBlock buildCbk;
	RunBlock runCbk;
	ExprTypeBlock typecbk;
	void* exprArgs;
public:
	StaticExprKernel6(BuildBlock buildCbk, RunBlock runCbk, ExprTypeBlock typecbk, void* exprArgs) : buildCbk(buildCbk), runCbk(runCbk), typecbk(typecbk), exprArgs(exprArgs) {}
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		buildCbk(buildtime, params, exprArgs);
		return this;
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		return runCbk(runtime, params, exprArgs);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return typecbk(parentBlock, params, exprArgs);
	}
};

MincOpaqueCastKernel::MincOpaqueCastKernel(MincObject* type)
	: type(type)
{
}
MincKernel* MincOpaqueCastKernel::build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
{
	params[0]->build(buildtime);
	buildtime.result.type = type;
	return this;
}
bool MincOpaqueCastKernel::run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
{
	return params[0]->run(runtime);
}
MincObject* MincOpaqueCastKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return type;
}

void raiseStepEvent(const MincExpr* loc, StepEventType type)
{
	for (const std::pair<StepEvent, void*>& listener: stepEventListeners)
		listener.first(loc, type, listener.second);
}

MincBlockExpr::MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs)
	: MincExpr(loc, MincExpr::ExprType::BLOCK), defaultStmtKernel(nullptr), defaultExprKernel(nullptr), castreg(this), stackSize(0), parent(nullptr), exprs(exprs),
	  scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isResuming(false), isBusy(false), user(nullptr), userType(nullptr)
{
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, MincKernel* stmt, MincBlockExpr* scope)
{
	if (scope == nullptr)
		scope = this;
	if (scope->isBuilt())
		throw CompileError("defining statement after block has been built", loc);

	for (MincExpr* tpltExpr: tplt)
		tpltExpr->resolve(this);
	scope->stmtreg.defineStmt(new MincListExpr('\0', tplt), stmt, scope, this);
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincBlockExpr* scope)
{
	struct StmtKernel : public MincKernel
	{
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		StmtKernel(std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run)
			: runCbk(run) {}
		virtual ~StmtKernel() {}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) { return runCbk(runtime, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return VOID.type; }
	};
	defineStmt(tplt, new StmtKernel(run), scope);
}

void MincBlockExpr::defineStmt(const std::vector<MincExpr*>& tplt,
							   std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
							   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincBlockExpr* scope)
{
	struct StmtKernel : public MincKernel
	{
		const std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> buildCbk;
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		StmtKernel(std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
				   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run)
			: buildCbk(build), runCbk(run) {}
		virtual ~StmtKernel() {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params) { if (buildCbk != nullptr) buildCbk(buildtime, params); return this; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) { if (runCbk != nullptr) return runCbk(runtime, params); return false; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return VOID.type; }
	};
	defineStmt(tplt, new StmtKernel(build, run), scope);
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
	// if (stmtIdx + 1 == builtStmts.size()) // If the current statement is the last statement
	// 									   // This avoids forgetting statements during consecutive iterations of already resolved blocks.
	// 	// Forget expressions beyond the current statement
	// 	for (MincExprIter expr = builtStmts.back().end; expr != exprs->end() && (*expr)->isResolved(); ++expr)
	// 		(*expr)->forget();
	//TODO: This doesn't forget future expressions for child blocks of the define target
	// For example test_mutable_exprs() will fail, because an expression is defined in file scope, while an already resolved expression in
	// test_mutable_exprs-scope is affected by the redefine.
	// A possible solution would be to remember the last define in the target block and check if parent blocks had recently been updated
	// during build.
	// Until such a solution has been implemented, MincBlockExpr::lookupStmt() forgets future expressions after every resolved statement.
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincObject* type)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		MincObject* const type;
		ExprKernel(std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincObject* type)
			: runCbk(run), type(type) {}
		virtual ~ExprKernel() {}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return runCbk(runtime, params);
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return type;
		}
	};
	defineExpr(tplt, new ExprKernel(run, type));
}

void MincBlockExpr::defineExpr(MincExpr* tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		const std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getTypeCbk;
		ExprKernel(std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
			: runCbk(run), getTypeCbk(getType) {}
		virtual ~ExprKernel() {}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) { return runCbk(runtime, params); }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getTypeCbk(parentBlock, params); }
	};
	defineExpr(tplt, new ExprKernel(run, getType));
}

void MincBlockExpr::defineExpr(MincExpr* tplt,
							   std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
							   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run,
							   MincObject* type)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> buildCbk;
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		MincObject* const type;
		ExprKernel(std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
				   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run,
				   MincObject* type)
			: buildCbk(build), runCbk(run), type(type) {}
		virtual ~ExprKernel() {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params) { if (buildCbk != nullptr) buildCbk(buildtime, params); return this; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) { if (runCbk != nullptr) return runCbk(runtime, params); return false; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return type; }
	};
	defineExpr(tplt, new ExprKernel(build, run, type));
}

void MincBlockExpr::defineExpr(MincExpr* tplt,
							   std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
							   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run,
							   std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
{
	if (isBuilt())
		throw CompileError("defining expression after block has been built", loc);

	struct ExprKernel : public MincKernel
	{
		const std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> buildCbk;
		const std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> runCbk;
		const std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getTypeCbk;
		ExprKernel(std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build,
				   std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run,
				   std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> getType)
			: buildCbk(build), runCbk(run), getTypeCbk(getType) {}
		virtual ~ExprKernel() {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params) { if (buildCbk != nullptr) buildCbk(buildtime, params); return this; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) { if (runCbk != nullptr) return runCbk(runtime, params); return false; }
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getTypeCbk(parentBlock, params); }
	};
	defineExpr(tplt, new ExprKernel(build, run, getType));
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
	if (lookupSymbolName(cast->fromType) == nullptr || lookupSymbolName(cast->toType) == nullptr)
		throw CompileError("type-cast defined from " + lookupSymbolName(cast->fromType, "UNKNOWN_TYPE") + " to " + lookupSymbolName(cast->toType, "UNKNOWN_TYPE"));
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
	if (derivedType == baseType)
		return true;
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

void MincBlockExpr::iterateBases(MincObject* derivedType, std::function<void(MincObject* baseType)> cbk) const
{
	for (const MincBlockExpr* block = this; block; block = block->parent)
	{
		block->castreg.iterateBases(derivedType, cbk);
		for (const MincBlockExpr* ref: block->references)
			ref->castreg.iterateBases(derivedType, cbk);
	}
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

	auto inserted = symbolMap.insert(std::make_pair(name, SymbolMapEntry(symbols.size(), SymbolType::BUILDTIME))); // Insert forward mapping if not already present
	if (inserted.second || // If name refers to a new symbol or ...
		inserted.first->second.type != SymbolType::BUILDTIME || // ... existing symbol is a stack symbol or ...
		type != symbols[inserted.first->second.index]->type) // ... existing symbol is of a different type
	{
		// Insert new symbol
		inserted.first->second.index = symbols.size();
		inserted.first->second.type = SymbolType::BUILDTIME;
		symbols.push_back(new MincSymbol(type, value)); // Insert symbol
	}
	else
	{
		// Update existing symbol
		MincSymbol* symbol = symbols[inserted.first->second.index];
		symbol->type = type; // Update symbol type
		symbol->value = value; // Update symbol value
	}
	symbolNameMap[value] = name; // Insert or replace backward mapping
}

const MincSymbol* MincBlockExpr::lookupSymbol(const std::string& name) const
{
	for (const MincBlockExpr* block = this; block != nullptr; block = block->parent)
	{
		std::map<std::string, SymbolMapEntry>::const_iterator symbolIter;
		if ((symbolIter = block->symbolMap.find(name)) != block->symbolMap.cend())
			return symbolIter->second.type == SymbolType::BUILDTIME ? block->symbols[symbolIter->second.index] : nullptr; // Symbol found in local scope

		for (const MincBlockExpr* ref: block->references)
			if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.cend())
				return symbolIter->second.type == SymbolType::BUILDTIME ? ref->symbols[symbolIter->second.index] : nullptr; // Symbol found in ref scope
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

size_t MincBlockExpr::countSymbols() const
{
	return symbolMap.size();
}

size_t MincBlockExpr::countSymbols(SymbolType symbolType) const
{
	size_t c = 0;
	for (const std::pair<std::string, SymbolMapEntry>& iter: symbolMap)
		c += iter.second.type == symbolType;
	return c;
}

void MincBlockExpr::iterateBuildtimeSymbols(std::function<void(const std::string& name, const MincSymbol& symbol)> cbk) const
{
	for (const std::pair<std::string, SymbolMapEntry>& iter: symbolMap)
		if (iter.second.type == SymbolType::BUILDTIME)
			cbk(iter.first, *symbols[iter.second.index]);
}

void MincBlockExpr::iterateStackSymbols(std::function<void(const std::string& name, const MincStackSymbol& symbol)> cbk) const
{
	for (const std::pair<std::string, SymbolMapEntry>& iter: symbolMap)
		if (iter.second.type == SymbolType::STACK)
			cbk(iter.first, *stackSymbols[iter.second.index]);
}

MincSymbol* MincBlockExpr::importSymbol(const std::string& name)
{
	std::map<std::string, SymbolMapEntry>::iterator symbolIter;
	if ((symbolIter = symbolMap.find(name)) != symbolMap.end())
		return symbolIter->second.type == SymbolType::BUILDTIME ? symbols[symbolIter->second.index] : nullptr; // Symbol found in local scope

	MincSymbol* symbol;
	for (MincBlockExpr* ref: references)
		if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.end())
		{
			if (symbolIter->second.type != SymbolType::BUILDTIME)
				continue;
			symbol = ref->symbols[symbolIter->second.index]; // Symbol found in ref scope

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

const MincStackSymbol* MincBlockExpr::allocStackSymbol(const std::string& name, MincObject* type, size_t size)
{
	if (size == 0)
		throw CompileError("defining symbol of size 0", loc);
	if (isBuilt())
		throw CompileError("defining symbol after block has been built", loc);

	auto inserted = symbolMap.insert(std::make_pair(name, SymbolMapEntry(stackSymbols.size(), SymbolType::STACK))); // Insert stack symbol if not already present
	if (inserted.second) // If name refers to a new symbol
	{
		// Insert new symbol
		stackSymbols.push_back(new MincStackSymbol(type, this, stackSize)); // Insert symbol
		stackSize += size; // Grow stack
	}
	else if (inserted.first->second.type != SymbolType::STACK || // If the existing symbol is a build time symbol or ...
			 type != stackSymbols[inserted.first->second.index]->type) // ... the existing symbol is of a different type
	{
		// Replace symbol
		symbolMap[name] = SymbolMapEntry(stackSymbols.size(), SymbolType::STACK); // Replace symbol in symbol map
		stackSymbols.push_back(new MincStackSymbol(type, this, stackSize)); // Insert symbol
		stackSize += size; // Grow stack
	}

	return stackSymbols[inserted.first->second.index];
}

const MincStackSymbol* MincBlockExpr::allocStackSymbol(MincObject* type, size_t size)
{
	if (isBuilt())
		throw CompileError("defining symbol after block has been built", loc);

	MincStackSymbol* stackSymbol = new MincStackSymbol(type, this, stackSize);
	stackSymbols.push_back(stackSymbol); // Insert symbol
	stackSize += size; // Grow stack
	return stackSymbol;
}

const MincStackSymbol* MincBlockExpr::lookupStackSymbol(const std::string& name) const
{
	for (const MincBlockExpr* block = this; block != nullptr; block = block->parent)
	{
		std::map<std::string, SymbolMapEntry>::const_iterator symbolIter;
		if ((symbolIter = block->symbolMap.find(name)) != block->symbolMap.cend())
			return symbolIter->second.type == SymbolType::STACK ? block->stackSymbols[symbolIter->second.index] : nullptr; // Symbol found in local scope

		for (const MincBlockExpr* ref: block->references)
			if ((symbolIter = ref->symbolMap.find(name)) != ref->symbolMap.cend())
				return symbolIter->second.type == SymbolType::STACK ? ref->stackSymbols[symbolIter->second.index] : nullptr; // Symbol found in ref scope
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

MincEnteredBlockExpr::MincEnteredBlockExpr(MincRuntime& runtime, const MincBlockExpr* block)
	: runtime(runtime), block(block), prevStackFrame(block->stackFrame) // Store previous stack frame
{
	if (block->isResumable) // If this is resumable block, ...
	{
		if ((block->stackFrame = runtime.heapFrame) == nullptr) // Make sure a heap frame was provided
			throw CompileError("No heap frame passed to resumable block");
		if (block->stackFrame->heapPointer == nullptr) // If the heap frame is unallocated, ...
		{
			// Allocate heap frame
			block->stackFrame->heapPointer = new unsigned char[block->stackSize];
			block->stackFrame->stmtIndex = 0;
			block->stackFrame->next = new MincStackFrame();
		}
		runtime.heapFrame = block->stackFrame->next;
	}
	else // If this is regular block, ...
	{
		// Reserve a frame from the stack
		block->stackFrame = &runtime.stackFrames[runtime.currentStackPointerIndex++]; // Get stack pointer for this block
		block->stackFrame->stackPointer = runtime.currentStackSize; // Set stack pointer for this block to the bottom of the stack
		runtime.currentStackSize += block->stackSize; // Grow the stack by this block's stack size

		if (runtime.currentStackSize >= runtime.stackSize) //TODO: Replace hardcoded stack size
			throw CompileError("Stack overflow", block->loc);
	}
}
MincEnteredBlockExpr::~MincEnteredBlockExpr()
{
	if (block != nullptr)
		exit();
}

void MincEnteredBlockExpr::exit()
{
	if (block == nullptr)
		return;

	if (!block->isResumable)
	{
		runtime.currentStackSize -= block->stackSize; // Shrink the stack by this block's stack size
		--runtime.currentStackPointerIndex; // Release stack pointer for this block
	}
	block->stackFrame = prevStackFrame; // Restore previous stack frame (for recursion)

	block = nullptr;
}

bool MincEnteredBlockExpr::run()
{
	if (runtime.parentBlock == block)
		throw CompileError("block expression can't be it's own parent", block->loc);
	if (block->builtStmts.size() == 0 && block->exprs->size() != 0)
		throw CompileError(runtime.parentBlock, block->loc, "expression not built: %e", block);
	block->isBusy = true;
	block->isResuming = block->isBlockSuspended && (runtime.resume || runtime.parentBlock->isResuming);

	try
	{
		raiseStepEvent(block, block->isResuming ? STEP_RESUME : STEP_IN);
	}
	catch (...)
	{
		block->isBlockSuspended = true;
		raiseStepEvent(block, STEP_SUSPEND);
		block->isBusy = false;
		block->isResuming = false;
		runtime.resume = false;
		throw;
	}
	if (block->isBlockSuspended && !block->isResuming)
		block->reset();
	block->isBlockSuspended = false;

	if (!block->isResumable)
		block->stackFrame->stmtIndex = 0;

	try
	{
		const size_t numBuiltStmts = block->builtStmts.size();
		for (; block->stackFrame->stmtIndex < numBuiltStmts; ++block->stackFrame->stmtIndex)
		{
			const MincStmt& currentStmt = block->builtStmts.at(block->stackFrame->stmtIndex);
			if (!currentStmt.isResolved())
				throw UndefinedStmtException(&currentStmt);
			runtime.parentBlock = block; // Set parent of block expr statement to block expr
			if (currentStmt.run(runtime))
			{
				block->resultCacheIdx = 0;

				runtime.parentBlock = block->parent; // Restore parent

				block->isBlockSuspended = true;
				raiseStepEvent(block, STEP_SUSPEND);

				block->isBusy = false;
				block->isResuming = false;
				runtime.resume = false;
				return true;
			}

			// Clear cached expressions
#ifdef CACHE_RESULTS
			// Coroutines exit run() without clearing resultCache by throwing an exception
			// They use the resultCache on reentry to avoid reexecuting expressions
			block->resultCache.clear();
			block->resultCacheIdx = 0;
#endif
		}
	}
	catch (...)
	{
		block->resultCacheIdx = 0;

		runtime.parentBlock = block->parent; // Restore parent

		block->isBlockSuspended = true;
		raiseStepEvent(block, STEP_SUSPEND);

		block->isBusy = false;
		block->isResuming = false;
		runtime.resume = false;
		throw;
	}

	runtime.parentBlock = block->parent; // Restore parent

	raiseStepEvent(block, STEP_OUT);

	block->isBusy = false;
	block->isResuming = false;
	runtime.resume = false;
	runtime.result = VOID.value;
	return false;
}

bool MincBlockExpr::run(MincRuntime& runtime) const
{
	MincEnteredBlockExpr entered(runtime, this);
	return entered.run();
}

MincSymbol& MincBlockExpr::build(MincBuildtime& buildtime)
{
	if (exprs == nullptr || // If empty or ...
		builtStmts.size() != 0) // ... if already built
		return buildtime.result = MincSymbol(nullptr, nullptr);

	isBusy = true;
	parent = buildtime.parentBlock;

	if (buildtime.parentBlock == nullptr)
		parent = rootBlock;

	MincBlockExpr* oldFileBlock = fileBlock;
	if (fileBlock == nullptr)
		fileBlock = this;

	// Resolve and build statements from expressions
	for (MincExprIter stmtBeginExpr = exprs->cbegin(); stmtBeginExpr != exprs->cend();)
	{
		builtStmts.push_back(MincStmt());
		MincStmt& currentStmt = builtStmts.back();

		// Resolve statement
		if (!lookupStmt(stmtBeginExpr, exprs->end(), currentStmt))
		{
			buildtime.parentBlock = parent; // Restore parent
			isBusy = false;

			if (fileBlock == this)
				fileBlock = oldFileBlock;

			throw UndefinedStmtException(&currentStmt);
		}

		// Build statement
		buildtime.parentBlock = this; // Set parent of block expr statement to block expr
		if (buildtime.settings.maxErrors == 0)
		{
			try
			{
				currentStmt.build(buildtime);
			}
			catch (...)
			{
				buildtime.parentBlock = parent; // Restore parent
				isBusy = false;

				if (fileBlock == this)
					fileBlock = oldFileBlock;

				throw;
			}
		}
		else
			try
			{
				currentStmt.build(buildtime);
			}
			catch (const CompileError& err)
			{
				if (buildtime.outputs.errors.size() >= buildtime.settings.maxErrors)
				{
					buildtime.parentBlock = parent; // Restore parent
					isBusy = false;

					if (fileBlock == this)
						fileBlock = oldFileBlock;

					throw TooManyErrorsException(err.loc);
				}
				buildtime.outputs.errors.push_back(err);
			}
			catch (...)
			{
				buildtime.parentBlock = parent; // Restore parent
				isBusy = false;

				if (fileBlock == this)
					fileBlock = oldFileBlock;

				throw;
			}

		// Advance beginning of next statement to end of current statement
		stmtBeginExpr = currentStmt.end;
	}

	buildtime.parentBlock = parent; // Restore parent
	isBusy = false;

	if (fileBlock == this)
		fileBlock = oldFileBlock;

	return buildtime.result = MincSymbol(nullptr, nullptr);
}

bool MincBlockExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void MincBlockExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	expr->resolvedKernel = &UNUSED_KERNEL;
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
	MincBlockExpr* clone = new MincBlockExpr(this->loc, this->exprs);
	clone->stackSize = this->stackSize;
	clone->parent = this->parent;
	clone->references = this->references;
	clone->isResumable = this->isResumable;
	clone->name = this->name;
	clone->scopeType = this->scopeType;
	clone->blockParams = this->blockParams;
	clone->symbols = this->symbols;
	clone->stackSymbols = this->stackSymbols;
	clone->symbolMap = this->symbolMap;
	clone->symbolNameMap = this->symbolNameMap;
	this->castreg.iterateCasts([&clone](const MincCast* cast) {
		clone->defineCast(const_cast<MincCast*>(cast)); //TODO: Remove const cast
	}); // Note: Cloning casts is necessary for run-time argument matching
	return clone;
}

void MincBlockExpr::reset() const
{
#ifdef CACHE_RESULTS
	resultCache.clear();
	resultCacheIdx = 0;
#endif
	isBlockSuspended = false;
	isStmtSuspended = false;
	isExprSuspended = false;
}

void MincBlockExpr::clearCache(size_t targetSize) const
{
#ifdef CACHE_RESULTS
	if (targetSize > resultCache.size())
		targetSize = resultCache.size();

	resultCacheIdx = targetSize;
	resultCache.erase(resultCache.begin() + targetSize, resultCache.end());
#endif
}

MincFlavor MincBlockExpr::flavorFromFile(const std::string& filename)
{
	return ::flavorFromFile(filename.c_str());
}

MincFlavor MincBlockExpr::flavorFromFile(const char* filename)
{
	return ::flavorFromFile(filename);
}

MincBlockExpr* MincBlockExpr::parseStream(std::istream& stream, MincFlavor flavor)
{
	return ::parseStream(stream, flavor);
}

MincBlockExpr* MincBlockExpr::parseFile(const char* filename, MincFlavor flavor)
{
	return ::parseFile(filename, flavor);
}

MincBlockExpr* MincBlockExpr::parseCode(const char* code, MincFlavor flavor)
{
	return ::parseCode(code, flavor);
}

const std::vector<MincExpr*> MincBlockExpr::parseTplt(const char* tpltStr, MincFlavor flavor)
{
	return ::parseTplt(tpltStr, flavor);
}

MincBlockExpr* MincBlockExpr::parseCStream(std::istream& stream)
{
	return ::parseCStream(stream);
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

MincBlockExpr* MincBlockExpr::parsePythonStream(std::istream& stream)
{
	return ::parsePythonStream(stream);
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

MincBlockExpr* MincBlockExpr::parseGoStream(std::istream& stream)
{
	return ::parseGoStream(stream);
}

MincBlockExpr* MincBlockExpr::parseGoFile(const char* filename)
{
	return ::parseGoFile(filename);
}

MincBlockExpr* MincBlockExpr::parseGoCode(const char* code)
{
	return ::parseGoCode(code);
}

const std::vector<MincExpr*> MincBlockExpr::parseGoTplt(const char* tpltStr)
{
	return ::parseGoTplt(tpltStr);
}

void MincBlockExpr::evalGoCode(const char* code, MincBlockExpr* scope)
{
	::evalGoBlock(code, scope);
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

	void defineStmt1(MincBlockExpr* refScope, const std::vector<MincExpr*>& tplt, RunBlock codeBlock, void* stmtArgs, MincBlockExpr* scope)
	{
		refScope->defineStmt(tplt, new StaticStmtKernel(codeBlock, stmtArgs), scope);
	}

	void defineStmt2(MincBlockExpr* refScope, const char* tpltStr, RunBlock codeBlock, void* stmtArgs, MincBlockExpr* scope)
	{
		refScope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel(codeBlock, stmtArgs), scope);
	}

	void defineStmt3(MincBlockExpr* refScope, const std::vector<MincExpr*>& tplt, MincKernel* stmt, MincBlockExpr* scope)
	{
		if (!tplt.empty() && (tplt.back()->exprtype == MincExpr::ExprType::STOP
						   || (tplt.back()->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)tplt.back())->p1 == 'B')
						   || (tplt.back()->exprtype == MincExpr::ExprType::LIST && ((MincListExpr*)tplt.back())->size() == 1
							   && ((MincListExpr*)tplt.back())->at(0)->exprtype == MincExpr::ExprType::PLCHLD && ((MincPlchldExpr*)((MincListExpr*)tplt.back())->at(0))->p1 == 'B')))
			refScope->defineStmt(tplt, stmt, scope);
		else
		{
			std::vector<MincExpr*> stoppedTplt(tplt);
			stoppedTplt.push_back(new MincStopExpr(MincLocation{}));
			refScope->defineStmt(stoppedTplt, stmt, scope);
		}
	}

	void defineStmt4(MincBlockExpr* refScope, const char* tpltStr, MincKernel* stmt, MincBlockExpr* scope)
	{
		refScope->defineStmt(parseCTplt(tpltStr), stmt, scope);
	}

	void defineStmt5(MincBlockExpr* refScope, const char* tpltStr, BuildBlock buildBlock, void* stmtArgs, MincBlockExpr* scope)
	{
		refScope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel2(buildBlock, stmtArgs), scope);
	}

	void defineStmt6(MincBlockExpr* refScope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, void* stmtArgs, MincBlockExpr* scope)
	{
		refScope->defineStmt(parseCTplt(tpltStr), new StaticStmtKernel3(buildBlock, runBlock, stmtArgs), scope);
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

	void defineDefaultStmt2(MincBlockExpr* scope, RunBlock codeBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(codeBlock == nullptr ? nullptr : new StaticStmtKernel(codeBlock, stmtArgs));
	}

	void defineDefaultStmt3(MincBlockExpr* scope, MincKernel* stmt)
	{
		scope->defineDefaultStmt(stmt);
	}

	void defineDefaultStmt5(MincBlockExpr* scope, BuildBlock buildBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(buildBlock == nullptr ? nullptr : new StaticStmtKernel2(buildBlock, stmtArgs));
	}

	void defineDefaultStmt6(MincBlockExpr* scope, BuildBlock buildBlock, RunBlock runBlock, void* stmtArgs)
	{
		scope->defineDefaultStmt(buildBlock == nullptr ? nullptr : new StaticStmtKernel3(buildBlock, runBlock, stmtArgs));
	}

	void defineExpr2(MincBlockExpr* scope, const char* tpltStr, RunBlock codeBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel(codeBlock, type, exprArgs));
	}

	void defineExpr3(MincBlockExpr* scope, const char* tpltStr, RunBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
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

	void defineExpr7(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel3(buildBlock, type, nullptr, exprArgs));
	}

	void defineExpr8(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel4(buildBlock, typeBlock, exprArgs));
	}

	void defineExpr9(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, MincObject* type, void* exprArgs)
	{
		scope->defineExpr(parseCTplt(tpltStr)[0], new StaticExprKernel5(buildBlock, runBlock, type, exprArgs));
	}

	void defineExpr10(MincBlockExpr* scope, const char* tpltStr, BuildBlock buildBlock, RunBlock runBlock, ExprTypeBlock typeBlock, void* exprArgs)
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

	void defineDefaultExpr2(MincBlockExpr* scope, RunBlock codeBlock, MincObject* type, void* exprArgs)
	{
		scope->defineDefaultExpr(codeBlock == nullptr ? nullptr : new StaticExprKernel(codeBlock, type, exprArgs));
	}

	void defineDefaultExpr3(MincBlockExpr* scope, RunBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs)
	{
		scope->defineDefaultExpr(codeBlock == nullptr ? nullptr : new StaticExprKernel2(codeBlock, typeBlock, exprArgs));
	}

	void defineDefaultExpr5(MincBlockExpr* scope, MincKernel* expr)
	{
		scope->defineDefaultExpr(expr);
	}

	void defineTypeCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, RunBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new TypeCast(fromType, toType, new StaticExprKernel(codeBlock, toType, castArgs)));
	}

	void defineTypeCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast)
	{
		scope->defineCast(new TypeCast(fromType, toType, cast));
	}

	void defineTypeCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, BuildBlock buildBlock, RunBlock runBlock, void* castArgs)
	{
		scope->defineCast(new TypeCast(fromType, toType, new StaticExprKernel5(buildBlock, runBlock, toType, castArgs)));
	}

	void defineInheritanceCast2(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, RunBlock codeBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprKernel(codeBlock, toType, castArgs)));
	}

	void defineInheritanceCast3(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, MincKernel* cast)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, cast));
	}

	void defineInheritanceCast9(MincBlockExpr* scope, MincObject* fromType, MincObject* toType, BuildBlock buildBlock, RunBlock runBlock, void* castArgs)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new StaticExprKernel5(buildBlock, runBlock, toType, castArgs)));
	}

	void defineOpaqueTypeCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new TypeCast(fromType, toType, new MincOpaqueCastKernel(toType)));
	}

	void defineOpaqueInheritanceCast(MincBlockExpr* scope, MincObject* fromType, MincObject* toType)
	{
		scope->defineCast(new InheritanceCast(fromType, toType, new MincOpaqueCastKernel(toType)));
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
		return scope->isInstance(fromType, toType);
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

	void iterateBases(const MincBlockExpr* expr, MincObject* derivedType, std::function<void(MincObject* baseType)> cbk)
	{
		expr->iterateBases(derivedType, cbk);
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

	size_t countBlockExprSymbols(const MincBlockExpr* expr)
	{
		return expr->countSymbols();
	}

	size_t countBlockExprBuildtimeSymbols(const MincBlockExpr* expr)
	{
		return expr->countSymbols(MincBlockExpr::SymbolType::BUILDTIME);
	}

	size_t countBlockExprStackSymbols(const MincBlockExpr* expr)
	{
		return expr->countSymbols(MincBlockExpr::SymbolType::STACK);
	}

	MincSymbol* importSymbol(MincBlockExpr* scope, const char* name)
	{
		return scope->importSymbol(name);
	}

	void iterateBlockExprSymbols(const MincBlockExpr* expr, std::function<void(const std::string& name, const MincSymbol& symbol)> cbk)
	{
		return expr->iterateBuildtimeSymbols(cbk);
	}

	const MincStackSymbol* allocStackSymbol(MincBlockExpr* scope, const char* name, MincObject* type, size_t size)
	{
		return scope->allocStackSymbol(name, type, size);
	}

	const MincStackSymbol* allocAnonymousStackSymbol(MincBlockExpr* scope, MincObject* type, size_t size)
	{
		while (!scope->isBusy)
			scope = scope->parent;
		return scope->allocStackSymbol(type, size);
	}

	const MincStackSymbol* lookupStackSymbol(const MincBlockExpr* scope, const char* name)
	{
		return scope->lookupStackSymbol(name);
	}

	MincObject* getStackSymbol(MincRuntime& runtime, const MincStackSymbol* stackSymbol)
	{
		return runtime.getStackSymbol(stackSymbol);
	}

	MincObject* getStackSymbolOfNextStackFrame(MincRuntime& runtime, const MincStackSymbol* stackSymbol)
	{
		return runtime.getStackSymbolOfNextStackFrame(stackSymbol);
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

	void addBlockExprReference(MincBlockExpr* expr, MincBlockExpr* reference)
	{
		if (reference == expr)
			throw CompileError("a scope cannot reference itself", expr->loc);
		expr->references.push_back(reference);
	}

	void clearBlockExprReferences(MincBlockExpr* expr)
	{
		expr->references.clear();
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

	bool isResumable(MincBlockExpr* block)
	{
		return block->isResumable;
	}

	void setResumable(MincBlockExpr* block, bool resumable)
	{
		if (block->isBuilt())
			throw CompileError("setting resumable after block has been built"); //TODO: Check this from C++ API

		block->isResumable = resumable;
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
		MincBuildtime buildtime = { scope };
		block->build(buildtime);
		MincRuntime runtime(scope, false);
		block->run(runtime);
		delete block;
	}

	void evalPythonBlock(const char* code, MincBlockExpr* scope)
	{
		MincBlockExpr* block = ::parsePythonCode(code);
		MincBuildtime buildtime = { scope };
		block->build(buildtime);
		MincRuntime runtime(scope, false);
		block->run(runtime);
		delete block;
	}

	void evalGoBlock(const char* code, MincBlockExpr* scope)
	{
		MincBlockExpr* block = ::parseGoCode(code);
		MincBuildtime buildtime = { scope };
		block->build(buildtime);
		MincRuntime runtime(scope, false);
		block->run(runtime);
		delete block;
	}

	MincEnteredBlockExpr* enterBlockExpr(MincRuntime& runtime, const MincBlockExpr* block)
	{
		return new MincEnteredBlockExpr(runtime, block);
	}
	void exitBlockExpr(MincEnteredBlockExpr* enteredBlock)
	{
		delete enteredBlock;
	}
}