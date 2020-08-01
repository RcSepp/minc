#include "minc_api.hpp"

void raiseStepEvent(const MincExpr* loc, StepEventType type);

MincParamExpr::MincParamExpr(const MincLocation& loc, size_t idx)
	: MincExpr(loc, MincExpr::ExprType::PARAM), idx(idx)
{
}

MincSymbol MincParamExpr::codegen(MincBlockExpr* parentBlock)
{
	try
	{
		raiseStepEvent(this, parentBlock->isExprSuspended ? STEP_RESUME : STEP_IN);
	}
	catch (...)
	{
		parentBlock->isExprSuspended = true;
		raiseStepEvent(this, STEP_SUSPEND);
		throw;
	}
	parentBlock->isExprSuspended = false;

	const std::vector<MincSymbol>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr)
		throw CompileError("invalid use of parameter expression in parameterless scope", loc);
	if (idx >= blockParams->size())
		throw CompileError("parameter index out of bounds", loc);

	raiseStepEvent(this, STEP_OUT);

	return blockParams->at(idx);
}

MincObject* MincParamExpr::getType(const MincBlockExpr* parentBlock) const
{
	const std::vector<MincSymbol>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr || idx >= blockParams->size())
		return nullptr;
	return blockParams->at(idx).type;
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