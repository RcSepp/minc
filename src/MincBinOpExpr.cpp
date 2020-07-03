#include "minc_api.hpp"

MincBinOpExpr::MincBinOpExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a, MincExpr* b)
	: MincExpr(loc, MincExpr::ExprType::BINOP), op(op), a(a), b(b), opstr(opstr), a_post(a->loc, op, opstr, a), b_pre(b->loc, op, opstr, b)
{
}

bool MincBinOpExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((MincBinOpExpr*)expr)->op == this->op && a->match(block, ((MincBinOpExpr*)expr)->a, score) && b->match(block, ((MincBinOpExpr*)expr)->b, score);
}

void MincBinOpExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((MincBinOpExpr*)expr)->a, params, paramIdx);
	b->collectParams(block, ((MincBinOpExpr*)expr)->b, params, paramIdx);
}

void MincBinOpExpr::resolveTypes(const MincBlockExpr* block)
{
	a->resolveTypes(block);
	b->resolveTypes(block);
	MincExpr::resolveTypes(block);
	a_post.resolveTypes(block);
	b_pre.resolveTypes(block);
}

std::string MincBinOpExpr::str() const
{
	return a->str() + " " + opstr + " " + b->str();
}

std::string MincBinOpExpr::shortStr() const
{
	return a->shortStr() + " " + opstr + " " + b->shortStr();
}

int MincBinOpExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincBinOpExpr* _other = (const MincBinOpExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	c = this->a->comp(_other->a);
	if (c) return c;
	return this->b->comp(_other->b);
}

MincExpr* MincBinOpExpr::clone() const
{
	return new MincBinOpExpr(loc, op, opstr.c_str(), a->clone(), b->clone());
}