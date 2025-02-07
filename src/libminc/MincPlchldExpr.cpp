#include <cstring>
#include "minc_api.hpp"

extern MincObject ERROR_TYPE;

void storeParam(MincExpr* param, std::vector<MincExpr*>& params, size_t paramIdx);

MincPlchldExpr::MincPlchldExpr(const MincLocation& loc, char p1)
	: MincExpr(loc, MincExpr::ExprType::PLCHLD), p1(p1), p2(nullptr), flags(Flags::NONE)
{
}

MincPlchldExpr::MincPlchldExpr(const MincLocation& loc, char p1, const char* p2, Flags flags)
	: MincExpr(loc, MincExpr::ExprType::PLCHLD), p1(p1), flags(flags)
{
	size_t p2len = strlen(p2);
	this->p2 = new char[p2len + 1];
	memcpy(this->p2, p2, p2len + 1);
}

MincPlchldExpr::MincPlchldExpr(const MincLocation& loc, const char* p2)
	: MincExpr(loc, MincExpr::ExprType::PLCHLD), p1(p2[0])
{
	size_t p2len = strlen(++p2);
	if (p2len && p2[p2len - 1] == '!')
	{
		--p2len;
		if (p2len && p2[p2len - 1] == '!')
		{
			--p2len;
			flags = Flags::NO_CAST;
		}
		else
			flags = Flags::NO_TYPECAST;
	}
	else
		flags = Flags::NONE;

	this->p2 = new char[p2len + 1];
	memcpy(this->p2, p2, p2len);
	this->p2[p2len] = '\0';
}

MincObject* MincPlchldExpr::getType(const MincBlockExpr* parentBlock) const
{
	if (p2 == nullptr || p1 == 'L')
		return nullptr;
	const MincSymbol* var = parentBlock->lookupSymbol(p2);
	if (var == nullptr)
		throw UndefinedIdentifierException(new MincIdExpr(loc, p2));
	return var->value;
}

bool MincPlchldExpr::match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const
{
	// Make sure collectParams(tplt, tplt) collects all placeholders
	if (expr == this)
		return true;

	switch(p1)
	{
	case 'I':
		if (expr->exprtype != MincExpr::ExprType::ID) return false;
		score += 1; // Reward $I (over $E or $S)
		break;
	case 'L':
		if (expr->exprtype != MincExpr::ExprType::LITERAL) return false;
		if (p2 == nullptr)
		{
			score += 6; // Reward vague match
			return true;
		}
		else
		{
			score += 7; // Reward exact match
			const std::string& value = ((const MincLiteralExpr*)expr)->value;
			if (value.back() == '"' || value.back() == '\'')
			{
				size_t prefixLen = value.find(value.back());
				return value.compare(0, prefixLen, p2) == 0 && p2[prefixLen] == '\0';
			}
			else
			{
				const char* postFix;
				for (postFix = value.c_str() + value.size() - 1; *postFix != '-' && (*postFix < '0' || *postFix > '9'); --postFix) {}
				return strcmp(p2, ++postFix) == 0;
			}
		}
	case 'B': score += 6; return expr->exprtype == MincExpr::ExprType::BLOCK;
	case 'P': score += 6; return expr->exprtype == MincExpr::ExprType::PLCHLD;
	case 'V': score += 6; return expr->exprtype == MincExpr::ExprType::ELLIPSIS;
	case 'D':
		// Don't match types for deferred placeholders
		score += 1; // Reward vague match
		return true;
	case 'E':
	case 'S':
		if (expr->exprtype == MincExpr::ExprType::STOP) return false;
		break;
	default: throw CompileError(std::string("Invalid placeholder: $") + p1, loc);
	}

	if (p2 == nullptr)
	{
		score += 1; // Reward vague match
		return true;
	}
	else
	{
		MincObject* exprType = expr->getType(block);
		MincObject* tpltType = getType(block);
		if (exprType == tpltType)
		{
			score += 5; // Reward exact match
			return true;
		}
		if (exprType == &ERROR_TYPE) // Errors match any template type
		{
			score -= 256; // Penalize erroneous match
			return true;
		}
		const MincCast* cast = flags == Flags::NO_CAST ? nullptr : block->lookupCast(exprType, tpltType);
		if (cast != nullptr)
		{
			int cost = cast->getCost();
			if (cost != 0 && flags == Flags::NO_TYPECAST)
				return false; // Prohibit illegal match
			score += 3; // Reward inexact match (inheritance or type-cast)
			score -= cost; // Penalize type-cast
			return true;
		}
		return false; // Prohibit mismatch
	}
}

void MincPlchldExpr::collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const
{
	if (p1 == 'D')
		expr->forget(); // Forget kernel for deferred parameters
	else if (p2 != nullptr && p1 != 'L' && p1 != 'I')
	{
		MincObject* exprType = expr->getType(block);
		if (exprType != &ERROR_TYPE)
		{
			MincObject* tpltType = getType(block);
			if (exprType != tpltType)
			{
				const MincCast* cast = block->lookupCast(exprType, tpltType);
				assert(cast != nullptr);
				expr = new MincCastExpr(cast, expr);
			}
		}
	}
	storeParam(expr, params, paramIdx++);
}

std::string MincPlchldExpr::str() const
{
	static const char* suffixes[] = {
		">", "!>", "!!>"
	};
	return '$' + std::string(1, p1) + (p2 == nullptr ? "" : '<' + std::string(p2) + (suffixes[flags]));
}

int MincPlchldExpr::comp(const MincExpr* other) const
{
	int c = MincExpr::comp(other);
	if (c) return c;
	const MincPlchldExpr* _other = (const MincPlchldExpr*)other;
	c = this->p1 - _other->p1;
	if (c) return c;
	c = (int)this->flags - (int)_other->flags;
	if (c) return c;
	if (this->p2 == nullptr || _other->p2 == nullptr) return this->p2 - _other->p2;
	return strcmp(this->p2, _other->p2);
}

MincExpr* MincPlchldExpr::clone() const
{
	return p2 == nullptr ? new MincPlchldExpr(loc, p1) : new MincPlchldExpr(loc, p1, p2, flags);
}

extern "C"
{
	bool ExprIsPlchld(const MincExpr* expr)
	{
		return expr->exprtype == MincExpr::ExprType::PLCHLD;
	}

	char getPlchldExprLabel(const MincPlchldExpr* expr)
	{
		return expr->p1;
	}

	const char* getPlchldExprSublabel(const MincPlchldExpr* expr)
	{
		return expr->p2;
	}
}