#include "minc_api.hpp"

extern MincObject ERROR_TYPE;

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincParamExpr::Kernel::Kernel(MincParamExpr* expr)
	: expr(expr)
{
}

MincSymbol MincParamExpr::Kernel::codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
{
	const std::vector<MincSymbol>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr)
		throw CompileError("invalid use of parameter expression in parameterless scope", expr->loc);
	if (expr->idx >= blockParams->size())
		throw CompileError("parameter index out of bounds", expr->loc);
	return blockParams->at(expr->idx);
}

MincObject* MincParamExpr::Kernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	const std::vector<MincSymbol>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr || expr->idx >= blockParams->size())
		return &ERROR_TYPE;
	return blockParams->at(expr->idx).type;
}

MincParamExpr::MincParamExpr(const MincLocation& loc, size_t idx)
	: MincExpr(loc, MincExpr::ExprType::PARAM), kernel(this), idx(idx)
{
	resolvedKernel = &kernel; //TODO: Make customizable
}

bool MincParamExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((MincParamExpr*)expr)->idx == this->idx;
}

void MincParamExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
}

std::string MincParamExpr::str() const
{
	return '$' + std::to_string(idx);
}

int MincParamExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincParamExpr* _other = (const MincParamExpr*)other;
	return this->idx - _other->idx;
}

MincExpr* MincParamExpr::clone() const
{
	return new MincParamExpr(loc, idx);
}

extern "C"
{
	bool ExprIsParam(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::PARAM;
	}
}