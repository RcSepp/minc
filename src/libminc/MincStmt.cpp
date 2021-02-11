#include "minc_api.h"
#include "minc_api.hpp"

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincStmt::MincStmt(MincExprIter exprBegin, MincExprIter exprEnd, MincKernel* kernel)
	: MincExpr(MincLocation{ exprBegin[0]->loc.filename, exprBegin[0]->loc.begin_line, exprBegin[0]->loc.begin_column, exprEnd[-1]->loc.end_line, exprEnd[-1]->loc.end_column }, MincExpr::ExprType::STMT), begin(exprBegin), end(exprEnd)
{
	resolvedKernel = kernel;
}

MincStmt::MincStmt()
	: MincExpr(MincLocation{0}, MincExpr::ExprType::STMT)
{
}

MincStmt::~MincStmt()
{
}

bool MincStmt::run(MincRuntime& runtime) const
{
	const MincBlockExpr* const parentBlock = runtime.parentBlock;

	// Handle expression caching for coroutines
#ifdef CACHE_RESULTS
	size_t resultCacheIdx;
	if (parentBlock->isResumable)
	{
		if (parentBlock->resultCacheIdx < parentBlock->resultCache.size())
		{
			if (parentBlock->resultCache[parentBlock->resultCacheIdx].second)
			{
				runtime.result = parentBlock->resultCache[parentBlock->resultCacheIdx++].first; // Return cached expression
				return false;
			}
		}
		else
		{
			assert(parentBlock->resultCacheIdx == parentBlock->resultCache.size());
			parentBlock->resultCache.push_back(std::make_pair(nullptr, false));
		}
		resultCacheIdx = parentBlock->resultCacheIdx++;
	}
#endif

	if (builtKernel == nullptr)
		throw CompileError(parentBlock, loc, "expression not built: %e", this);

	try
	{
		runtime.currentExpr = this;
		raiseStepEvent(this, (runtime.resume || parentBlock->isResuming) && parentBlock->isStmtSuspended ? STEP_RESUME : STEP_IN);
		if (builtKernel->run(runtime, resolvedParams))
		{
			runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
			parentBlock->isStmtSuspended = true;
			raiseStepEvent(this, STEP_SUSPEND);
			return true;
		}
	}
	catch (...)
	{
		runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
		parentBlock->isStmtSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		throw;
	}
	runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
	parentBlock->isStmtSuspended = false;

	// Cache expression result for coroutines
#ifdef CACHE_RESULTS
	if (parentBlock->isResumable)
	{
		parentBlock->resultCache[resultCacheIdx] = std::make_pair(getVoid().value, true);
		if (resultCacheIdx + 1 != parentBlock->resultCache.size())
		{
			parentBlock->resultCacheIdx = resultCacheIdx + 1;
			parentBlock->resultCache.erase(parentBlock->resultCache.begin() + resultCacheIdx + 1, parentBlock->resultCache.end());
		}
	}
#endif

	raiseStepEvent(this, STEP_OUT);

	runtime.result = nullptr; // Note: getVoid().value == nullptr
	return false;
}

bool MincStmt::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	assert(0);
	return false; // LCOV_EXCL_LINE
}

void MincStmt::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	assert(0);
}

void MincStmt::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		for (MincExprIter expr = begin; expr != end; ++expr)
			(*expr)->resolve(block);
		block->lookupStmt(begin, block->exprs->end(), *this);
	}
}

void MincStmt::forget()
{
	for (MincExprIter expr = begin; expr != end; ++expr)
		(*expr)->forget();
	MincExpr::forget();
}

std::string MincStmt::str() const
{
	if (begin == end)
		return "";
	std::string result = (*begin)->str();
	for (MincExprIter expr = begin; ++expr != end;)
		result += (*expr)->exprtype == MincExpr::ExprType::STOP ? (*expr)->str() : ' ' + (*expr)->str();
	return result;
}

std::string MincStmt::shortStr() const
{
	if (begin == end)
		return "";
	std::string result = (*begin)->shortStr();
	for (MincExprIter expr = begin; ++expr != end;)
		result += (*expr)->exprtype == MincExpr::ExprType::STOP ? (*expr)->shortStr() : ' ' + (*expr)->shortStr();
	return result;
}

int MincStmt::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincStmt* _other = (const MincStmt*)other;
	c = (int)(this->end - this->begin) - (int)(_other->end - _other->begin);
	if (c) return c;
	for (std::vector<MincExpr*>::const_iterator t = this->begin, o = _other->begin; t != this->end; ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

MincExpr* MincStmt::clone() const
{
	return new MincStmt(begin, end, resolvedKernel);
}

void MincStmt::evalCCode(const char* code, MincBlockExpr* scope)
{
	::evalCStmt(code, scope);
}

void MincStmt::evalPythonCode(const char* code, MincBlockExpr* scope)
{
	::evalPythonStmt(code, scope);
}

extern "C"
{
	bool ExprIsStmt(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::STMT;
	}

	void evalCStmt(const char* code, MincBlockExpr* scope)
	{
		std::vector<MincExpr*> exprs = ::parseCTplt(code);
		MincStmt stmt;
		if (!scope->lookupStmt(exprs.begin(), exprs.end(), stmt))
			throw UndefinedStmtException(&stmt);
		stmt.resolve(scope);
		MincBuildtime buildtime = { scope };
		stmt.build(buildtime);
		MincRuntime runtime(scope, false);
		stmt.run(runtime);
	}

	void evalPythonStmt(const char* code, MincBlockExpr* scope)
	{
		std::vector<MincExpr*> exprs = ::parsePythonTplt(code);
		MincBlockExpr stmt(MincLocation{"", 0, 0, 0, 0}, &exprs);
		stmt.resolve(scope);
		MincBuildtime buildtime = { scope };
		stmt.build(buildtime);
		MincRuntime runtime(scope, false);
		stmt.run(runtime);
	}
}