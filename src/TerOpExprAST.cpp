#include "minc_api.hpp"

TerOpExprAST::TerOpExprAST(const Location& loc, int op1, int op2, const char* opstr1, const char* opstr2, ExprAST* a, ExprAST* b, ExprAST* c)
	: ExprAST(loc, ExprAST::ExprType::TEROP), op1(op1), op2(op2), a(a), b(b), c(c), opstr1(opstr1), opstr2(opstr2)
{
}

bool TerOpExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((TerOpExprAST*)expr)->op1 == this->op1
		&& ((TerOpExprAST*)expr)->op2 == this->op2
		&& a->match(block, ((TerOpExprAST*)expr)->a, score)
		&& b->match(block, ((TerOpExprAST*)expr)->b, score)
		&& c->match(block, ((TerOpExprAST*)expr)->c, score)
	;
}

void TerOpExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((TerOpExprAST*)expr)->a, params, paramIdx);
	b->collectParams(block, ((TerOpExprAST*)expr)->b, params, paramIdx);
	c->collectParams(block, ((TerOpExprAST*)expr)->c, params, paramIdx);
}

void TerOpExprAST::resolveTypes(const BlockExprAST* block)
{
	a->resolveTypes(block);
	b->resolveTypes(block);
	c->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string TerOpExprAST::str() const
{
	return a->str() + " " + opstr1 + " " + b->str() + " " + opstr2 + " " + c->str();
}

int TerOpExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const TerOpExprAST* _other = (const TerOpExprAST*)other;
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

ExprAST* TerOpExprAST::clone() const
{
	return new TerOpExprAST(loc, op1, op2, opstr1.c_str(), opstr2.c_str(), a->clone(), b->clone(), c->clone());
}