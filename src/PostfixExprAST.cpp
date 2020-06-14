#include "minc_api.hpp"

PostfixExprAST::PostfixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a)
	: ExprAST(loc, ExprAST::ExprType::POSTOP), op(op), a(a), opstr(opstr)
{
}

bool PostfixExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((PostfixExprAST*)expr)->op == this->op && a->match(block, ((PostfixExprAST*)expr)->a, score);
}

void PostfixExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((PostfixExprAST*)expr)->a, params, paramIdx);
}

void PostfixExprAST::resolveTypes(const BlockExprAST* block)
{
	a->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string PostfixExprAST::str() const
{
	return a->str() + (std::isalpha(opstr.front()) ? opstr + ' ' : opstr);
}

std::string PostfixExprAST::shortStr() const
{
	return a->shortStr() + (std::isalpha(opstr.front()) ? opstr + ' ' : opstr);
}

int PostfixExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const PostfixExprAST* _other = (const PostfixExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}

ExprAST* PostfixExprAST::clone() const
{
	return new PostfixExprAST(loc, op, opstr.c_str(), a->clone());
}