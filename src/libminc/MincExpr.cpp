#include <cstring>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>
#include "minc_api.h"
#include "minc_api.hpp"

MincObject ERROR_TYPE, NONE_TYPE;

void raiseStepEvent(const MincExpr* loc, StepEventType type);

template<class T> class AtomicCounter
{
	mutable std::mutex mutex;
	T count;

public:
	AtomicCounter(size_t initial) : count(initial) {}
	operator T() const { return count; }
	T operator--(int)
	{
		mutex.lock();
		count--;
		mutex.unlock();
		return count;
	}
	void wait(T target) const
	{
		mutex.lock();
		while (count != target)
		{
			mutex.unlock();
			mutex.lock();
		}
		mutex.unlock();
	}
};

MincExpr::MincExpr(const MincLocation& loc, ExprType exprtype)
	: loc(loc), exprtype(exprtype), resolvedKernel(nullptr), resolvedType(&NONE_TYPE), builtKernel(nullptr)
{
}

MincExpr::~MincExpr()
{
}

bool MincExpr::run(MincRuntime& runtime) const
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
			return true;
		}
	}
	catch (...)
	{
		runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
		parentBlock->isExprSuspended = true;
		//TODO: Raise error if getType() != &ERROR_TYPE
		raiseStepEvent(this, STEP_SUSPEND);
		throw;
	}
	runtime.parentBlock = parentBlock; // Restore runtime.parentBlock
	parentBlock->isExprSuspended = false;

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
		MincInteropData interopData;
		MincRuntime runtime(parentBlock, parentBlock->isResuming, interopData);
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

		// Update runners
		MincRunner& runner = resolvedKernel->runner;
		if (buildtime.runners.insert(&runner).second) // If this is the first kernel with this runner
		{
			if (buildtime.currentRunner == nullptr) // If this is the first kernel of the build
				buildtime.currentRunner = &runner; // Set current runner to expression runner, because execution will start with the first runner
			runner.buildBegin(buildtime);
		}
		if (buildtime.currentFileRunners != nullptr && buildtime.currentFileRunners->insert(&runner).second) // If this is the first kernel in this file
			runner.buildBeginFile(buildtime, loc.filename);

		// Build expression
		MincRunner* const currentRunner = buildtime.currentRunner;
		if (&runner != currentRunner)
		{
			currentRunner->buildSuspendExpr(buildtime, this);
			buildtime.currentRunner = &runner;
			runner.buildNestedExpr(buildtime, this, *currentRunner);
			buildtime.currentRunner = currentRunner;
			currentRunner->buildResumeExpr(buildtime, this);
		}
		else
			runner.buildExpr(buildtime, this);
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

MincExpr* MincExpr::getSourceExpr()
{
	return exprtype == ExprType::CAST ? ((MincCastExpr*)this)->getSourceExpr() : this;
}

MincExpr* MincExpr::getDerivedExpr()
{
	return exprtype == ExprType::CAST ? ((MincCastExpr*)this)->getDerivedExpr() : this;
}

int MincExpr::exec(MincBuildtime& buildtime)
{
	if (buildtime.runners.size() == 1)
	{
		MincInteropData interopData;
		return (*buildtime.runners.begin())->run(this, interopData);
	}

	std::vector<std::thread> runnerThreads;

	// Execute runners
	struct RunArgs
	{
		// Input
		MincExpr* expr;
		MincBuildtime& buildtime;
		MincInteropData interopData;
		AtomicCounter<size_t> numRunnersStarting;

		// Output
		std::variant<std::monostate, MincException, std::exception, MincSymbol, MincObject*> err;
		int exitCode;

		// State
		bool quit;
	} args = {
		this,
		buildtime,
		MincInteropData(),
		buildtime.runners.size(),
		std::monostate(),
		0,
		false,
	};
	for (MincRunner* runner: buildtime.runners)
		runnerThreads.push_back(std::thread([](RunArgs* args, MincRunner* runner) {
			// Initially lock runner
			mtx_lock(&runner->mutex);
			args->numRunnersStarting--;
			mtx_lock(&runner->mutex);

			// Run expr
			int exitCode = 0;
			try
			{
				exitCode = runner->run(args->expr, args->interopData);
			} catch (const MincException& err) {
				args->err.emplace<MincException>(err);
			} catch (const std::exception& err) {
				args->err.emplace<std::exception>(err);
			} catch (const MincSymbol& err) {
				args->err.emplace<MincSymbol>(err);
			} catch (MincObject* err) {
				args->err.emplace<MincObject*>(err);
			}

			if (args->quit == false) // If this is the first thread to finish
			{
				args->quit = true;
				args->exitCode = exitCode;

				// Quit other runners
				args->interopData.followupExpr = nullptr;
				for (MincRunner* otherRunner: args->buildtime.runners)
					if (otherRunner != runner)
						mtx_unlock(&otherRunner->mutex);
			}
		}, &args, runner));

	// Wait until all threads have started
	args.numRunnersStarting.wait(0);

	// Unlock thread of first expression
	MincRunner& firstRunner = exprtype == ExprType::BLOCK ? ((MincBlockExpr*)this)->builtStmts.front()->resolvedKernel->runner : resolvedKernel->runner;
	mtx_unlock(&firstRunner.mutex);

	// Wait for all runners to finish
	for (std::thread& thread: runnerThreads)
		thread.join();

	// Rethrow exceptions
	switch (args.err.index())
	{
	case 1: throw std::get<1>(args.err);
	case 2: throw std::get<2>(args.err);
	case 3: throw std::get<3>(args.err);
	case 4: throw std::get<4>(args.err);
	}
	return args.exitCode;
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
		if (&expr->resolvedKernel->runner != &MINC_INTERPRETER)
			throw CompileError("Only interpreted expressions can be evaluated");
		MincInteropData interopData;
		MincRuntime runtime(scope, false, interopData);
		return MincSymbol(expr->getType(scope), expr->run(runtime) ? nullptr : runtime.result);
	}

	MincSymbol evalPythonExpr(const char* code, MincBlockExpr* scope)
	{
		MincExpr* expr = MincBlockExpr::parsePythonTplt(code)[0];
		expr->resolve(scope);
		MincBuildtime buildtime = { scope };
		expr->build(buildtime);
		if (&expr->resolvedKernel->runner != &MINC_INTERPRETER)
			throw CompileError("Only interpreted expressions can be evaluated");
		MincInteropData interopData;
		MincRuntime runtime(scope, false, interopData);
		return MincSymbol(expr->getType(scope), expr->run(runtime) ? nullptr : runtime.result);
	}
}