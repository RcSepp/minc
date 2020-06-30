#include "minc_api.hpp"

InheritanceCast::InheritanceCast(MincObject* fromType, MincObject* toType, CodegenContext* context)
	: Cast(fromType, toType, context)
{
}

int InheritanceCast::getCost() const
{
	return 0;
}

Cast* InheritanceCast::derive() const
{
	return nullptr;
}

TypeCast::TypeCast(MincObject* fromType, MincObject* toType, CodegenContext* context)
	: Cast(fromType, toType, context)
{
}

int TypeCast::getCost() const
{
	return 1;
}

Cast* TypeCast::derive() const
{
	return new TypeCast(fromType, toType, context);
}

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
	MincObject* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return second->context->getType(parentBlock, { new CastExprAST(first, params[0]) });
	}
};

CastRegister::CastRegister(BlockExprAST* block)
	: block(block)
{
}

void CastRegister::defineDirectCast(Cast* cast)
{
	const auto& key = std::make_pair(cast->fromType, cast->toType);
	casts[key] = cast;

	fwdCasts.insert(std::make_pair(cast->fromType, cast));
	bwdCasts.insert(std::make_pair(cast->toType, cast));

// // Print type-casts
// if (fromTypeName[fromTypeName.size() - 1] != ')'
// 	&& fromTypeName.rfind("ExprAST<", 0)
// 	&& fromTypeName.find("UNKNOWN_TYPE") == std::string::npos
// 	&& (toTypeName != "MincObject")
// 	&& (toTypeName != "BuiltinType")
// 	&& (toTypeName != "PawsBase")
// 	&& cast->getCost() >= 1
// )
// 	std::cout << "    " << fromTypeName << "-->|" << cast->getCost() << "|" << toTypeName << ";\n";
}

void CastRegister::defineIndirectCast(const CastRegister& castreg, Cast* cast)
{
	std::map<std::pair<MincObject*, MincObject*>, Cast*>::const_iterator existingCast;

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

const Cast* CastRegister::lookupCast(MincObject* fromType, MincObject* toType) const
{
	const auto& cast = casts.find(std::make_pair(fromType, toType));
	return cast == casts.end() ? nullptr : cast->second;
}

bool CastRegister::isInstance(MincObject* derivedType, MincObject* baseType) const
{
	const auto& cast = casts.find(std::make_pair(derivedType, baseType));
	return cast != casts.end() && cast->second->getCost() == 0; // Zero-cost casts are inheritance casts
}

void CastRegister::listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const
{
	for (const std::pair<std::pair<MincObject*, MincObject*>, Cast*>& cast: this->casts)
		casts.push_back(cast.first);
}

size_t CastRegister::countCasts() const
{
	return casts.size();
}

void CastRegister::iterateCasts(std::function<void(const Cast* cast)> cbk) const
{
	for (const std::pair<const std::pair<MincObject*, MincObject*>, Cast*>& iter: casts)
		cbk(iter.second);
}