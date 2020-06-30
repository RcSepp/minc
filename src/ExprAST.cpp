#include "minc_api.hpp"

void raiseStepEvent(const ExprAST* loc, StepEventType type);

ExprAST::ExprAST(const Location& loc, ExprType exprtype) : loc(loc), exprtype(exprtype), resolvedContext(nullptr)
{
}

ExprAST::~ExprAST()
{
}

Variable ExprAST::codegen(BlockExprAST* parentBlock)
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

	if (resolvedContext)
	{
		Variable var;
		try
		{
			raiseStepEvent(this, parentBlock->isExprSuspended ? STEP_RESUME : STEP_IN);
			var = resolvedContext->codegen(parentBlock, resolvedParams);
		}
		catch (...)
		{
			parentBlock->isExprSuspended = true;
			raiseStepEvent(this, STEP_SUSPEND);
			throw;
		}
		parentBlock->isExprSuspended = false;

		const MincObject *expectedType = resolvedContext->getType(parentBlock, resolvedParams), *gotType = var.type;
		if (expectedType != gotType)
		{
			throw CompileError(
				("invalid expression return type: " + this->str() + "<" + parentBlock->lookupSymbolName(gotType, "UNKNOWN_TYPE") + ">, expected: <" + parentBlock->lookupSymbolName(expectedType, "UNKNOWN_TYPE") + ">").c_str(),
				this->loc
			);
		}

		// Cache expression result for coroutines
		parentBlock->resultCache[resultCacheIdx] = new Variable(var);

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

		return var;
	}
	else
		throw UndefinedExprException{this};
}

MincObject* ExprAST::getType(const BlockExprAST* parentBlock) const
{
	return resolvedContext ? resolvedContext->getType(parentBlock, resolvedParams) : nullptr;
}

void ExprAST::resolveTypes(const BlockExprAST* block)
{
	if (this->resolvedContext == nullptr)
		block->lookupExpr(this);
}

std::string ExprAST::shortStr() const
{
	return str();
}

int ExprAST::comp(const ExprAST* other) const
{
	return this->exprtype - other->exprtype;
}

bool operator<(const ExprAST& left, const ExprAST& right)
{
	return left.comp(&right) < 0;
}