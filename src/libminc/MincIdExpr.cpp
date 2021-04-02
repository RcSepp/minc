#include "minc_api.hpp"

MincIdExpr::MincIdExpr(const MincLocation& loc, const char* name) : MincExpr(loc, MincExpr::ExprType::ID), name(name)
{
}

bool MincIdExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	score += 6; // Reward exact match (score is disregarded on mismatch)
	return expr->exprtype == this->exprtype && ((MincIdExpr*)expr)->name == this->name;
}

void MincIdExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	expr->resolvedKernel = &UNUSED_KERNEL;
}

std::string MincIdExpr::str() const
{
	return name;
}

int MincIdExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincIdExpr* _other = (const MincIdExpr*)other;
	return this->name.compare(_other->name);
}

MincExpr* MincIdExpr::clone() const
{
	return new MincIdExpr(loc, name.c_str());
}

extern "C"
{
	bool ExprIsId(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::ID;
	}

	const char* getIdExprName(const MincIdExpr* expr)
	{
		return expr->name.c_str();
	}
}