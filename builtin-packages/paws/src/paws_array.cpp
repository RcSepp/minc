#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsArrayScope = nullptr;

typedef PawsValue<std::vector<MincObject*>> PawsArray;

struct PawsArrayType : public PawsType
{
private:
	static std::mutex mutex;
	static std::set<PawsArrayType> awaitableTypes;

	PawsArrayType(PawsType* valueType)
		: PawsType(sizeof(std::vector<MincObject*>)), valueType(valueType) {}

public:
	PawsType* const valueType;
	static PawsArrayType* get(PawsType* valueType)
	{
		std::unique_lock<std::mutex> lock(mutex);
		std::set<PawsArrayType>::iterator iter = awaitableTypes.find(PawsArrayType(valueType));
		if (iter == awaitableTypes.end())
		{
			iter = awaitableTypes.insert(PawsArrayType(valueType)).first;
			PawsArrayType* t = const_cast<PawsArrayType*>(&*iter); //TODO: Find a way to avoid const_cast
			t->name = valueType->name + "[]";
			defineSymbol(pawsArrayScope, t->name.c_str(), PawsType::TYPE, t);
			defineOpaqueInheritanceCast(pawsArrayScope, t, PawsBase::TYPE);
			defineOpaqueInheritanceCast(pawsArrayScope, t, PawsArray::TYPE);
		}
		return const_cast<PawsArrayType*>(&*iter); //TODO: Find a way to avoid const_cast
	}
	MincObject* copy(MincObject* value) { return new PawsArray(*(PawsArray*)value); }
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
std::mutex PawsArrayType::mutex;
std::set<PawsArrayType> PawsArrayType::awaitableTypes;
bool operator<(const PawsArrayType& lhs, const PawsArrayType& rhs)
{
	return lhs.valueType < rhs.valueType;
}

MincPackage PAWS_ARRAY("paws.array", [](MincBlockExpr* pkgScope) {
	pawsArrayScope = pkgScope;
	registerType<PawsArray>(pkgScope, "PawsArray");

	// Inline array declaration
	defineExpr10(pkgScope, "[$E<PawsBase>, ...]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			for (MincExpr* key: getListExprExprs((MincListExpr*)params[0]))
				buildExpr(key, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			PawsArray* arr = new PawsArray();
			if (values.size() == 0)
				return MincSymbol(PawsArrayType::get(PawsVoid::TYPE), arr);
			arr->get().reserve(values.size());
			for (MincExpr* value: values)
				arr->get().push_back(runExpr(value, parentBlock).value);
			return MincSymbol(PawsArrayType::get((PawsType*)getType(getDerivedExpr(values[0]), parentBlock)), arr);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			std::vector<PawsType*> valueTypes;
			if (values.size() == 0)
				return PawsArrayType::get(PawsVoid::TYPE);
			valueTypes.reserve(values.size());
			for (MincExpr* value: values)
				valueTypes.push_back((PawsType*)getType(getDerivedExpr(value), parentBlock));
			return PawsArrayType::get(valueTypes[0]);
		}
	);

	// Array getter
	defineExpr10(pkgScope, "$E<PawsArray>[$E<PawsInt>]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol arrVar = runExpr(getDerivedExpr(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsArrayType*)arrVar.type)->valueType;
			size_t idx = ((PawsInt*)runExpr(params[1], parentBlock).value)->get();
			if (idx >= arr->get().size())
				throw CompileError(parentBlock, getLocation(params[0]), "index %i out of bounds for array of length %i", (int)idx, (int)arr->get().size());
			return MincSymbol(valueType, arr->get()[idx]);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	);

	// Array setter
	defineExpr10(pkgScope, "$E<PawsArray>[$E<PawsInt>] = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);

			PawsType* valueType = ((PawsArrayType*)getType(getDerivedExpr(params[0]), parentBlock))->valueType;
			if (valueType == PawsVoid::TYPE)
				throw CompileError(parentBlock, getLocation(params[0]), "cannot assign to empty array");
			MincExpr* valueExpr = getCastExprSource((MincCastExpr*)params[2]);

			MincObject *expectedType = valueType, *gotType = getType(valueExpr, parentBlock);
			if (expectedType != gotType)
			{
				MincExpr* castExpr = lookupCast(parentBlock, valueExpr, expectedType);
				if (castExpr == nullptr)
					throw CompileError(parentBlock, getLocation(valueExpr), "invalid conversion of %E from <%t> to <%t>", valueExpr, gotType, expectedType);
				buildExpr(castExpr, parentBlock);
				valueExpr = castExpr;
			}

			buildExpr(params[2] = valueExpr, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol arrVar = runExpr(getDerivedExpr(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsArrayType*)arrVar.type)->valueType;
			size_t idx = ((PawsInt*)runExpr(params[1], parentBlock).value)->get();
			if (idx >= arr->get().size())
				throw CompileError(parentBlock, getLocation(params[0]), "index %i out of bounds for array of length %i", (int)idx, (int)arr->get().size());

			MincSymbol sym = runExpr(params[2], parentBlock);
			MincObject* value = ((PawsType*)sym.type)->copy((PawsBase*)sym.value);
			arr->get()[idx] = value;

			return MincSymbol(valueType, value);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsArrayType*)type)->valueType;
		}
	);

	defineExpr9(pkgScope, "$E<PawsType>[]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)runExpr(params[0], parentBlock).value;
			return MincSymbol(PawsType::TYPE, PawsArrayType::get(returnType));
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

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], parentBlock);
			PawsType* valueType = ((PawsArrayType*)::getType(getDerivedExpr(params[1]), parentBlock))->valueType;
			defineSymbol((MincBlockExpr*)params[2], getIdExprName((MincIdExpr*)params[0]), valueType, nullptr);
			MincSymbolId iterId = lookupSymbolId((MincBlockExpr*)params[2], getIdExprName((MincIdExpr*)params[0]));
			buildExpr(params[2], parentBlock);
			return new ArrayIterationKernel(iterId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbol arrVar = runExpr(getDerivedExpr(params[1]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			MincSymbol* iter = getSymbol(body, iterId);
			for (MincObject* value: arr->get())
			{
				iter->value = value;
				runExpr((MincExpr*)body, parentBlock);
			}
			return getVoid();
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "for ($I: $E<PawsArray>) $B", new ArrayIterationKernel());
});