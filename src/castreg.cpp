#include "ast.h"
#include <iostream> //DELETE

#define DETECT_UNDEFINED_TYPE_CASTS

struct IndirectCast : public Cast, public CodegenContext
{
	Cast* first;
	Cast* second;

	IndirectCast(Cast* first, Cast* second)
		: Cast(first->fromType, second->toType, this), first(first), second(second) {}
	int getCost() const { return first->getCost() + second->getCost(); }
	Cast* derive() const
	{
		Cast* derivedSecond = second->derive();
		return derivedSecond == nullptr ? first : new IndirectCast(first, derivedSecond);
	}

	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		std::vector<ExprAST*> _params(1, new CastExprAST(first, params[0])); //TODO: make params const and implement this inline (`codegen(parentBlock, { new CastExprAST(first, params[0]) });`)
		return second->context->codegen(parentBlock, _params);
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return second->context->getType(parentBlock, { new CastExprAST(first, params[0]) });
	}
};

void CastRegister::defineDirectCast(Cast* cast)
{
	const auto& key = std::make_pair(cast->fromType, cast->toType);
	casts[key] = cast;

	fwdCasts.insert(std::make_pair(cast->fromType, cast));
	bwdCasts.insert(std::make_pair(cast->toType, cast));

#ifdef DETECT_UNDEFINED_TYPE_CASTS
	std::string fromTypeName = getTypeName(cast->fromType), toTypeName = getTypeName(cast->toType);
	if (fromTypeName.find("UNKNOWN_TYPE") != std::string::npos || toTypeName.find("UNKNOWN_TYPE") != std::string::npos)
		throw CompileError("type-cast defined from " + fromTypeName + " to " + toTypeName);
#endif

// // Print type-casts
// if (fromTypeName[fromTypeName.size() - 1] != ')'
// 	&& fromTypeName.rfind("ExprAST<", 0)
// 	&& fromTypeName.find("UNKNOWN_TYPE") == std::string::npos
// 	&& (toTypeName != "BaseType")
// 	&& (toTypeName != "BuiltinType")
// 	&& (toTypeName != "PawsBase")
// 	&& cast->getCost() >= 1
// )
// 	std::cout << "    " << fromTypeName << "-->|" << cast->getCost() << "|" << toTypeName << ";\n";
}
void CastRegister::defineIndirectCast(const CastRegister& castreg, Cast* cast)
{
	std::map<std::pair<BaseType*, BaseType*>, Cast*>::const_iterator existingCast;

	auto toTypefwdCasts = castreg.fwdCasts.equal_range(cast->toType);
	for (auto iter = toTypefwdCasts.first; iter != toTypefwdCasts.second; ++iter)
	{
		Cast* c = iter->second;
		// Define cast        |----------------------->|
		//             cast->fromType cast->toType c->toType

		// Skip if cast cast->fromType -> c->toType already exists with equal or lower cost
		if ((existingCast = casts.find(std::make_pair(cast->fromType, c->toType))) != casts.end())
		{
			if (existingCast->second->getCost() > cast->getCost() + c->getCost())
			{
				((IndirectCast*)existingCast->second)->first = cast;
				((IndirectCast*)existingCast->second)->second = c;
			}
			continue;
		}
		block->defineCast(new IndirectCast(cast, c));
	}

	auto fromTypebwdCasts = castreg.bwdCasts.equal_range(cast->fromType);
	for (auto iter = fromTypebwdCasts.first; iter != fromTypebwdCasts.second; ++iter)
	{
		Cast* c = iter->second;
		// Define cast      |------------------------->|
		//             c->fromType cast->fromType cast->toType

		// Skip if cast c->fromType -> toType already exists with equal or lower cost
		if ((existingCast = casts.find(std::make_pair(c->fromType, cast->toType))) != casts.end())
		{
			if (existingCast->second->getCost() > c->getCost() + cast->getCost())
			{
				((IndirectCast*)existingCast->second)->first = c;
				((IndirectCast*)existingCast->second)->second = cast;
			}
			continue;
		}

		block->defineCast(new IndirectCast(c, cast));
	}
}

const Cast* CastRegister::lookupCast(BaseType* fromType, BaseType* toType) const
{
	const auto& cast = casts.find(std::make_pair(fromType, toType));
	return cast == casts.end() ? nullptr : cast->second;
}
bool CastRegister::isInstance(BaseType* derivedType, BaseType* baseType) const
{
	const auto& cast = casts.find(std::make_pair(derivedType, baseType));
	return cast != casts.end() && cast->second->getCost() == 0; // Zero-cost casts are inheritance casts
}

void CastRegister::listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
{
	for (const std::pair<std::pair<BaseType*, BaseType*>, Cast*>& cast: this->casts)
		casts.push_back(cast.first);
}