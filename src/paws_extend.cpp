#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXTEND("paws.extend", [](BlockExprAST* pkgScope) {
	defineStmt2(pkgScope, "stmt $E ... $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const std::vector<ExprAST*>& stmtParamsAST = getListExprASTExprs((ListExprAST*)params[0]);
			BlockExprAST* blockAST = (BlockExprAST*)params[1];

			// Collect parameters
			std::vector<ExprAST*> stmtParams;
			for (ExprAST* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			setBlockExprASTParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(parentBlock, stmtParamsAST, new PawsCodegenContext(blockAST, getVoid().type, blockParams));
		}
	);

	defineStmt2(pkgScope, "$E<PawsMetaType> expr $E $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsType* exprType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			ExprAST* exprParamAST = params[1];
			BlockExprAST* blockAST = (BlockExprAST*)params[2];

			// Collect parameters
			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			setBlockExprASTParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(parentBlock, exprParamAST, new PawsCodegenContext(blockAST, exprType, blockParams));
		}
	);
});