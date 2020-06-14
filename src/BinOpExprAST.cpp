#include "minc_api.hpp"

BinOpExprAST::BinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a, ExprAST* b)
	: ExprAST(loc, ExprAST::ExprType::BINOP), op(op), a(a), b(b), opstr(opstr), a_post(a->loc, op, opstr, a), b_pre(b->loc, op, opstr, b)
{
}

bool BinOpExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((BinOpExprAST*)expr)->op == this->op && a->match(block, ((BinOpExprAST*)expr)->a, score) && b->match(block, ((BinOpExprAST*)expr)->b, score);
}

void BinOpExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx);
	b->collectParams(block, ((BinOpExprAST*)expr)->b, params, paramIdx);
}

void BinOpExprAST::resolveTypes(const BlockExprAST* block)
{
	a->resolveTypes(block);
	b->resolveTypes(block);
	ExprAST::resolveTypes(block);
	a_post.resolveTypes(block);
	b_pre.resolveTypes(block);
}

std::string BinOpExprAST::str() const
{
	return a->str() + " " + opstr + " " + b->str();
}

std::string BinOpExprAST::shortStr() const
{
	return a->shortStr() + " " + opstr + " " + b->shortStr();
}

int BinOpExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const BinOpExprAST* _other = (const BinOpExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	c = this->a->comp(_other->a);
	if (c) return c;
	return this->b->comp(_other->b);
}

ExprAST* BinOpExprAST::clone() const
{
	return new BinOpExprAST(loc, op, opstr.c_str(), a->clone(), b->clone());
}