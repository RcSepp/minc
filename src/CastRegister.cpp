#include "minc_api.hpp"

InheritanceCast::InheritanceCast(MincObject* fromType, MincObject* toType, MincKernel* kernel)
	: MincCast(fromType, toType, kernel)
{
}

int InheritanceCast::getCost() const
{
	return 0;
}

MincCast* InheritanceCast::derive() const
{
	return nullptr;
}

TypeCast::TypeCast(MincObject* fromType, MincObject* toType, MincKernel* kernel)
	: MincCast(fromType, toType, kernel)
{
}

int TypeCast::getCost() const
{
	return 1;
}

MincCast* TypeCast::derive() const
{
	return new TypeCast(fromType, toType, kernel);
}

struct IndirectCast : public MincCast, public MincKernel
{
	MincCast* first;
	MincCast* second;

	IndirectCast(MincCast* first, MincCast* second)
		: MincCast(first->fromType, second->toType, this), first(first), second(second) {}
	int getCost() const { return first->getCost() + second->getCost(); }
	MincCast* derive() const
	{
		MincCast* derivedSecond = second->derive();
		return derivedSecond == nullptr ? first : new IndirectCast(first, derivedSecond);
	}

	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
	{
		std::vector<MincExpr*> _params(1, new MincCastExpr(first, params[0])); //TODO: make params const and implement this inline (`codegen(parentBlock, { new MincCastExpr(first, params[0]) });`)
		return second->kernel->codegen(parentBlock, _params);
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return second->kernel->getType(parentBlock, { new MincCastExpr(first, params[0]) });
	}
};

CastRegister::CastRegister(MincBlockExpr* block)
	: block(block)
{
}

void CastRegister::defineDirectCast(MincCast* cast)
{
	const auto& key = std::make_pair(cast->fromType, cast->toType);
	casts[key] = cast;

	fwdCasts.insert(std::make_pair(cast->fromType, cast));
	bwdCasts.insert(std::make_pair(cast->toType, cast));

// // Print type-casts
// if (fromTypeName[fromTypeName.size() - 1] != ')'
// 	&& fromTypeName.rfind("MincExpr<", 0)
// 	&& fromTypeName.find("UNKNOWN_TYPE") == std::string::npos
// 	&& (toTypeName != "MincObject")
// 	&& (toTypeName != "BuiltinType")
// 	&& (toTypeName != "PawsBase")
// 	&& cast->getCost() >= 1
// )
// 	std::cout << "    " << fromTypeName << "-->|" << cast->getCost() << "|" << toTypeName << ";\n";
}

void CastRegister::defineIndirectCast(const CastRegister& castreg, MincCast* cast)
{
	std::map<std::pair<MincObject*, MincObject*>, MincCast*>::const_iterator existingCast;

	auto toTypefwdCasts = castreg.fwdCasts.equal_range(cast->toType);
	for (auto iter = toTypefwdCasts.first; iter != toTypefwdCasts.second; ++iter)
	{
		MincCast* c = iter->second;
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
		MincCast* c = iter->second;
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

const MincCast* CastRegister::lookupCast(MincObject* fromType, MincObject* toType) const
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
	for (const std::pair<std::pair<MincObject*, MincObject*>, MincCast*>& cast: this->casts)
		casts.push_back(cast.first);
}

size_t CastRegister::countCasts() const
{
	return casts.size();
}

void CastRegister::iterateCasts(std::function<void(const MincCast* cast)> cbk) const
{
	for (const std::pair<const std::pair<MincObject*, MincObject*>, MincCast*>& iter: casts)
		cbk(iter.second);
}