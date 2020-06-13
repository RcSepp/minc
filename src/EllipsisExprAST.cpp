#include "minc_api.hpp"

EllipsisExprAST::EllipsisExprAST(const Location& loc, ExprAST* expr)
	: ExprAST(loc, ExprAST::ExprType::ELLIPSIS), expr(expr)
{
}

bool EllipsisExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	score--; // Penalize ellipsis match
	return this->expr->match(block, expr->exprtype == ExprAST::ExprType::ELLIPSIS ? ((EllipsisExprAST*)expr)->expr : expr, score);
};

void EllipsisExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	size_t ellipsisBegin = paramIdx;
	this->expr->collectParams(block, expr->exprtype == ExprAST::ExprType::ELLIPSIS ? ((EllipsisExprAST*)expr)->expr : expr, params, paramIdx);

	// Replace all non-list parameters that are part of this ellipsis with single-element lists,
	// because ellipsis parameters are expected to always be lists
	for (size_t i = ellipsisBegin; i < paramIdx; ++i)
		if (params[i]->exprtype != ExprAST::ExprType::LIST)
			params[i] = new ListExprAST('\0', { params[i] });
}

void EllipsisExprAST::resolveTypes(const BlockExprAST* block)
{
	expr->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string EllipsisExprAST::str() const
{
	return expr->str() + ", ...";
}

int EllipsisExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const EllipsisExprAST* _other = (const EllipsisExprAST*)other;
	return this->expr->comp(_other->expr);
}

ExprAST* EllipsisExprAST::clone() const
{
	return new EllipsisExprAST(loc, expr->clone());
}