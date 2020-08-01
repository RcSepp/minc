#include "minc_api.hpp"

MincEllipsisExpr::MincEllipsisExpr(const MincLocation& loc, MincExpr* expr)
	: MincExpr(loc, MincExpr::ExprType::ELLIPSIS), expr(expr)
{
}

bool MincEllipsisExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	score--; // Penalize ellipsis match
	return this->expr->match(block, expr->exprtype == MincExpr::ExprType::ELLIPSIS ? ((MincEllipsisExpr*)expr)->expr : expr, score);
};

void MincEllipsisExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	size_t ellipsisBegin = paramIdx;
	this->expr->collectParams(block, expr->exprtype == MincExpr::ExprType::ELLIPSIS ? ((MincEllipsisExpr*)expr)->expr : expr, params, paramIdx);

	// Replace all non-list parameters that are part of this ellipsis with single-element lists,
	// because ellipsis parameters are expected to always be lists
	for (size_t i = ellipsisBegin; i < paramIdx; ++i)
		if (params[i]->exprtype != MincExpr::ExprType::LIST)
			params[i] = new MincListExpr('\0', { params[i] });
}

void MincEllipsisExpr::resolveTypes(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		expr->resolveTypes(block);
		MincExpr::resolveTypes(block);
	}
}

void MincEllipsisExpr::forget()
{
	expr->forget();
	MincExpr::forget();
}

std::string MincEllipsisExpr::str() const
{
	return expr->str() + ", ...";
}

std::string MincEllipsisExpr::shortStr() const
{
	return expr->shortStr() + ", ...";
}

int MincEllipsisExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincEllipsisExpr* _other = (const MincEllipsisExpr*)other;
	return this->expr->comp(_other->expr);
}

MincExpr* MincEllipsisExpr::clone() const
{
	return new MincEllipsisExpr(loc, expr->clone());
}