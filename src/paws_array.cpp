#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

typedef PawsValue<std::vector<BaseValue*>> PawsArray;

PawsPackage PAWS_ARRAY("array", [](BlockExprAST* pkgScope) {
	registerType<PawsArray>(pkgScope, "PawsArray");

	// Inline array declaration
	defineExpr3(pkgScope, "[$E<PawsBase>, ...]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& values = getExprListASTExpressions((ExprListAST*)params[0]);
			PawsArray* arr = new PawsArray(std::vector<BaseValue*>());
			arr->get().reserve(values.size());
			for (ExprAST* value: values)
				arr->get().push_back(codegenExpr(value, parentBlock).value);
			return Variable(PawsTpltType::get(PawsArray::TYPE, (PawsType*)getType(getDerivedExprAST(values[0]), parentBlock)), arr);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<ExprAST*>& values = getExprListASTExpressions((ExprListAST*)params[0]);
			std::vector<PawsType*> valueTypes;
			valueTypes.reserve(values.size());
			for (ExprAST* value: values)
				valueTypes.push_back((PawsType*)getType(getDerivedExprAST(value), parentBlock));
			return PawsTpltType::get(PawsArray::TYPE, valueTypes[0]);
		}
	);

	// Array getter
	defineExpr3(pkgScope, "$E<PawsArray>[$E<PawsInt>]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable arrVar = codegenExpr(getDerivedExprAST(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsTpltType*)arrVar.type)->tpltType;
			PawsInt* idx = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			return Variable(valueType, arr->get()[idx->get()]);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			if (!ExprASTIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);

	// Array setter
	defineExpr3(pkgScope, "$E<PawsArray>[$E<PawsInt>] = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable arrVar = codegenExpr(getDerivedExprAST(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsTpltType*)arrVar.type)->tpltType;
			PawsInt* idx = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			ExprAST* valueExpr = params[2];

			BaseType *expectedType = valueType, *gotType = getType(valueExpr, parentBlock);
			if (expectedType != gotType)
			{
				ExprAST* castExpr = lookupCast(parentBlock, valueExpr, expectedType);
				if (castExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, valueExpr);
					raiseCompileError(
						("invalid conversion of " + ExprASTToString(valueExpr) + " from <" + getTypeName(gotType) + "> to <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
						valueExpr
					);
				}
				valueExpr = castExpr;
			}

			BaseValue* value = codegenExpr(valueExpr, parentBlock).value;
			arr->get()[idx->get()] = value;

			return Variable(valueType, value);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			if (!ExprASTIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
});