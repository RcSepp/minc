#include <cassert>
#include "minc_api.hpp"
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
			return stmts.block->countCasts();
		}
	);

	class CastMapIterationKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			params[1]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			body->defineSymbol(castExpr->name, PawsCast::TYPE, nullptr);
			body->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincIdExpr* castExpr = (MincIdExpr*)params[0];
			if (params[1]->run(runtime))
				return true;
			PawsCastMap* casts = (PawsCastMap*)runtime.result;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsCast value;
			body->defineSymbol(castExpr->name, PawsCast::TYPE, &value);
			bool cancel = false;
			casts->get().block->iterateCasts([&](const MincCast* cast) {
				value.set(cast);
				cancel || (cancel = body->run(runtime));
			});
			return cancel;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I: $E<PawsCastMap>) $B"), new CastMapIterationKernel());

	class CastDefinitionKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			CastMap const casts = ((PawsCastMap*)params[0]->build(buildtime).value)->get();
			MincBlockExpr* const scope = casts;
			PawsType* fromType = (PawsType*)params[1]->build(buildtime).value;
			PawsType* toType = (PawsType*)params[2]->build(buildtime).value;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Get block parameter types
			std::vector<MincSymbol> blockParams(1, MincSymbol(PawsTpltType::get(buildtime.parentBlock, PawsExpr::TYPE, fromType), nullptr));

			blockAST->parent = scope;
			definePawsReturnStmt(blockAST, toType);

			scope->defineCast(new TypeCast(fromType, toType, new PawsKernel(blockAST, toType, buildtime, blockParams)));
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsCastMap>[$E<PawsType> -> $E<PawsType>] = $B"), new CastDefinitionKernel());

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