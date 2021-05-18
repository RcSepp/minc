#include "minc_api.hpp"

void MincRunner::buildStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	stmt->resolvedKernel->build(buildtime, stmt->resolvedParams);
}

void MincRunner::buildExpr(MincBuildtime& buildtime, MincExpr* expr)
{
	expr->resolvedKernel->build(buildtime, expr->resolvedParams);
}

void MincRunner::buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next)
{
	expr->resolvedKernel->build(buildtime, expr->resolvedParams);
}

void MincRunner::handover(MincRunner& next)
{
	if (mtx_trylock(&next.mutex) == thrd_success)
		{ do { mtx_unlock(&next.mutex); } while (mtx_trylock(&next.mutex) == thrd_success); }
	else
		mtx_unlock(&next.mutex);
	mtx_lock(&mutex);
}
