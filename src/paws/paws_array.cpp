#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<std::vector<MincObject*>> PawsArray;

template<> const std::string PawsArray::toString() const
{
	const size_t size = val.size();
	if (size == 0)
		return "[]";
	else
	{
		std::string str = "[" + ((PawsBase*)val[0])->toString(); //TODO: Check if val is instance of PawsBase
		for (size_t i = 1; i < size; ++i)
			str += ", " + ((PawsBase*)val[i])->toString(); //TODO: Check if val is instance of PawsBase
		return str + ']';
	}
}

MincPackage PAWS_ARRAY("paws.array", [](MincBlockExpr* pkgScope) {
	registerType<PawsArray>(pkgScope, "PawsArray");

	// Inline array declaration
	defineExpr3(pkgScope, "[$E<PawsBase>, ...]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			PawsArray* arr = new PawsArray(std::vector<MincObject*>());
			if (values.size() == 0)
				return MincSymbol(PawsArray::TYPE, arr);
			arr->get().reserve(values.size());
			for (MincExpr* value: values)
				arr->get().push_back(codegenExpr(value, parentBlock).value);
			return MincSymbol(PawsTpltType::get(parentBlock, PawsArray::TYPE, (PawsType*)getType(getDerivedExpr(values[0]), parentBlock)), arr);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[0]);
			std::vector<PawsType*> valueTypes;
			if (values.size() == 0)
				return PawsArray::TYPE;
			valueTypes.reserve(values.size());
			for (MincExpr* value: values)
				valueTypes.push_back((PawsType*)getType(getDerivedExpr(value), parentBlock));
			return PawsTpltType::get(const_cast<MincBlockExpr*>(parentBlock), PawsArray::TYPE, valueTypes[0]); //TODO: Remove const_cast
		}
	);

	// Array getter
	defineExpr3(pkgScope, "$E<PawsArray>[$E<PawsInt>]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (!ExprIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			MincSymbol arrVar = codegenExpr(getDerivedExpr(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsTpltType*)arrVar.type)->tpltType;
			PawsInt* idx = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return MincSymbol(valueType, arr->get()[idx->get()]);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);

	// Array setter
	defineExpr3(pkgScope, "$E<PawsArray>[$E<PawsInt>] = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (!ExprIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			MincSymbol arrVar = codegenExpr(getDerivedExpr(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsTpltType*)arrVar.type)->tpltType;
			PawsInt* idx = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			MincExpr* valueExpr = getCastExprSource((MincCastExpr*)params[2]);

			MincObject *expectedType = valueType, *gotType = getType(valueExpr, parentBlock);
			if (expectedType != gotType)
			{
				MincExpr* castExpr = lookupCast(parentBlock, valueExpr, expectedType);
				if (castExpr == nullptr)
					throw CompileError(parentBlock, getLocation(valueExpr), "invalid conversion of %E from <%t> to <%t>", valueExpr, gotType, expectedType);
				valueExpr = castExpr;
			}

			MincObject* value = ((PawsBase*)codegenExpr(valueExpr, parentBlock).value)->copy();
			arr->get()[idx->get()] = value;

			return MincSymbol(valueType, value);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return getErrorType();
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
});