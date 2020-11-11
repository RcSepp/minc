#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<const MincCast*> PawsCast;

struct CastMap { MincBlockExpr* block; operator MincBlockExpr*() const { return block; } };
typedef PawsValue<CastMap> PawsCastMap;

MincPackage PAWS_CASTREG("paws.castreg", [](MincBlockExpr* pkgScope) {
	registerType<PawsCastMap>(pkgScope, "PawsCastMap");
	registerType<PawsCast>(pkgScope, "PawsCast");

	defineExpr(pkgScope, "$E<PawsBlockExpr>.casts",
		+[](MincBlockExpr* block) -> CastMap {
			return CastMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsCastMap>.length",
		+[](CastMap stmts) -> int {
			return countBlockExprCasts(stmts);
		}
	);

	defineStmt6(pkgScope, "for ($I: $E<PawsCastMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			buildExpr(params[1], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			PawsCastMap* casts = (PawsCastMap*)runExpr(params[1], parentBlock).value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsCast value;
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, &value);
			iterateBlockExprCasts(casts->get(), [&](const MincCast* cast) {
				value.set(cast);
				runExpr((MincExpr*)body, parentBlock);
			});
		}
	);

	defineStmt6(pkgScope, "$E<PawsCastMap>[$E<PawsType> -> $E<PawsType>] = $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
			buildExpr(params[2], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			CastMap const casts = ((PawsCastMap*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* const scope = casts;
			PawsType* fromType = (PawsType*)runExpr(params[1], parentBlock).value;
			PawsType* toType = (PawsType*)runExpr(params[2], parentBlock).value;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Get block parameter types
			std::vector<MincSymbol> blockParams(1, MincSymbol(PawsTpltType::get(parentBlock, PawsExpr::TYPE, fromType), nullptr));

			setBlockExprParent(blockAST, scope);
			definePawsReturnStmt(blockAST, toType);

			defineTypeCast3(scope, fromType, toType, new PawsKernel(blockAST, toType, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.fromType",
		+[](const MincCast* cast) -> MincObject* {
			return cast->fromType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.toType",
		+[](const MincCast* cast) -> MincObject* {
			return cast->toType;
		}
	);

	defineExpr(pkgScope, "$E<PawsCast>.cost",
		+[](const MincCast* cast) -> int {
			return cast->getCost();
		}
	);
});