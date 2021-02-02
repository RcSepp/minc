#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXTEND("paws.extend", [](MincBlockExpr* pkgScope) {
	defineStmt5(pkgScope, "stmt $E ... $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			const std::vector<MincExpr*>& stmtParamsAST = getListExprExprs((MincListExpr*)params[0]);
			MincBlockExpr* blockAST = (MincBlockExpr*)params[1];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
				collectParams(buildtime.parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, stmtParams, blockParams);

			setBlockExprParams(blockAST, blockParams);
			setBlockExprParent(blockAST, buildtime.parentBlock);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE); //TODO: Move this line into PawsKernel

			defineStmt3(buildtime.parentBlock, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, buildtime, blockParams));
		}
	);

	defineStmt5(pkgScope, "$E expr $E $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincObject* exprType = buildExpr(params[0], buildtime).value;
			MincExpr* exprParamAST = params[1];
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			collectParams(buildtime.parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, exprParams, blockParams);

			setBlockExprParams(blockAST, blockParams);
			setBlockExprParent(blockAST, buildtime.parentBlock);
			definePawsReturnStmt(blockAST, exprType); //TODO: Move this line into PawsKernel

			defineExpr5(buildtime.parentBlock, exprParamAST, new PawsKernel(blockAST, exprType, buildtime, blockParams));
		}
	);
});