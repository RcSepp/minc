#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsArrayScope = nullptr;

typedef PawsValue<std::vector<MincObject*>*> PawsArray;

struct PawsArrayType : public PawsType
{
private:
	static std::recursive_mutex mutex;
	static std::set<PawsArrayType> arrayTypes;

	PawsArrayType(PawsType* valueType)
		: PawsType(sizeof(std::vector<MincObject*>*)), valueType(valueType) {}

public:
	PawsType* const valueType;
	static PawsArrayType* get(const MincBlockExpr* scope, PawsType* valueType)
	{
		std::unique_lock<std::recursive_mutex> lock(mutex);
		std::set<PawsArrayType>::iterator iter = arrayTypes.find(PawsArrayType(valueType));
		if (iter == arrayTypes.end())
		{
			iter = arrayTypes.insert(PawsArrayType(valueType)).first;
			PawsArrayType* t = const_cast<PawsArrayType*>(&*iter); //TODO: Find a way to avoid const_cast
			t->name = valueType->name + "[]";
			pawsArrayScope->defineSymbol(t->name, PawsType::TYPE, t);
			pawsArrayScope->defineCast(new InheritanceCast(t, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE)));
			pawsArrayScope->defineCast(new InheritanceCast(t, PawsArray::TYPE, new MincOpaqueCastKernel(PawsArray::TYPE)));
			scope->iterateBases(valueType, [&](MincObject* baseType) {
				PawsArrayType* baseArrayType = get(scope, (PawsType*)baseType);
				pawsArrayScope->defineCast(new InheritanceCast(t, baseArrayType, new MincOpaqueCastKernel(baseArrayType))); //TODO: Should create new array with all elements casted to baseType
			});
		}
		return const_cast<PawsArrayType*>(&*iter); //TODO: Find a way to avoid const_cast
	}
	MincObject* copy(MincObject* value) { return value; }//new PawsArray(*(PawsArray*)value); }
	void copyTo(MincObject* src, MincObject* dest) { ((PawsArray*)dest)->set(((PawsArray*)src)->get()); }
	void copyToNew(MincObject* src, MincObject* dest) { new(dest) PawsArray(((PawsArray*)src)->get()); }
	MincObject* alloc() { return new PawsArray(new std::vector<MincObject*>()); }
	MincObject* allocTo(MincObject* memory) { return new(memory) PawsArray(); }
	std::string toString(MincObject* value) const
	{
		std::vector<MincObject*>* const val = ((PawsArray*)value)->get();
		const size_t size = val->size();
		if (size == 0)
			return "[]";
		else
		{
			std::string str = "[" + valueType->toString(val->at(0));
			for (size_t i = 1; i < size; ++i)
				str += ", " + valueType->toString(val->at(i));
			return str + ']';
		}
	}
};
std::recursive_mutex PawsArrayType::mutex;
std::set<PawsArrayType> PawsArrayType::arrayTypes;
bool operator<(const PawsArrayType& lhs, const PawsArrayType& rhs)
{
	return lhs.valueType < rhs.valueType;
}

MincPackage PAWS_ARRAY("paws.array", [](MincBlockExpr* pkgScope) {
	pawsArrayScope = pkgScope;
	registerType<PawsArray>(pkgScope, "PawsArray");

	// Inline array declaration
	struct InlineDeclKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			for (MincExpr* key: ((MincListExpr*)params[0])->exprs)
				key->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			std::vector<MincExpr*>& values = ((MincListExpr*)params[0])->exprs;
			PawsArray* arr = new PawsArray(new std::vector<MincObject*>());
			if (values.size() == 0)
			{
				runtime.result = MincSymbol(PawsArrayType::get(runtime.parentBlock, PawsVoid::TYPE), arr);
				return false;
			}
			arr->get()->reserve(values.size());
			for (MincExpr* value: values)
			{
				if (value->run(runtime))
					return true;
				arr->get()->push_back(runtime.result.value);
			}
			runtime.result = MincSymbol(PawsArrayType::get(runtime.parentBlock, (PawsType*)((MincCastExpr*)values[0])->getDerivedExpr()->getType(runtime.parentBlock)), arr);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<MincExpr*>& values = ((MincListExpr*)params[0])->exprs;
			std::vector<PawsType*> valueTypes;
			if (values.size() == 0)
				return PawsArrayType::get(parentBlock, PawsVoid::TYPE);
			valueTypes.reserve(values.size());
			for (MincExpr* value: values)
				valueTypes.push_back((PawsType*)((MincCastExpr*)value)->getDerivedExpr()->getType(parentBlock));
			return PawsArrayType::get(parentBlock, valueTypes[0]);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("[$E<PawsBase>, ...]")[0], new InlineDeclKernel());

	// Array getter
	struct GetterKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getDerivedExpr()->run(runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			PawsType* valueType = ((PawsArrayType*)runtime.result.type)->valueType;
			if (params[1]->run(runtime))
				return true;
			size_t idx = ((PawsInt*)runtime.result.value)->get();
			if (idx >= arr->get()->size())
				throw CompileError(runtime.parentBlock, params[0]->loc, "index %i out of bounds for array of length %i", (int)idx, (int)arr->get()->size());
			runtime.result = MincSymbol(valueType, arr->get()->at(idx));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* type = (PawsType*)((MincCastExpr*)params[0])->getDerivedExpr()->getType(parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsArray>[$E<PawsInt>]")[0], new GetterKernel());

	// Array setter
	struct SetterKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);

			PawsType* valueType = ((PawsArrayType*)((MincCastExpr*)params[0])->getDerivedExpr()->getType(buildtime.parentBlock))->valueType;
			if (valueType == PawsVoid::TYPE)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot assign to empty array");
			MincExpr* valueExpr = ((MincCastExpr*)params[2])->getSourceExpr();

			MincObject *expectedType = valueType, *gotType = valueExpr->getType(buildtime.parentBlock);
			if (expectedType != gotType)
			{
				const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
				if (cast == nullptr)
					throw CompileError(buildtime.parentBlock, valueExpr->loc, "invalid conversion of %E from <%t> to <%t>", valueExpr, gotType, expectedType);
				(valueExpr = new MincCastExpr(cast, valueExpr))->build(buildtime);
			}

			(params[2] = valueExpr)->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getDerivedExpr()->run(runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			PawsType* valueType = ((PawsArrayType*)runtime.result.type)->valueType;
			if (params[1]->run(runtime))
				return true;
			size_t idx = ((PawsInt*)runtime.result.value)->get();
			if (idx >= arr->get()->size())
				throw CompileError(runtime.parentBlock, params[0]->loc, "index %i out of bounds for array of length %i", (int)idx, (int)arr->get()->size());

			if (params[2]->run(runtime))
				return true;
			MincObject* value = ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value);
			arr->get()->at(idx) = value;

			runtime.result = MincSymbol(valueType, value);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* type = (PawsType*)((MincCastExpr*)params[0])->getDerivedExpr()->getType(parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsArray>[$E<PawsInt>] = $E<PawsBase>")[0], new SetterKernel());

	// Array appender
	struct AppendKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);

			PawsType* valueType = ((PawsArrayType*)((MincCastExpr*)params[0])->getDerivedExpr()->getType(buildtime.parentBlock))->valueType;
			if (valueType == PawsVoid::TYPE)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "cannot assign to empty array");
			MincExpr* valueExpr = ((MincCastExpr*)params[1])->getSourceExpr();

			MincObject *expectedType = valueType, *gotType = valueExpr->getType(buildtime.parentBlock);
			if (expectedType != gotType)
			{
				const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
				if (cast == nullptr)
					throw CompileError(buildtime.parentBlock, valueExpr->loc, "invalid conversion of %E from <%t> to <%t>", valueExpr, gotType, expectedType);
				(valueExpr = new MincCastExpr(cast, valueExpr))->build(buildtime);
			}

			(params[1] = valueExpr)->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[0])->getDerivedExpr()->run(runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			PawsArrayType* arrType = (PawsArrayType*)runtime.result.type;

			if (params[1]->run(runtime))
				return true;
			MincObject* value = ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value);
			arr->get()->push_back(value);

			runtime.result = MincSymbol(arrType, arr);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return ((MincCastExpr*)params[0])->getDerivedExpr()->getType(parentBlock);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsArray> += $E<PawsBase>")[0], new AppendKernel());

	struct ArrayTypeKernel : public MincKernel
	{
		MincObject* const arrayType;
		ArrayTypeKernel(MincObject* arrayType=nullptr) : arrayType(arrayType) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			buildtime.result.type = PawsType::TYPE;
			return new ArrayTypeKernel(buildtime.result.value = PawsArrayType::get(buildtime.parentBlock, (PawsType*)buildtime.result.value));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			runtime.result.type = PawsType::TYPE;
			runtime.result.value = arrayType;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsType>[]")[0], new ArrayTypeKernel());

	// Define array iterating for statement
	class ArrayIterationKernel : public MincKernel
	{
		const MincStackSymbol* const iterId;
	public:
		ArrayIterationKernel(const MincStackSymbol* iterId=nullptr) : iterId(iterId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			PawsType* valueType = ((PawsArrayType*)((MincCastExpr*)params[1])->getDerivedExpr()->getType(buildtime.parentBlock))->valueType;
			const MincStackSymbol* iterId =
				valueType == PawsVoid::TYPE ? nullptr // Cannot allocate void
				: ((MincBlockExpr*)params[2])->allocStackSymbol(((MincIdExpr*)params[0])->name, valueType, valueType->size)
			;
			params[2]->build(buildtime);
			return new ArrayIterationKernel(iterId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (((MincCastExpr*)params[1])->getDerivedExpr()->run(runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			if (arr->get()->empty())
				return false;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			MincObject* iter = body->getStackSymbolOfNextStackFrame(runtime, iterId);
			for (MincObject* value: *arr->get())
			{
				((PawsType*)iterId->type)->copyTo(value, iter);
				if (body->run(runtime))
					return true;
			}
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I: $E<PawsArray>) $B"), new ArrayIterationKernel());
});