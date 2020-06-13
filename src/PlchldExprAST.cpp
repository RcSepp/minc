#include <cstring>
#include "minc_api.hpp"

void storeParam(ExprAST* param, std::vector<ExprAST*>& params, size_t paramIdx);

PlchldExprAST::PlchldExprAST(const Location& loc, char p1)
	: ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p1), p2(nullptr), allowCast(false)
{
}

PlchldExprAST::PlchldExprAST(const Location& loc, char p1, const char* p2, bool allowCast)
	: ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p1), allowCast(allowCast)
{
	size_t p2len = strlen(p2);
	this->p2 = new char[p2len + 1];
	memcpy(this->p2, p2, p2len + 1);
}

PlchldExprAST::PlchldExprAST(const Location& loc, const char* p2)
	: ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p2[0])
{
	size_t p2len = strlen(++p2);
	if (p2len && p2[p2len - 1] == '!')
	{
		allowCast = false;
		this->p2 = new char[p2len];
		memcpy(this->p2, p2, p2len - 1);
		this->p2[p2len - 1] = '\0';
	}
	else
	{
		allowCast = true;
		this->p2 = new char[p2len + 1];
		memcpy(this->p2, p2, p2len + 1);
	}
}

BaseType* PlchldExprAST::getType(const BlockExprAST* parentBlock) const
{
	if (p2 == nullptr || p1 == 'L')
		return nullptr;
	const Variable* var = parentBlock->lookupSymbol(p2);
	if (var == nullptr)
		throw UndefinedIdentifierException(new IdExprAST(loc, p2));
	return (BaseType*)var->value->getConstantValue();
}

bool PlchldExprAST::match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
{
	// Make sure collectParams(tplt, tplt) collects all placeholders
	if (expr == this)
		return true;

	switch(p1)
	{
	case 'I':
		if (expr->exprtype != ExprAST::ExprType::ID) return false;
		score += 1; // Reward $I (over $E or $S)
		break;
	case 'L':
		if (expr->exprtype != ExprAST::ExprType::LITERAL) return false;
		if (p2 == nullptr)
		{
			score += 6; // Reward vague match
			return true;
		}
		else
		{
			score += 7; // Reward exact match
			const std::string& value = ((const LiteralExprAST*)expr)->value;
			if (value.back() == '"' || value.back() == '\'')
			{
				size_t prefixLen = value.rfind(value.back());
				return value.compare(0, prefixLen, p2) == 0 && p2[prefixLen] == '\0';
			}
			else
			{
				const char* postFix;
				for (postFix = value.c_str() + value.size() - 1; *postFix != '-' && (*postFix < '0' || *postFix > '9'); --postFix) {}
				return strcmp(p2, ++postFix) == 0;
			}
		}
	case 'B': score += 6; return expr->exprtype == ExprAST::ExprType::BLOCK;
	case 'P': score += 6; return expr->exprtype == ExprAST::ExprType::PLCHLD;
	case 'V': score += 6; return expr->exprtype == ExprAST::ExprType::ELLIPSIS;
	case 'E':
	case 'S':
		if (expr->exprtype == ExprAST::ExprType::STOP) return false;
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
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType == tpltType)
		{
			score += 5; // Reward exact match
			return true;
		}
		const Cast* cast = allowCast ? block->lookupCast(exprType, tpltType) : nullptr;
		if (cast != nullptr)
		{
			score += 3; // Reward inexact match (inheritance or type-cast)
			score -= cast->getCost(); // Penalize type-cast
			return true;
		}
		return false;
	}
}

void PlchldExprAST::collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
{
	if (p2 != nullptr && p1 != 'L')
	{
		BaseType* exprType = expr->getType(block);
		BaseType* tpltType = getType(block);
		if (exprType != tpltType)
		{
			const Cast* cast = block->lookupCast(exprType, tpltType);
			assert(cast != nullptr);
			ExprAST* castExpr = new CastExprAST(cast, expr);
			storeParam(castExpr, params, paramIdx++);
			return;
		}
	}
	storeParam(expr, params, paramIdx++);
}

std::string PlchldExprAST::str() const
{
	return '$' + std::string(1, p1) + (p2 == nullptr ? "" : '<' + std::string(p2) + (allowCast ? ">" : "!>"));
}

int PlchldExprAST::comp(const ExprAST* other) const
{
	int c = ExprAST::comp(other);
	if (c) return c;
	const PlchldExprAST* _other = (const PlchldExprAST*)other;
	c = this->p1 - _other->p1;
	if (c) return c;
	c = (int)this->allowCast - (int)_other->allowCast;
	if (c) return c;
	if (this->p2 == nullptr || _other->p2 == nullptr) return this->p2 - _other->p2;
	return strcmp(this->p2, _other->p2);
}

ExprAST* PlchldExprAST::clone() const
{
	return p2 == nullptr ? new PlchldExprAST(loc, p1) : new PlchldExprAST(loc, p1, p2, allowCast);
}