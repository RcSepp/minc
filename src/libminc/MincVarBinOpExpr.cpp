#include "minc_api.hpp"

MincVarBinOpExpr::MincVarBinOpExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a)
	: MincExpr(loc, MincExpr::ExprType::VARBINOP), op(op), a(a), opstr(opstr)
{
}

bool MincVarBinOpExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	if (expr->exprtype == this->exprtype)
		return ((MincVarBinOpExpr*)expr)->op == this->op && a->match(block, ((MincVarBinOpExpr*)expr)->a, score);
	else if (expr->exprtype == MincExpr::ExprType::BINOP)
		return ((MincBinOpExpr*)expr)->op == this->op && this->match(block, ((MincBinOpExpr*)expr)->a, score) && this->match(block, ((MincBinOpExpr*)expr)->b, score);
	else
		return a->match(block, expr, score);
}

void MincVarBinOpExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	size_t paramBegin = paramIdx;
	if (expr->exprtype == this->exprtype)
		a->collectParams(block, ((MincVarBinOpExpr*)expr)->a, params, paramIdx);
	else if (expr->exprtype == MincExpr::ExprType::BINOP)
	{
		size_t& paramIdx1 = paramIdx, paramIdx2 = paramIdx;
		this->collectParams(block, ((MincBinOpExpr*)expr)->a, params, paramIdx1);
		this->collectParams(block, ((MincBinOpExpr*)expr)->b, params, paramIdx2);
		paramIdx = paramIdx1 > paramIdx2 ? paramIdx1 : paramIdx2;
	}
	else
		a->collectParams(block, expr, params, paramIdx);

	// Replace all non-list parameters within this MincVarBinOpExpr with single-element lists,
	// because ellipsis parameters are expected to always be lists
	for (size_t i = paramBegin; i < paramIdx; ++i)
		if (params[i]->exprtype != MincExpr::ExprType::LIST)
			params[i] = new MincListExpr('\0', { params[i] });
}

void MincVarBinOpExpr::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		a->resolve(block);
		MincExpr::resolve(block);
	}
}

void MincVarBinOpExpr::forget()
{
	a->forget();
	MincExpr::forget();
}

std::string MincVarBinOpExpr::str() const
{
	return a->str() + " " + opstr + " ...";
}

std::string MincVarBinOpExpr::shortStr() const
{
	return a->shortStr() + " " + opstr + " ...";
}

int MincVarBinOpExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincVarBinOpExpr* _other = (const MincVarBinOpExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}
MincExpr* MincVarBinOpExpr::clone() const
{
	return new MincVarBinOpExpr(loc, op, opstr.c_str(), a->clone());
}