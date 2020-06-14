#include "minc_api.hpp"

PrefixExprAST::PrefixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a)
	: ExprAST(loc, ExprAST::ExprType::PREOP), op(op), a(a), opstr(opstr)
{
}
bool PrefixExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((PrefixExprAST*)expr)->op == this->op && a->match(block, ((PrefixExprAST*)expr)->a, score);
}

void PrefixExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	a->collectParams(block, ((PrefixExprAST*)expr)->a, params, paramIdx);
}

void PrefixExprAST::resolveTypes(const BlockExprAST* block)
{
	a->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string PrefixExprAST::str() const
{
	return (std::isalpha(opstr.back()) ? opstr + ' ' : opstr) + a->str();
}

std::string PrefixExprAST::shortStr() const
{
	return (std::isalpha(opstr.back()) ? opstr + ' ' : opstr) + a->shortStr();
}

int PrefixExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const PrefixExprAST* _other = (const PrefixExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}

ExprAST* PrefixExprAST::clone() const
{
	return new PrefixExprAST(loc, op, opstr.c_str(), a->clone());
}