#include "minc_api.hpp"

void raiseStepEvent(const ExprAST* loc, StepEventType type);

ParamExprAST::ParamExprAST(const Location& loc, size_t idx)
	: ExprAST(loc, ExprAST::ExprType::PARAM), idx(idx)
{
}

Variable ParamExprAST::codegen(BlockExprAST* parentBlock)
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

	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr)
		throw CompileError("invalid use of parameter expression in parameterless scope", loc);
	if (idx >= blockParams->size())
		throw CompileError("parameter index out of bounds", loc);

	raiseStepEvent(this, STEP_OUT);

	return blockParams->at(idx);
}

MincObject* ParamExprAST::getType(const BlockExprAST* parentBlock) const
{
	const std::vector<Variable>* blockParams = parentBlock->getBlockParams();
	if (blockParams == nullptr || idx >= blockParams->size())
		return nullptr;
	return blockParams->at(idx).type;
}

bool ParamExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((ParamExprAST*)expr)->idx == this->idx;
}

void ParamExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
}

std::string ParamExprAST::str() const
{
	return '$' + std::to_string(idx);
}

int ParamExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const ParamExprAST* _other = (const ParamExprAST*)other;
	return this->idx - _other->idx;
}

ExprAST* ParamExprAST::clone() const
{
	return new ParamExprAST(loc, idx);
}