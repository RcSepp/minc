#include <cstring>
#include "minc_api.hpp"

MincObject ERROR_TYPE;

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincExpr::MincExpr(const MincLocation& loc, ExprType exprtype) : loc(loc), exprtype(exprtype), isVolatile(false), resolvedKernel(nullptr)
{
}

MincExpr::~MincExpr()
{
}

MincSymbol MincExpr::codegen(MincBlockExpr* parentBlock, bool resume)
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
			raiseStepEvent(this, (resume || parentBlock->isResuming) && parentBlock->isExprSuspended ? STEP_RESUME : STEP_IN);
			var = resolvedKernel->codegen(parentBlock, resolvedParams);
		}
		catch (...)
		{
			parentBlock->isExprSuspended = true;
			//TODO: Raise error if getType() != &ERROR_TYPE
			raiseStepEvent(this, STEP_SUSPEND);
			if (isVolatile)
				forget();
			throw;
		}
		parentBlock->isExprSuspended = false;

		const MincObject *expectedType, *gotType = var.type;
		try //TODO: Make getType() noexcept
		{
			expectedType = resolvedKernel->getType(parentBlock, resolvedParams);
		}
		catch(...)
		{
			throw CompileError(("exception raised in expression type resolver: " + this->str()).c_str(), this->loc);
		}
		if (expectedType != gotType)
		{
			if (expectedType == &ERROR_TYPE)
				throw CompileError(
					("no exception raised in expression returning error type: " + this->str()).c_str(),
					this->loc
				);

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

		if (isVolatile)
			forget();

		return var;
	}
	else
		throw UndefinedExprException{this};
}

MincObject* MincExpr::getType(const MincBlockExpr* parentBlock) const
{
	try //TODO: Make getType() noexcept
	{
		return resolvedKernel ? resolvedKernel->getType(parentBlock, resolvedParams) : nullptr;
	}
	catch(...)
	{
		throw CompileError(("exception raised in expression type resolver: " + this->str()).c_str(), this->loc);
	}
}

MincObject* MincExpr::getType(MincBlockExpr* parentBlock)
{
	if (resolvedKernel == nullptr)
		return nullptr;

	MincObject* type;
	try
	{
		type = resolvedKernel->getType(parentBlock, resolvedParams);
	}
	catch(...)
	{
		throw CompileError(("exception raised in expression type resolver: " + this->str()).c_str(), this->loc);
	}
	if (type != &ERROR_TYPE)
		return type;

	// If type == &ERROR_TYPE, run codegen() to throw underlying exception
	try
	{
		resolvedKernel->codegen(parentBlock, resolvedParams);
	}
	catch(...)
	{
		throw;
	}
	throw CompileError(("no exception raised executing expression returning error type: " + this->str()).c_str(), this->loc);
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

extern "C"
{
	MincSymbol codegenExpr(MincExpr* expr, MincBlockExpr* scope)
	{
		return expr->codegen(scope, false);
	}

	MincSymbol resumeExpr(MincExpr* expr, MincBlockExpr* scope)
	{
		return expr->codegen(scope, true);
	}

	MincObject* getType1(const MincExpr* expr, const MincBlockExpr* scope)
	{
		return expr->getType(scope);
	}

	MincObject* getType2(MincExpr* expr, MincBlockExpr* scope)
	{
		return expr->getType(scope);
	}

	void collectParams(const MincBlockExpr* scope, const MincExpr* tplt, MincExpr* expr, std::vector<MincExpr*>& params)
	{
		size_t paramIdx = params.size();
		tplt->collectParams(scope, expr, params, paramIdx);
	}

	void resolveExpr(MincExpr* expr, MincBlockExpr* scope)
	{
		expr->resolve(scope);
	}

	void forgetExpr(MincExpr* expr)
	{
		expr->forget();
	}

	void setExprVolatile(MincExpr* expr, bool isVolatile)
	{
		expr->isVolatile = isVolatile;
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

	const MincLocation& getLocation(const MincExpr* expr)
	{
		return expr->loc;
	}

	const char* getExprFilename(const MincExpr* expr)
	{
		return expr->loc.filename;
	}

	unsigned getExprLine(const MincExpr* expr)
	{
		return expr->loc.begin_line;
	}

	unsigned getExprColumn(const MincExpr* expr)
	{
		return expr->loc.begin_column;
	}

	unsigned getExprEndLine(const MincExpr* expr)
	{
		return expr->loc.end_line;
	}

	unsigned getExprEndColumn(const MincExpr* expr)
	{
		return expr->loc.end_column;
	}

	MincObject* getErrorType()
	{
		return &ERROR_TYPE;
	}
}