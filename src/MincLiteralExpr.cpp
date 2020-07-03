#include <regex>
#include "minc_api.hpp"

MincLiteralExpr::MincLiteralExpr(const MincLocation& loc, const char* value)
	: MincExpr(loc, MincExpr::ExprType::LITERAL), value(value)
{
}

bool MincLiteralExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((MincLiteralExpr*)expr)->value == this->value;
}

void MincLiteralExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
}

std::string MincLiteralExpr::str() const
{
	return std::regex_replace(std::regex_replace(value, std::regex("\n"), "\\n"), std::regex("\r"), "\\r");
}

int MincLiteralExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincLiteralExpr* _other = (const MincLiteralExpr*)other;
	return this->value.compare(_other->value);
}

MincExpr* MincLiteralExpr::clone() const
{
	return new ::MincLiteralExpr(loc, value.c_str());
}