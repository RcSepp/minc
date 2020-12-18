#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXTEND("paws.extend", [](MincBlockExpr* pkgScope) {
	defineStmt6_2(pkgScope, "stmt $E ... $B",
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
			definePawsReturnStmt(blockAST, PawsVoid::TYPE); //TODO: Move this line into PawsKernel

			defineStmt3(parentBlock, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, blockParams));
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			// Set stmt block parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[1];
			setBlockExprParent(blockAST, runtime.parentBlock);
			return false;
		}
	);

	defineStmt6_2(pkgScope, "$E expr $E $B",
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
			definePawsReturnStmt(blockAST, exprType); //TODO: Move this line into PawsKernel

			defineExpr5(parentBlock, exprParamAST, new PawsKernel(blockAST, exprType, blockParams));
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			// Set stmt block parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];
			setBlockExprParent(blockAST, runtime.parentBlock);
			return false;
		}
	);
});