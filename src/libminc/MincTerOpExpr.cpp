#include "minc_api.hpp"

MincTerOpExpr::MincTerOpExpr(const MincLocation& loc, int op1, int op2, const char* opstr1, const char* opstr2, MincExpr* a, MincExpr* b, MincExpr* c)
	: MincExpr(loc, MincExpr::ExprType::TEROP), op1(op1), op2(op2), a(a), b(b), c(c), opstr1(opstr1), opstr2(opstr2)
{
}

bool MincTerOpExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((MincTerOpExpr*)expr)->op1 == this->op1
		&& ((MincTerOpExpr*)expr)->op2 == this->op2
		&& a->match(block, ((MincTerOpExpr*)expr)->a, score)
		&& b->match(block, ((MincTerOpExpr*)expr)->b, score)
		&& c->match(block, ((MincTerOpExpr*)expr)->c, score)
	;
}

void MincTerOpExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((MincTerOpExpr*)expr)->a, params, paramIdx);
	b->collectParams(block, ((MincTerOpExpr*)expr)->b, params, paramIdx);
	c->collectParams(block, ((MincTerOpExpr*)expr)->c, params, paramIdx);
}

void MincTerOpExpr::resolve(const MincBlockExpr* block)
{
	if (!isResolved())
	{
		a->resolve(block);
		b->resolve(block);
		c->resolve(block);
		MincExpr::resolve(block);
	}
}

void MincTerOpExpr::forget()
{
	a->forget();
	b->forget();
	c->forget();
	MincExpr::forget();
}

std::string MincTerOpExpr::str() const
{
	return a->str() + " " + opstr1 + " " + b->str() + " " + opstr2 + " " + c->str();
}

std::string MincTerOpExpr::shortStr() const
{
	return a->shortStr() + " " + opstr1 + " " + b->shortStr() + " " + opstr2 + " " + c->shortStr();
}

int MincTerOpExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincTerOpExpr* _other = (const MincTerOpExpr*)other;
	c = this->op1 - _other->op1;
	if (c) return c;
	c = this->op2 - _other->op2;
	if (c) return c;
	c = this->a->comp(_other->a);
	if (c) return c;
	c = this->b->comp(_other->b);
	if (c) return c;
	return this->c->comp(_other->c);
}

MincExpr* MincTerOpExpr::clone() const
{
	return new MincTerOpExpr(loc, op1, op2, opstr1.c_str(), opstr2.c_str(), a->clone(), b->clone(), c->clone());
}

extern "C"
{
	bool ExprIsTerOp(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::TEROP;
	}
}