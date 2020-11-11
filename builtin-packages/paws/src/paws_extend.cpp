#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXTEND("paws.extend", [](MincBlockExpr* pkgScope) {
	defineStmt6(pkgScope, "stmt $E ... $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			const std::vector<MincExpr*>& stmtParamsAST = getListExprExprs((MincListExpr*)params[0]);
			MincBlockExpr* blockAST = (MincBlockExpr*)params[1];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			setBlockExprParams(blockAST, blockParams);
			setBlockExprParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(parentBlock, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, blockParams));
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			// Set stmt block parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[1];
			setBlockExprParent(blockAST, parentBlock);
		}
	);

	defineStmt6(pkgScope, "$E expr $E $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			MincObject* exprType = runExpr(params[0], parentBlock).value;
			MincExpr* exprParamAST = params[1];
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			setBlockExprParams(blockAST, blockParams);
			setBlockExprParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(parentBlock, exprParamAST, new PawsKernel(blockAST, exprType, blockParams));
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			// Set stmt block parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];
			setBlockExprParent(blockAST, parentBlock);
		}
	);
});