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
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			buildExpr(params[1], buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			if (runExpr(params[1], runtime))
				return true;
			PawsCastMap* casts = (PawsCastMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsCast value;
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, &value);
			bool cancel = false;
			iterateBlockExprCasts(casts->get(), [&](const MincCast* cast) {
				value.set(cast);
				cancel || (cancel = runExpr((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);

	defineStmt5(pkgScope, "$E<PawsCastMap>[$E<PawsType> -> $E<PawsType>] = $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			CastMap const casts = ((PawsCastMap*)buildExpr(params[0], buildtime).value)->get();
			MincBlockExpr* const scope = casts;
			PawsType* fromType = (PawsType*)buildExpr(params[1], buildtime).value;
			PawsType* toType = (PawsType*)buildExpr(params[2], buildtime).value;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Get block parameter types
			std::vector<MincSymbol> blockParams(1, MincSymbol(PawsTpltType::get(buildtime.parentBlock, PawsExpr::TYPE, fromType), nullptr));

			setBlockExprParent(blockAST, scope);
			definePawsReturnStmt(blockAST, toType);

			defineTypeCast3(scope, fromType, toType, new PawsKernel(blockAST, toType, buildtime, blockParams));
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