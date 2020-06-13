#include "minc_api.hpp"

ArgOpExprAST::ArgOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* var, ListExprAST* args)
	: ExprAST(loc, ExprAST::ExprType::ARGOP), op(op), var(var), args(args), oopstr(oopstr), copstr(copstr)
{
}

bool ArgOpExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype
		&& ((ArgOpExprAST*)expr)->op == this->op
		&& var->match(block, ((ArgOpExprAST*)expr)->var, score)
		&& args->match(block, ((ArgOpExprAST*)expr)->args, score)
	;
}

void ArgOpExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	var->collectParams(block, ((ArgOpExprAST*)expr)->var, params, paramIdx);
	args->collectParams(block, ((ArgOpExprAST*)expr)->args, params, paramIdx);
}

void ArgOpExprAST::resolveTypes(const BlockExprAST* block)
{
	var->resolveTypes(block);
	args->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string ArgOpExprAST::str() const
{
	return var->str() + oopstr + args->str() + copstr;
}

int ArgOpExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const ArgOpExprAST* _other = (const ArgOpExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	c = this->var->comp(_other->var);
	if (c) return c;
	return this->args->comp(_other->args);
}

ExprAST* ArgOpExprAST::clone() const
{
	return new ArgOpExprAST(loc, op, oopstr.c_str(), copstr.c_str(), var->clone(), (ListExprAST*)args->clone());
}