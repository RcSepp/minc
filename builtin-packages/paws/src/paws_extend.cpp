#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXTEND("paws.extend", [](MincBlockExpr* pkgScope) {
	class StmtDefinitionKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::vector<MincExpr*>& stmtParamsAST = ((MincListExpr*)params[0])->exprs;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[1];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
			{
				size_t paramIdx = stmtParams.size();
				stmtParam->collectParams(buildtime.parentBlock, stmtParam, stmtParams, paramIdx);
			}

			// Get block parameter types
			getBlockParameterTypes(buildtime.parentBlock, stmtParams, blockAST->blockParams);

			blockAST->parent = buildtime.parentBlock;
			definePawsReturnStmt(blockAST, PawsVoid::TYPE); //TODO: Move this line into PawsKernel

			std::vector<MincExpr*> tplt(stmtParamsAST);
			tplt.push_back(new MincStopExpr(MincLocation{}));
			buildtime.parentBlock->defineStmt(tplt, new PawsKernel(blockAST, getVoid().type, buildtime, blockAST->blockParams));
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("stmt $E ... $B"), new StmtDefinitionKernel());

	class ExprDefinitionKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincObject* exprType = params[0]->build(buildtime).value;
			MincExpr* exprParamAST = params[1];
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			size_t paramIdx = exprParams.size();
			exprParamAST->collectParams(buildtime.parentBlock, exprParamAST, exprParams, paramIdx);

			// Get block parameter types
			getBlockParameterTypes(buildtime.parentBlock, exprParams, blockAST->blockParams);

			blockAST->parent = buildtime.parentBlock;
			definePawsReturnStmt(blockAST, exprType); //TODO: Move this line into PawsKernel

			buildtime.parentBlock->defineExpr(exprParamAST, new PawsKernel(blockAST, exprType, buildtime, blockAST->blockParams));
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E expr $E $B"), new ExprDefinitionKernel());
});