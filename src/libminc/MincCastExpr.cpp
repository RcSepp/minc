#include "minc_api.hpp"

MincCastExpr::MincCastExpr(const MincCast* cast, MincExpr* source) : MincExpr(source->loc, MincExpr::ExprType::CAST), cast(cast)
{
	resolvedKernel = cast->kernel;
	resolvedParams.push_back(source);
}

bool MincCastExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	assert(0);
	return false;
}

void MincCastExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	assert(0);
}

std::string MincCastExpr::str() const
{
	return "cast expression";
	//TODO: Think of a way to pass scope to make this more verbose
	//TODO	Example: `return "cast expression from " + scope->lookupSymbolName(cast->fromType, "UNKNOWN_TYPE") + " to " + scope->lookupSymbolName(cast->toType, "UNKNOWN_TYPE");`
}

MincExpr* MincCastExpr::getSourceExpr() const
{
	return resolvedParams[0];
}

MincExpr* MincCastExpr::getDerivedExpr() const
{
	MincCast* derivedCast = cast->derive();
	MincExpr* expr = resolvedParams[0];
	return derivedCast ?
		new MincCastExpr(derivedCast, expr) :
		(expr->exprtype == MincExpr::ExprType::CAST ? ((MincCastExpr*)expr)->getDerivedExpr() : expr)
	;
}

MincExpr* MincCastExpr::clone() const
{
	return new MincCastExpr(cast, resolvedParams[0]->clone());
}

extern "C"
{
	bool ExprIsCast(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::CAST;
	}

	MincExpr* getDerivedExpr(MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::CAST ? ((MincCastExpr*)expr)->getDerivedExpr() : expr;
	}

	MincExpr* getCastExprSource(const MincCastExpr* expr)
	{
		return expr->resolvedParams[0];
	}
}