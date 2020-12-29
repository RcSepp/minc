#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsArrayScope = nullptr;

typedef PawsValue<std::vector<MincObject*>> PawsArray;

struct PawsArrayType : public PawsType
{
private:
	static std::recursive_mutex mutex;
	static std::set<PawsArrayType> arrayTypes;

	PawsArrayType(PawsType* valueType)
		: PawsType(sizeof(std::vector<MincObject*>)), valueType(valueType) {}

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
			defineSymbol(pawsArrayScope, t->name.c_str(), PawsType::TYPE, t);
			defineOpaqueInheritanceCast(pawsArrayScope, t, PawsBase::TYPE);
			defineOpaqueInheritanceCast(pawsArrayScope, t, PawsArray::TYPE);
			iterateBases(scope, valueType, [&](MincObject* baseType) {
				defineOpaqueInheritanceCast(pawsArrayScope, t, get(scope, (PawsType*)baseType)); //TODO: Should create new array with all elements casted to baseType
			});
		}
		return const_cast<PawsArrayType*>(&*iter); //TODO: Find a way to avoid const_cast
	}
	MincObject* copy(MincObject* value) { return value; }//new PawsArray(*(PawsArray*)value); }
	std::string toString(MincObject* value) const
	{
		const std::vector<MincObject*>& val = ((PawsArray*)value)->get();
		const size_t size = val.size();
		if (size == 0)
			return "[]";
		else
		{
			std::string str = "[" + PawsInt::TYPE->toString(val[0]); //TODO: Check if val is instance of PawsBase
			for (size_t i = 1; i < size; ++i)
				str += ", " + PawsInt::TYPE->toString(val[i]); //TODO: Check if val is instance of PawsBase
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
	defineExpr10(pkgScope, "[$E<PawsBase>, ...]",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			for (MincExpr* key: getListExprExprs((MincListExpr*)params[0]))
				buildExpr(key, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			PawsArray* arr = new PawsArray();
			if (values.size() == 0)
			{
				runtime.result = MincSymbol(PawsArrayType::get(runtime.parentBlock, PawsVoid::TYPE), arr);
				return false;
			}
			arr->get().reserve(values.size());
			for (MincExpr* value: values)
			{
				if (runExpr(value, runtime))
					return true;
				arr->get().push_back(runtime.result.value);
			}
			runtime.result = MincSymbol(PawsArrayType::get(runtime.parentBlock, (PawsType*)getType(getDerivedExpr(values[0]), runtime.parentBlock)), arr);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			std::vector<PawsType*> valueTypes;
			if (values.size() == 0)
				return PawsArrayType::get(parentBlock, PawsVoid::TYPE);
			valueTypes.reserve(values.size());
			for (MincExpr* value: values)
				valueTypes.push_back((PawsType*)getType(getDerivedExpr(value), parentBlock));
			return PawsArrayType::get(parentBlock, valueTypes[0]);
		}
	);

	// Array getter
	defineExpr10(pkgScope, "$E<PawsArray>[$E<PawsInt>]",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(getDerivedExpr(params[0]), runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			PawsType* valueType = ((PawsArrayType*)runtime.result.type)->valueType;
			if (runExpr(params[1], runtime))
				return true;
			size_t idx = ((PawsInt*)runtime.result.value)->get();
			if (idx >= arr->get().size())
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "index %i out of bounds for array of length %i", (int)idx, (int)arr->get().size());
			runtime.result = MincSymbol(valueType, arr->get()[idx]);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	);

	// Array setter
	defineExpr10(pkgScope, "$E<PawsArray>[$E<PawsInt>] = $E<PawsBase>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);

			PawsType* valueType = ((PawsArrayType*)getType(getDerivedExpr(params[0]), buildtime.parentBlock))->valueType;
			if (valueType == PawsVoid::TYPE)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "cannot assign to empty array");
			MincExpr* valueExpr = getCastExprSource((MincCastExpr*)params[2]);

			MincObject *expectedType = valueType, *gotType = getType(valueExpr, buildtime.parentBlock);
			if (expectedType != gotType)
			{
				MincExpr* castExpr = lookupCast(buildtime.parentBlock, valueExpr, expectedType);
				if (castExpr == nullptr)
					throw CompileError(buildtime.parentBlock, getLocation(valueExpr), "invalid conversion of %E from <%t> to <%t>", valueExpr, gotType, expectedType);
				buildExpr(castExpr, buildtime);
				valueExpr = castExpr;
			}

			buildExpr(params[2] = valueExpr, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(getDerivedExpr(params[0]), runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			PawsType* valueType = ((PawsArrayType*)runtime.result.type)->valueType;
			if (runExpr(params[1], runtime))
				return true;
			size_t idx = ((PawsInt*)runtime.result.value)->get();
			if (idx >= arr->get().size())
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "index %i out of bounds for array of length %i", (int)idx, (int)arr->get().size());

			if (runExpr(params[2], runtime))
				return true;
			MincObject* value = ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value);
			arr->get()[idx] = value;

			runtime.result = MincSymbol(valueType, value);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	);

	defineExpr7(pkgScope, "$E<PawsType>[]",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			PawsType* returnType = (PawsType*)buildtime.result.value;
			buildtime.result = MincSymbol(PawsType::TYPE, PawsArrayType::get(buildtime.parentBlock, returnType));
		},
		PawsType::TYPE
	);

	// Define array iterating for statement
	class ArrayIterationKernel : public MincKernel
	{
		const MincSymbolId iterId;
	public:
		ArrayIterationKernel() : iterId(MincSymbolId::NONE) {}
		ArrayIterationKernel(MincSymbolId iterId) : iterId(iterId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			PawsType* valueType = ((PawsArrayType*)::getType(getDerivedExpr(params[1]), buildtime.parentBlock))->valueType;
			defineSymbol((MincBlockExpr*)params[2], getIdExprName((MincIdExpr*)params[0]), valueType, nullptr);
			MincSymbolId iterId = lookupSymbolId((MincBlockExpr*)params[2], getIdExprName((MincIdExpr*)params[0]));
			buildExpr(params[2], buildtime);
			return new ArrayIterationKernel(iterId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(getDerivedExpr(params[1]), runtime))
				return true;
			PawsArray* arr = (PawsArray*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			MincSymbol* iter = getSymbol(body, iterId);
			for (MincObject* value: arr->get())
			{
				iter->value = value;
				if (runExpr((MincExpr*)body, runtime))
					return true;
			}
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "for ($I: $E<PawsArray>) $B", new ArrayIterationKernel());
});