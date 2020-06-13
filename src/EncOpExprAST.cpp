#include "minc_api.hpp"

EncOpExprAST::EncOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* val)
	: ExprAST(loc, ExprAST::ExprType::ENCOP), op(op), val(val), oopstr(oopstr), copstr(copstr)
{
}

bool EncOpExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((EncOpExprAST*)expr)->op == this->op
		&& val->match(block, ((EncOpExprAST*)expr)->val, score)
	;
}

void EncOpExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	val->collectParams(block, ((EncOpExprAST*)expr)->val, params, paramIdx);
}

void EncOpExprAST::resolveTypes(const BlockExprAST* block)
{
	val->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string EncOpExprAST::str() const
{
	return oopstr + val->str() + copstr;
}

int EncOpExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const EncOpExprAST* _other = (const EncOpExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->val->comp(_other->val);
}

ExprAST* EncOpExprAST::clone() const
{
	return new EncOpExprAST(loc, op, oopstr.c_str(), copstr.c_str(), val->clone());
}