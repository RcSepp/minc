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

MincSymbol MincStmt::codegen(MincBlockExpr* parentBlock)
{
	// Handle expression caching for coroutines
	if (parentBlock->resultCacheIdx < parentBlock->resultCache.size())
	{
		if (parentBlock->resultCache[parentBlock->resultCacheIdx])
			return *parentBlock->resultCache[parentBlock->resultCacheIdx++]; // Return cached expression
	}
	else
	{
		assert(parentBlock->resultCacheIdx == parentBlock->resultCache.size());
		parentBlock->resultCache.push_back(nullptr);
	}
	size_t resultCacheIdx = parentBlock->resultCacheIdx++;

	try
	{
		raiseStepEvent(this, parentBlock->isStmtSuspended ? STEP_RESUME : STEP_IN);
		resolvedKernel->codegen(parentBlock, resolvedParams);
	}
	catch (...)
	{
		parentBlock->isStmtSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		throw;
	}
	parentBlock->isStmtSuspended = false;

	// Cache expression result for coroutines
	parentBlock->resultCache[resultCacheIdx] = new MincSymbol(getVoid());

assert(resultCacheIdx <= parentBlock->resultCache.size()); //TODO: Testing hypothesis
//TODO: If this hypothesis stays true, then the following delete-loop and erase() can be replaced with a delete if-block and pop_back()!
	for (std::vector<MincSymbol*>::iterator cachedResult = parentBlock->resultCache.begin() + resultCacheIdx + 1; cachedResult != parentBlock->resultCache.end(); ++cachedResult)
	{
		--parentBlock->resultCacheIdx;
		if (*cachedResult)
			delete *cachedResult;
	}
	parentBlock->resultCache.erase(parentBlock->resultCache.begin() + resultCacheIdx + 1, parentBlock->resultCache.end());

	raiseStepEvent(this, STEP_OUT);

	return getVoid();
}

bool MincStmt::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	assert(0);
	return false; // Unreachable
}

void MincStmt::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	assert(0);
}

void MincStmt::resolveTypes(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		for (MincExprIter expr = begin; ++expr != end;)
			(*expr)->resolveTypes(block);
		block->lookupStmt(begin, *this);
	}
}

void MincStmt::forget()
{
	for (MincExprIter expr = begin; ++expr != end;)
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