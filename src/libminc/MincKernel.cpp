#include "minc_api.hpp"

extern MincObject ERROR_TYPE;
extern const MincSymbol VOID;
struct UnresolvableExprKernel UNRESOLVABLE_EXPR_KERNEL, UNUSED_KERNEL;
struct UnresolvableStmtKernel UNRESOLVABLE_STMT_KERNEL;

MincKernel::~MincKernel()
{
}

MincKernel* MincKernel::build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
{
	return this;
}

void MincKernel::dispose(MincKernel* kernel)
{
}

bool MincKernel::run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
{
	return false;
}

MincObject* MincKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return VOID.type;
}

bool UnresolvableExprKernel::run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
{
	throw UndefinedExprException{runtime.currentExpr};
}

MincObject* UnresolvableExprKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return &ERROR_TYPE;
}

bool UnresolvableStmtKernel::run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
{
assert(runtime.currentExpr->exprtype == MincExpr::ExprType::STMT);
	throw UndefinedStmtException{(MincStmt*)runtime.currentExpr};
}

MincObject* UnresolvableStmtKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return &ERROR_TYPE;
}