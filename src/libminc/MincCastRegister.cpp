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
	const MincCast* first;
	const MincCast* second;

	IndirectCast(const MincCast* first, const MincCast* second)
		: MincCast(first->fromType, second->toType, this), MincKernel(&second->kernel->runner), first(first), second(second) {}
	int getCost() const { return first->getCost() + second->getCost(); }
	MincCast* derive() const
	{
		MincCast* derivedSecond = second->derive();
		return derivedSecond == nullptr ? first->derive() : new IndirectCast(first, derivedSecond);
	}

	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
	{
		params[0] = new MincCastExpr(first, params[0]);
		return second->kernel->build(buildtime, params);
	}
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
	{
		assert(0);
		return false; // LCOV_EXCL_LINE
	}
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
	{
		return toType;
	}
};

MincCastRegister::MincCastRegister(MincBlockExpr* block)
	: block(block)
{
}

void MincCastRegister::defineDirectCast(MincCast* cast)
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

void MincCastRegister::defineIndirectCast(const MincCastRegister& castreg, const MincCast* cast)
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

const MincCast* MincCastRegister::lookupCast(MincObject* fromType, MincObject* toType) const
{
	const auto& cast = casts.find(std::make_pair(fromType, toType));
	return cast == casts.end() ? nullptr : cast->second;
}

bool MincCastRegister::isInstance(MincObject* derivedType, MincObject* baseType) const
{
	const auto& cast = casts.find(std::make_pair(derivedType, baseType));
	return cast != casts.end() && cast->second->getCost() == 0; // Zero-cost casts are inheritance casts
}

void MincCastRegister::listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const
{
	for (const std::pair<std::pair<MincObject*, MincObject*>, MincCast*>& cast: this->casts)
		casts.push_back(cast.first);
}

size_t MincCastRegister::countCasts() const
{
	return casts.size();
}

void MincCastRegister::iterateCasts(std::function<void(const MincCast* cast)> cbk) const
{
	for (const std::pair<const std::pair<MincObject*, MincObject*>, MincCast*>& iter: casts)
		cbk(iter.second);
}

void MincCastRegister::iterateBases(MincObject* derivedType, std::function<void(MincObject* baseType)> cbk) const
{
	for (const std::pair<const std::pair<MincObject*, MincObject*>, MincCast*>& iter: casts)
		if (iter.first.first == derivedType && iter.second->getCost() == 0) // Zero-cost casts are inheritance casts
			cbk(iter.first.second);
}