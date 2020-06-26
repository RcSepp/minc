#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<const Cast*> PawsCast;

struct CastMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsValue<CastMap> PawsCastMap;

MincPackage PAWS_CASTREG("paws.castreg", [](BlockExprAST* pkgScope) {
	registerType<PawsCastMap>(pkgScope, "PawsCastMap");
	registerType<PawsCast>(pkgScope, "PawsCast");

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.casts",
		+[](BlockExprAST* block) -> CastMap {
			return CastMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsCastMap>.length",
		+[](CastMap stmts) -> int {
			return countBlockExprASTCasts(stmts);
		}
	);

	defineStmt2(pkgScope, "for ($I: $E<PawsCastMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* castExpr = (IdExprAST*)params[0];
			PawsCastMap* casts = (PawsCastMap*)codegenExpr(params[1], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[2];
			PawsCast value;
			defineSymbol(body, getIdExprASTName(castExpr), PawsCast::TYPE, &value);
			iterateBlockExprASTCasts(casts->get(), [&](const Cast* cast) {
				value.set(cast);
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);

	defineStmt2(pkgScope, "$E<PawsCastMap>[$E<PawsType> -> $E<PawsType>] = $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			CastMap const casts = ((PawsCastMap*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* const scope = casts;
			PawsType* fromType = (PawsType*)codegenExpr(params[1], parentBlock).value;
			PawsType* toType = (PawsType*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* blockAST = (BlockExprAST*)params[3];

			// Get block parameter types
			std::vector<Variable> blockParams(1, Variable(PawsTpltType::get(PawsExprAST::TYPE, fromType), nullptr));

			setBlockExprASTParent(blockAST, scope);
			definePawsReturnStmt(blockAST, toType);

			defineTypeCast3(scope, fromType, toType, new PawsCodegenContext(blockAST, toType, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.fromType",
		+[](const Cast* cast) -> MincObject* {
			return cast->fromType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.toType",
		+[](const Cast* cast) -> MincObject* {
			return cast->toType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.cost",
		+[](const Cast* cast) -> int {
			return cast->getCost();
		}
	);
});