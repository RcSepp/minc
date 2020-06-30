#include "minc_api.hpp"

CastExprAST::CastExprAST(const Cast* cast, ExprAST* source) : ExprAST(source->loc, ExprAST::ExprType::CAST), cast(cast)
{
	resolvedContext = cast->context;
	resolvedParams.push_back(source);
}

bool CastExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	assert(0);
	return false;
}

void CastExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	assert(0);
}

std::string CastExprAST::str() const
{
	return "cast expression";
	//TODO: Think of a way to pass scope to make this more verbose
	//TODO	Example: `return "cast expression from " + scope->lookupSymbolName(cast->fromType, "UNKNOWN_TYPE") + " to " + scope->lookupSymbolName(cast->toType, "UNKNOWN_TYPE");`
}

ExprAST* CastExprAST::getSourceExpr() const
{
	return resolvedParams[0];
}

ExprAST* CastExprAST::getDerivedExpr() const
{
	Cast* derivedCast = cast->derive();
	return derivedCast ? new CastExprAST(derivedCast, resolvedParams[0]) : resolvedParams[0];
}

ExprAST* CastExprAST::clone() const
{
	return new CastExprAST(cast, resolvedParams[0]->clone());
}