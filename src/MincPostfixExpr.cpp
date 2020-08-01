#include "minc_api.hpp"

MincPostfixExpr::MincPostfixExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a)
	: MincExpr(loc, MincExpr::ExprType::POSTOP), op(op), a(a), opstr(opstr)
{
}

bool MincPostfixExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((MincPostfixExpr*)expr)->op == this->op && a->match(block, ((MincPostfixExpr*)expr)->a, score);
}

void MincPostfixExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((MincPostfixExpr*)expr)->a, params, paramIdx);
}

void MincPostfixExpr::resolveTypes(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		a->resolveTypes(block);
		MincExpr::resolveTypes(block);
	}
}

void MincPostfixExpr::forget()
{
	a->forget();
	MincExpr::forget();
}

std::string MincPostfixExpr::str() const
{
	return a->str() + (std::isalpha(opstr.front()) ? opstr + ' ' : opstr);
}

std::string MincPostfixExpr::shortStr() const
{
	return a->shortStr() + (std::isalpha(opstr.front()) ? opstr + ' ' : opstr);
}

int MincPostfixExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincPostfixExpr* _other = (const MincPostfixExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}

MincExpr* MincPostfixExpr::clone() const
{
	return new MincPostfixExpr(loc, op, opstr.c_str(), a->clone());
}