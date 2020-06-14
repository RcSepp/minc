#include "minc_api.hpp"

VarBinOpExprAST::VarBinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a)
	: ExprAST(loc, ExprAST::ExprType::VARBINOP), op(op), a(a), opstr(opstr)
{
}

bool VarBinOpExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	if (expr->exprtype == this->exprtype)
		return ((VarBinOpExprAST*)expr)->op == this->op && a->match(block, ((VarBinOpExprAST*)expr)->a, score);
	else if (expr->exprtype == ExprAST::ExprType::BINOP)
		return ((BinOpExprAST*)expr)->op == this->op && this->match(block, ((BinOpExprAST*)expr)->a, score) && this->match(block, ((BinOpExprAST*)expr)->b, score);
	else
		return a->match(block, expr, score);
}

void VarBinOpExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	size_t paramBegin = paramIdx;
	if (expr->exprtype == this->exprtype)
		a->collectParams(block, ((VarBinOpExprAST*)expr)->a, params, paramIdx);
	else if (expr->exprtype == ExprAST::ExprType::BINOP)
	{
		size_t& paramIdx1 = paramIdx, paramIdx2 = paramIdx;
		this->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx1);
		this->collectParams(block, ((BinOpExprAST*)expr)->b, params, paramIdx2);
		paramIdx = paramIdx1 > paramIdx2 ? paramIdx1 : paramIdx2;
	}
	else
		a->collectParams(block, expr, params, paramIdx);

	// Replace all non-list parameters within this VarBinOpExprAST with single-element lists,
	// because ellipsis parameters are expected to always be lists
	for (size_t i = paramBegin; i < paramIdx; ++i)
		if (params[i]->exprtype != ExprAST::ExprType::LIST)
			params[i] = new ListExprAST('\0', { params[i] });
}

void VarBinOpExprAST::resolveTypes(const BlockExprAST* block)
{
	a->resolveTypes(block);
	ExprAST::resolveTypes(block);
}

std::string VarBinOpExprAST::str() const
{
	return a->str() + " " + opstr + " ...";
}

std::string VarBinOpExprAST::shortStr() const
{
	return a->shortStr() + " " + opstr + " ...";
}

int VarBinOpExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const VarBinOpExprAST* _other = (const VarBinOpExprAST*)other;
	c = this->op - _other->op;
	if (c) return c;
	return this->a->comp(_other->a);
}
ExprAST* VarBinOpExprAST::clone() const
{
	return new VarBinOpExprAST(loc, op, opstr.c_str(), a->clone());
}