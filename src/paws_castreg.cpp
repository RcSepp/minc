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
			iterateBlockExprASTCasts(casts->get(), [&](const Cast* cast) {
				value.set(cast);
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);

	defineStmt2(pkgScope, "$E<PawsCastMap>[$E<PawsMetaType> -> $E<PawsMetaType>] = $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			CastMap const casts = ((PawsCastMap*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* const scope = casts;
			BaseType* fromType = ((PawsMetaType*)codegenExpr(params[1], parentBlock).value)->get();
			BaseType* toType = ((PawsMetaType*)codegenExpr(params[2], parentBlock).value)->get();
			BlockExprAST* blockAST = (BlockExprAST*)params[3];

			// Get block parameter types
			std::vector<Variable> blockParams(1, Variable(PawsTpltType::get(PawsExprAST::TYPE, fromType), nullptr));

			definePawsReturnStmt(blockAST, toType);

			defineTypeCast3(parentBlock, fromType, toType, new PawsCodegenContext(blockAST, toType, blockParams));
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