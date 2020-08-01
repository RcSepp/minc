#include "minc_api.hpp"

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincExpr::MincExpr(const MincLocation& loc, ExprType exprtype) : loc(loc), exprtype(exprtype), resolvedKernel(nullptr)
{
}

MincExpr::~MincExpr()
{
}

MincSymbol MincExpr::codegen(MincBlockExpr* parentBlock)
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

	if (resolvedKernel)
	{
		MincSymbol var;
		try
		{
			raiseStepEvent(this, parentBlock->isExprSuspended ? STEP_RESUME : STEP_IN);
			var = resolvedKernel->codegen(parentBlock, resolvedParams);
		}
		catch (...)
		{
			parentBlock->isExprSuspended = true;
			raiseStepEvent(this, STEP_SUSPEND);
			throw;
		}
		parentBlock->isExprSuspended = false;

		const MincObject *expectedType = resolvedKernel->getType(parentBlock, resolvedParams), *gotType = var.type;
		if (expectedType != gotType)
		{
			throw CompileError(
				("invalid expression return type: " + this->str() + "<" + parentBlock->lookupSymbolName(gotType, "UNKNOWN_TYPE") + ">, expected: <" + parentBlock->lookupSymbolName(expectedType, "UNKNOWN_TYPE") + ">").c_str(),
				this->loc
			);
		}

		// Cache expression result for coroutines
		parentBlock->resultCache[resultCacheIdx] = new MincSymbol(var);

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

		return var;
	}
	else
		throw UndefinedExprException{this};
}

MincObject* MincExpr::getType(const MincBlockExpr* parentBlock) const
{
	return resolvedKernel ? resolvedKernel->getType(parentBlock, resolvedParams) : nullptr;
}

void MincExpr::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
		block->lookupExpr(this);
}

void MincExpr::forget()
{
	resolvedKernel = nullptr;
}

std::string MincExpr::shortStr() const
{
	return str();
}

int MincExpr::comp(const MincExpr* other) const
{
	return this->exprtype - other->exprtype;
}

bool operator<(const MincExpr& left, const MincExpr& right)
{
	return left.comp(&right) < 0;
}