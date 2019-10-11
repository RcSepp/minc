#include <cassert>
#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

typedef PawsType<const Cast*> PawsCast;

struct CastMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<CastMap> PawsCastMap;

PawsPackage PAWS_CASTREG("castreg", [](BlockExprAST* pkgScope) {
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
			iterateBlockExprASTCasts(casts->val, [&](const Cast* cast) {
				value.val = cast;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.fromType",
		+[](const Cast* cast) -> BaseType* {
			return cast->fromType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.toType",
		+[](const Cast* cast) -> BaseType* {
			return cast->toType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.cost",
		+[](const Cast* cast) -> int {
			return cast->getCost();
		}
	);
});