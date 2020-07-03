#include "minc_api.hpp"

MincArgOpExpr::MincArgOpExpr(const MincLocation& loc, int op, const char* oopstr, const char* copstr, MincExpr* var, MincListExpr* args)
	: MincExpr(loc, MincExpr::ExprType::ARGOP), op(op), var(var), args(args), oopstr(oopstr), copstr(copstr)
{
}

bool MincArgOpExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((MincArgOpExpr*)expr)->op == this->op
		&& var->match(block, ((MincArgOpExpr*)expr)->var, score)
		&& args->match(block, ((MincArgOpExpr*)expr)->args, score)
	;
}

void MincArgOpExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	var->collectParams(block, ((MincArgOpExpr*)expr)->var, params, paramIdx);
	args->collectParams(block, ((MincArgOpExpr*)expr)->args, params, paramIdx);
}

void MincArgOpExpr::resolveTypes(const MincBlockExpr* block)
{
	var->resolveTypes(block);
	args->resolveTypes(block);
	MincExpr::resolveTypes(block);
}

std::string MincArgOpExpr::str() const
{
	return var->str() + oopstr + args->str() + copstr;
}

std::string MincArgOpExpr::shortStr() const
{
	return var->shortStr() + oopstr + args->shortStr() + copstr;
}

int MincArgOpExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincArgOpExpr* _other = (const MincArgOpExpr*)other;
	c = this->op - _other->op;
	if (c) return c;
	c = this->var->comp(_other->var);
	if (c) return c;
	return this->args->comp(_other->args);
}

MincExpr* MincArgOpExpr::clone() const
{
	return new MincArgOpExpr(loc, op, oopstr.c_str(), copstr.c_str(), var->clone(), (MincListExpr*)args->clone());
}