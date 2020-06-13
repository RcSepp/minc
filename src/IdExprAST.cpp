#include "minc_api.hpp"

IdExprAST::IdExprAST(const Location& loc, const char* name) : ExprAST(loc, ExprAST::ExprType::ID), name(name)
{
}

bool IdExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	score += 6; // Reward exact match (score is disregarded on mismatch)
	return expr->exprtype == this->exprtype && ((IdExprAST*)expr)->name == this->name;
}

void IdExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
}

std::string IdExprAST::str() const
{
	return name;
}

int IdExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const IdExprAST* _other = (const IdExprAST*)other;
	return this->name.compare(_other->name);
}

ExprAST* IdExprAST::clone() const
{
	return new IdExprAST(loc, name.c_str());
}