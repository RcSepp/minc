#include "minc_api.hpp"

StopExprAST::StopExprAST(const Location& loc)
	: ExprAST(loc, ExprAST::ExprType::STOP)
{
}

bool StopExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void StopExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
}

std::string StopExprAST::str() const
{
	return ";";
}

ExprAST* StopExprAST::clone() const
{
	return new StopExprAST(loc);
}