#include "minc_api.hpp"

void raiseStepEvent(const ExprAST* loc, StepEventType type);

StmtAST::StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context)
	: ExprAST(Location{ exprBegin[0]->loc.filename, exprBegin[0]->loc.begin_line, exprBegin[0]->loc.begin_column, exprEnd[-1]->loc.end_line, exprEnd[-1]->loc.end_column }, ExprAST::ExprType::STMT), begin(exprBegin), end(exprEnd)
{
	resolvedContext = context;
}

StmtAST::StmtAST()
	: ExprAST(Location{0}, ExprAST::ExprType::STMT)
{
}

StmtAST::~StmtAST()
{
}

Variable StmtAST::codegen(BlockExprAST* parentBlock)
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
		resolvedContext->codegen(parentBlock, resolvedParams);
	}
	catch (...)
	{
		parentBlock->isStmtSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		throw;
	}
	parentBlock->isStmtSuspended = false;

	// Cache expression result for coroutines
	parentBlock->resultCache[resultCacheIdx] = new Variable(getVoid());

assert(resultCacheIdx <= parentBlock->resultCache.size()); //TODO: Testing hypothesis
//TODO: If this hypothesis stays true, then the following delete-loop and erase() can be replaced with a delete if-block and pop_back()!
	for (std::vector<Variable*>::iterator cachedResult = parentBlock->resultCache.begin() + resultCacheIdx + 1; cachedResult != parentBlock->resultCache.end(); ++cachedResult)
	{
		--parentBlock->resultCacheIdx;
		if (*cachedResult)
			delete *cachedResult;
	}
	parentBlock->resultCache.erase(parentBlock->resultCache.begin() + resultCacheIdx + 1, parentBlock->resultCache.end());

	raiseStepEvent(this, STEP_OUT);

	return getVoid();
}

bool StmtAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	assert(0);
	return false; // Unreachable
}

void StmtAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	assert(0);
}

void StmtAST::resolveTypes(const BlockExprAST* block)
{
	assert(0);
}

std::string StmtAST::str() const
{
	if (begin == end)
		return "";
	std::string result = (*begin)->str();
	for (ExprASTIter expr = begin; ++expr != end;)
		result += (*expr)->exprtype == ExprAST::ExprType::STOP ? (*expr)->str() : ' ' + (*expr)->str();
	return result;
}

std::string StmtAST::shortStr() const
{
	if (begin == end)
		return "";
	std::string result = (*begin)->shortStr();
	for (ExprASTIter expr = begin; ++expr != end;)
		result += (*expr)->exprtype == ExprAST::ExprType::STOP ? (*expr)->shortStr() : ' ' + (*expr)->shortStr();
	return result;
}

int StmtAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const StmtAST* _other = (const StmtAST*)other;
	c = (int)(this->end - this->begin) - (int)(_other->end - _other->begin);
	if (c) return c;
	for (std::vector<ExprAST*>::const_iterator t = this->begin, o = _other->begin; t != this->end; ++t, ++o)
	{
		c = (*t)->comp(*o);
		if (c) return c;
	}
	return 0;
}

ExprAST* StmtAST::clone() const
{
	return new StmtAST(begin, end, resolvedContext);
}