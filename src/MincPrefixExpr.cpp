#include "minc_api.hpp"

MincPrefixExpr::MincPrefixExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a)
	: MincExpr(loc, MincExpr::ExprType::PREOP), op(op), a(a), opstr(opstr)
{
}
bool MincPrefixExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((MincPrefixExpr*)expr)->op == this->op && a->match(block, ((MincPrefixExpr*)expr)->a, score);
}

void MincPrefixExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((MincPrefixExpr*)expr)->a, params, paramIdx);
}

void MincPrefixExpr::resolveTypes(const MincBlockExpr* block)
{
	a->resolveTypes(block);
	MincExpr::resolveTypes(block);
}

std::string MincPrefixExpr::str() const
{
	return (std::isalpha(opstr.back()) ? opstr + ' ' : opstr) + a->str();
}

std::string MincPrefixExpr::shortStr() const
{
	return (std::isalpha(opstr.back()) ? opstr + ' ' : opstr) + a->shortStr();
}

int MincPrefixExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincPrefixExpr* _other = (const MincPrefixExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}

MincExpr* MincPrefixExpr::clone() const
{
	return new MincPrefixExpr(loc, op, opstr.c_str(), a->clone());
}