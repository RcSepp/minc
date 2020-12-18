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

	defineStmt6_2(pkgScope, "for ($I: $E<PawsCastMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			buildExpr(params[1], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			if (runExpr2(params[1], runtime))
				return true;
			PawsCastMap* casts = (PawsCastMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsCast value;
			defineSymbol(body, getIdExprName(castExpr), PawsCast::TYPE, &value);
			bool cancel = false;
			iterateBlockExprCasts(casts->get(), [&](const MincCast* cast) {
				value.set(cast);
				cancel || (cancel = runExpr2((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);

	defineStmt6_2(pkgScope, "$E<PawsCastMap>[$E<PawsType> -> $E<PawsType>] = $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
			buildExpr(params[2], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			CastMap const casts = ((PawsCastMap*)runtime.result.value)->get();
			MincBlockExpr* const scope = casts;
			if (runExpr2(params[1], runtime))
				return true;
			PawsType* fromType = (PawsType*)runtime.result.value;
			if (runExpr2(params[2], runtime))
				return true;
			PawsType* toType = (PawsType*)runtime.result.value;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Get block parameter types
			std::vector<MincSymbol> blockParams(1, MincSymbol(PawsTpltType::get(runtime.parentBlock, PawsExpr::TYPE, fromType), nullptr));

			setBlockExprParent(blockAST, scope);
			definePawsReturnStmt(blockAST, toType);

			defineTypeCast3(scope, fromType, toType, new PawsKernel(blockAST, toType, blockParams));
			return false;
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