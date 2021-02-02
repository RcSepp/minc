#include <cstring>
#include "minc_api.h"
#include "minc_api.hpp"

//#define CHECK_RUN_RESULT_TYPES

MincObject ERROR_TYPE, NONE_TYPE;

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincExpr::MincExpr(const MincLocation& loc, ExprType exprtype)
	: loc(loc), exprtype(exprtype), isVolatile(false), resolvedKernel(nullptr), resolvedType(&NONE_TYPE), builtKernel(nullptr)
{
}

MincExpr::~MincExpr()
{
}

bool MincExpr::run(MincRuntime& runtime)
{
	MincBlockExpr* const parentBlock = runtime.parentBlock;

	// Handle expression caching for coroutines
#ifdef CACHE_RESULTS
	size_t resultCacheIdx;
	if (parentBlock->isResumable)
	{
		if (parentBlock->resultCacheIdx < parentBlock->resultCache.size())
		{
			if (parentBlock->resultCache[parentBlock->resultCacheIdx].second)
			{
				const MincSymbol& cached = parentBlock->resultCache[parentBlock->resultCacheIdx++].first; // Return cached expression
				runtime.result.type = cached.type;
				runtime.result.value = cached.value;
				return false;
			}
		}
		else
		{
			assert(parentBlock->resultCacheIdx == parentBlock->resultCache.size());
			parentBlock->resultCache.push_back(std::make_pair(MincSymbol(), false));
		}
		resultCacheIdx = parentBlock->resultCacheIdx++;
	}
#endif

	if (!isResolved())
		throw UndefinedExprException{this};

	if (builtKernel == nullptr)
		throw CompileError(parentBlock, loc, "expression not built: %e", this);

	try
	{
		runtime.currentExpr = this;
		raiseStepEvent(this, (runtime.resume || parentBlock->isResuming) && parentBlock->isExprSuspended ? STEP_RESUME : STEP_IN);
		if (builtKernel->run(runtime, resolvedParams))
		{
			runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
			parentBlock->isExprSuspended = true;
			//TODO: Raise error if getType() != &ERROR_TYPE
			raiseStepEvent(this, STEP_SUSPEND);
			if (isVolatile)
				forget();
			return true;
		}
	}
	catch (...)
	{
		runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
		parentBlock->isExprSuspended = true;
		//TODO: Raise error if getType() != &ERROR_TYPE
		raiseStepEvent(this, STEP_SUSPEND);
		if (isVolatile)
			forget();
		throw;
	}
	runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
	parentBlock->isExprSuspended = false;

#ifdef CHECK_RUN_RESULT_TYPES
	const MincObject *expectedType, *gotType = runtime.result.type;
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
#endif

	// Cache expression result for coroutines
#ifdef CACHE_RESULTS
	if (parentBlock->isResumable)
	{
		parentBlock->resultCache[resultCacheIdx] = std::make_pair(runtime.result, true);
		if (resultCacheIdx + 1 != parentBlock->resultCache.size())
		{
			parentBlock->resultCacheIdx = resultCacheIdx + 1;
			parentBlock->resultCache.erase(parentBlock->resultCache.begin() + resultCacheIdx + 1, parentBlock->resultCache.end());
		}
	}
#endif

	raiseStepEvent(this, STEP_OUT);

	if (isVolatile)
		forget();

	return false;
}

//TODO: Replace getType(const MincBlockExpr*), getType(MincBlockExpr*) with getType(MincBlockExpr*), getType(MincBuildtime&), getType(MincRuntime&)

MincObject* MincExpr::getType(const MincBlockExpr* parentBlock) const
{
	if (resolvedType != &NONE_TYPE && resolvedType != &ERROR_TYPE)
		return resolvedType;
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
	if (resolvedType != &NONE_TYPE && resolvedType != &ERROR_TYPE)
		return resolvedType;
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

	// If type == &ERROR_TYPE, call run() to throw underlying exception
	if (builtKernel == nullptr)
	{
		MincBuildtime buildtime = { parentBlock };
		builtKernel = resolvedKernel->build(buildtime, resolvedParams);
	}
	try
	{
		MincRuntime runtime(parentBlock, parentBlock->isResuming);
		runtime.currentExpr = this;
		if (builtKernel->run(runtime, resolvedParams))
			throw runtime.result;
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
	resolvedType = &NONE_TYPE;
}

MincSymbol& MincExpr::build(MincBuildtime& buildtime)
{
	if (!isResolved())
		throw UndefinedExprException{this};

	buildtime.result = MincSymbol(nullptr, nullptr);
	if (!isBuilt())
	{
		resolvedType = resolvedKernel->getType(buildtime.parentBlock, resolvedParams);
		builtKernel = resolvedKernel->build(buildtime, resolvedParams);
	}
	buildtime.result.type = resolvedType;
	return buildtime.result;
}

std::string MincExpr::shortStr() const
{
	return str();
}

int MincExpr::comp(const MincExpr* other) const
{
	return this->exprtype - other->exprtype;
}

MincSymbol MincExpr::evalCCode(const char* code, MincBlockExpr* scope)
{
	return ::evalCExpr(code, scope);
}

MincSymbol MincExpr::evalPythonCode(const char* code, MincBlockExpr* scope)
{
	return ::evalPythonExpr(code, scope);
}

bool operator<(const MincExpr& left, const MincExpr& right)
{
	return left.comp(&right) < 0;
}

extern "C"
{
	bool runExpr(MincExpr* expr, MincRuntime& runtime)
	{
		return expr->run(runtime);
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

	MincSymbol& buildExpr(MincExpr* expr, MincBuildtime& buildtime)
	{
		return expr->build(buildtime);
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

	MincExpr* cloneExpr(const MincExpr* expr)
	{
		return expr->clone();
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

	MincSymbol evalCExpr(const char* code, MincBlockExpr* scope)
	{
		MincExpr* expr = MincBlockExpr::parseCTplt(code)[0];
		expr->resolve(scope);
		MincBuildtime buildtime = { scope };
		expr->build(buildtime);
		MincRuntime runtime(scope, false);
		return expr->run(runtime) ? MincSymbol(nullptr, nullptr) : runtime.result;
	}

	MincSymbol evalPythonExpr(const char* code, MincBlockExpr* scope)
	{
		MincExpr* expr = MincBlockExpr::parsePythonTplt(code)[0];
		expr->resolve(scope);
		MincBuildtime buildtime = { scope };
		expr->build(buildtime);
		MincRuntime runtime(scope, false);
		return expr->run(runtime) ? MincSymbol(nullptr, nullptr) : runtime.result;
	}
}