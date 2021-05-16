#include "minc_api.hpp"
#include "minc_cli.h"

class SuspendExprKernel : public MincKernel
{
private:
	MincExpr* expr;

public:
	SuspendExprKernel(MincExpr* expr) : MincKernel(&MINC_INTERPRETER), expr(expr) { assert(&expr->resolvedKernel->runner != &runner); }
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		runtime.interopData.followupExpr = expr;
		mtx_t* nextMutex = &expr->resolvedKernel->runner.mutex;

		if (mtx_trylock(nextMutex) == thrd_success)
			{ do { mtx_unlock(nextMutex); } while (mtx_trylock(nextMutex) == thrd_success); }
		else
			mtx_unlock(nextMutex);
		mtx_lock(&MINC_INTERPRETER.mutex);

		if (runtime.interopData.followupExpr != nullptr)
			runtime.interopData.followupExpr->run(runtime);

		runtime.result = runtime.interopData.exprResult;
		return false;
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return expr->getType(parentBlock);
	}
};

class SuspendStmtKernel : public MincKernel
{
private:
	MincRunner& next;

public:
	SuspendStmtKernel(MincRunner& next) : MincKernel(&MINC_INTERPRETER), next(next) { assert(&next != &runner); }
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		runtime.interopData.followupExpr = nullptr;

		if (mtx_trylock(&next.mutex) == thrd_success)
			{ do { mtx_unlock(&next.mutex); } while (mtx_trylock(&next.mutex) == thrd_success); }
		else
			mtx_unlock(&next.mutex);
		mtx_lock(&MINC_INTERPRETER.mutex);

		if (runtime.interopData.followupExpr != nullptr)
			runtime.interopData.followupExpr->run(runtime);

		runtime.result = runtime.interopData.exprResult;
		return false;
	}
};

class NestedExprKernel : public MincKernel
{
private:
	MincKernel* nestedKernel;
	MincRunner& next;
	MincBlockExpr* parentBlock;

public:
	NestedExprKernel(MincKernel* nestedKernel, MincRunner& next, MincBlockExpr* parentBlock)
		: MincKernel(&MINC_INTERPRETER), nestedKernel(nestedKernel), next(next), parentBlock(parentBlock) { assert(&next != &runner); }
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		runtime.parentBlock = parentBlock;
		if (parentBlock->stackFrame == nullptr)
		{
			//TODO: Multiple nested expressions within the same parent reenter the parent on every execution
			//		Implement logic to keep the parent open instead.
			MincEnteredBlockExpr entered(runtime, parentBlock);
			if (nestedKernel->run(runtime, params))
				return true;
		}
		else
		{
			if (nestedKernel->run(runtime, params))
				return true;
		}
		runtime.interopData.followupExpr = nullptr;
		runtime.interopData.exprResult = runtime.result;

		if (mtx_trylock(&next.mutex) == thrd_success)
			{ do { mtx_unlock(&next.mutex); } while (mtx_trylock(&next.mutex) == thrd_success); }
		else
			mtx_unlock(&next.mutex);
		mtx_lock(&MINC_INTERPRETER.mutex);

		if (runtime.interopData.followupExpr != nullptr)
			runtime.interopData.followupExpr->run(runtime);

		return false;
	}
};

MincInterpreter::MincInterpreter() : MincRunner("Interpreter")
{
}

void MincInterpreter::buildStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	stmt->builtKernel = stmt->resolvedKernel->build(buildtime, stmt->resolvedParams);
	buildtime.parentBlock->runStmts.push_back(stmt);
}

void MincInterpreter::buildSuspendStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	stmt->builtKernel = new SuspendStmtKernel(stmt->resolvedKernel->runner);
	buildtime.parentBlock->runStmts.push_back(stmt);
}

void MincInterpreter::buildExpr(MincBuildtime& buildtime, MincExpr* expr)
{
	expr->builtKernel = expr->resolvedKernel->build(buildtime, expr->resolvedParams);
}

void MincInterpreter::buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next)
{
	MincKernel* builtKernel = expr->resolvedKernel->build(buildtime, expr->resolvedParams);
	expr->builtKernel = new NestedExprKernel(builtKernel, next, buildtime.parentBlock);
}

void MincInterpreter::buildSuspendExpr(MincBuildtime& buildtime, MincExpr* expr)
{
	expr->builtKernel = new SuspendExprKernel(expr);
}

int MincInterpreter::run(MincExpr* expr, MincInteropData& interopData)
{
	MincRuntime runtime(nullptr, false, interopData);
	try {
		if (expr->run(runtime))
			throw runtime.exceptionType;
	} catch (const ExitException& err) {
		return err.code;
	}
	return 0;
}

MincInterpreter MINC_INTERPRETER;