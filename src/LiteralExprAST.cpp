#include <regex>
#include "minc_api.hpp"

LiteralExprAST::LiteralExprAST(const Location& loc, const char* value)
	: ExprAST(loc, ExprAST::ExprType::LITERAL), value(value)
{
}

bool LiteralExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	return expr->exprtype == this->exprtype && ((LiteralExprAST*)expr)->value == this->value;
}

void LiteralExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
}

std::string LiteralExprAST::str() const
{
	return std::regex_replace(std::regex_replace(value, std::regex("\n"), "\\n"), std::regex("\r"), "\\r");
}

int LiteralExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const LiteralExprAST* _other = (const LiteralExprAST*)other;
	return this->value.compare(_other->value);
}

ExprAST* LiteralExprAST::clone() const
{
	return new ::LiteralExprAST(loc, value.c_str());
}