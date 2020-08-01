#include "minc_api.hpp"

MincEncOpExpr::MincEncOpExpr(const MincLocation& loc, int op, const char* oopstr, const char* copstr, MincExpr* val)
	: MincExpr(loc, MincExpr::ExprType::ENCOP), op(op), val(val), oopstr(oopstr), copstr(copstr)
{
}

bool MincEncOpExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((MincEncOpExpr*)expr)->op == this->op
		&& val->match(block, ((MincEncOpExpr*)expr)->val, score)
	;
}

void MincEncOpExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	val->collectParams(block, ((MincEncOpExpr*)expr)->val, params, paramIdx);
}

void MincEncOpExpr::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		val->resolve(block);
		MincExpr::resolve(block);
	}
}

void MincEncOpExpr::forget()
{
	val->forget();
	MincExpr::forget();
}

std::string MincEncOpExpr::str() const
{
	return oopstr + val->str() + copstr;
}

std::string MincEncOpExpr::shortStr() const
{
	return oopstr + val->shortStr() + copstr;
}

int MincEncOpExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincEncOpExpr* _other = (const MincEncOpExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->val->comp(_other->val);
}

MincExpr* MincEncOpExpr::clone() const
{
	return new MincEncOpExpr(loc, op, oopstr.c_str(), copstr.c_str(), val->clone());
}