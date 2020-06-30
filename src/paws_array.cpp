#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<std::vector<MincObject*>> PawsArray;

MincPackage PAWS_ARRAY("paws.array", [](BlockExprAST* pkgScope) {
	registerType<PawsArray>(pkgScope, "PawsArray");

	// Inline array declaration
	defineExpr3(pkgScope, "[$E<PawsBase>, ...]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& values = getListExprASTExprs((ListExprAST*)params[0]);
			PawsArray* arr = new PawsArray(std::vector<MincObject*>());
			arr->get().reserve(values.size());
			for (ExprAST* value: values)
				arr->get().push_back(codegenExpr(value, parentBlock).value);
			return Variable(PawsTpltType::get(parentBlock, PawsArray::TYPE, (PawsType*)getType(getDerivedExprAST(values[0]), parentBlock)), arr);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			//TODO: Determine common sub-class, instead of enforcing identical classes of all array values
			std::vector<ExprAST*>& values = getListExprASTExprs((ListExprAST*)params[0]);
			std::vector<PawsType*> valueTypes;
			valueTypes.reserve(values.size());
			for (ExprAST* value: values)
				valueTypes.push_back((PawsType*)getType(getDerivedExprAST(value), parentBlock));
			return PawsTpltType::get(const_cast<BlockExprAST*>(parentBlock), PawsArray::TYPE, valueTypes[0]); //TODO: Remove const_cast
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
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			if (!ExprASTIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);

	// Array setter
	defineExpr3(pkgScope, "$E<PawsArray>[$E<PawsInt>] = $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable arrVar = codegenExpr(getDerivedExprAST(params[0]), parentBlock);
			PawsArray* arr = (PawsArray*)arrVar.value;
			PawsType* valueType = ((PawsTpltType*)arrVar.type)->tpltType;
			PawsInt* idx = (PawsInt*)codegenExpr(params[1], parentBlock).value;
			ExprAST* valueExpr = getCastExprASTSource((CastExprAST*)params[2]);

			MincObject *expectedType = valueType, *gotType = getType(valueExpr, parentBlock);
			if (expectedType != gotType)
			{
				ExprAST* castExpr = lookupCast(parentBlock, valueExpr, expectedType);
				if (castExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, valueExpr);
					throw CompileError(
						parentBlock, getLocation(valueExpr), "invalid conversion of %E from <%t> to <%t>\n%S",
						valueExpr, gotType, expectedType, candidateReport
					);
				}
				valueExpr = castExpr;
			}

			MincObject* value = ((PawsBase*)codegenExpr(valueExpr, parentBlock).value)->copy();
			arr->get()[idx->get()] = value;

			return Variable(valueType, value);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			if (!ExprASTIsCast(params[0]))
				raiseCompileError("attempting to access array of unspecified type", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
});