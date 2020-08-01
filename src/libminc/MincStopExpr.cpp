#include "minc_api.hpp"

MincStopExpr::MincStopExpr(const MincLocation& loc)
	: MincExpr(loc, MincExpr::ExprType::STOP)
{
}

bool MincStopExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype;
}

void MincStopExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
}

std::string MincStopExpr::str() const
{
	return ";";
}

MincExpr* MincStopExpr::clone() const
{
	return new MincStopExpr(loc);
}