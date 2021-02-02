#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

extern unsigned long long EXPR_RESOLVE_COUNTER, STMT_RESOLVE_COUNTER;

struct StmtMap { MincBlockExpr* block; operator MincBlockExpr*() const { return block; } };
typedef PawsValue<StmtMap> PawsStmtMap;

struct ExprMap { MincBlockExpr* block; operator MincBlockExpr*() const { return block; } };
typedef PawsValue<ExprMap> PawsExprMap;

struct SymbolMap { MincBlockExpr* block; operator MincBlockExpr*() const { return block; } };
typedef PawsValue<SymbolMap> PawsSymbolMap;

MincPackage PAWS_STMTREG("paws.stmtreg", [](MincBlockExpr* pkgScope) {
	registerType<PawsStmtMap>(pkgScope, "PawsStmtMap", true);
	registerType<PawsExprMap>(pkgScope, "PawsExprMap", true);
	registerType<PawsSymbolMap>(pkgScope, "PawsSymbolMap", true);

	defineExpr7(pkgScope, "$E<PawsStaticBlockExpr>.stmts",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincBlockExpr* block = ((PawsStaticBlockExpr*)buildExpr(params[0], buildtime).value)->get();
			buildtime.result = MincSymbol(PawsStmtMap::TYPE, new PawsStmtMap(StmtMap{block}));
		},
		PawsStmtMap::TYPE
	);

	defineExpr7(pkgScope, "$E<PawsStaticBlockExpr>.exprs",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincBlockExpr* block = ((PawsStaticBlockExpr*)buildExpr(params[0], buildtime).value)->get();
			buildtime.result = MincSymbol(PawsExprMap::TYPE, new PawsExprMap(ExprMap{block}));
		},
		PawsExprMap::TYPE
	);

	defineExpr7(pkgScope, "$E<PawsStaticBlockExpr>.symbols",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincBlockExpr* block = ((PawsStaticBlockExpr*)buildExpr(params[0], buildtime).value)->get();
			buildtime.result = MincSymbol(PawsSymbolMap::TYPE, new PawsSymbolMap(SymbolMap{block}));
		},
		PawsSymbolMap::TYPE
	);

	defineExpr(pkgScope, "$E<PawsStmtMap>.length",
		+[](StmtMap stmts) -> int {
			return countBlockExprStmts(stmts);
		}
	);
	defineStmt6(pkgScope, "for ($I, $I: $E<PawsStmtMap>) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincListExpr*>::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincListExpr*>::TYPE, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr(params[2], runtime))
				return true;
			PawsStmtMap* stmts = (PawsStmtMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincListExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincListExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincListExpr*>::TYPE, &value);
			bool cancel = false;
			iterateBlockExprStmts(stmts->get(), [&](const MincListExpr* tplt, MincKernel* stmt) {
				key.set(tplt);
				cancel || (cancel = runExpr((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);

	defineStmt5(pkgScope, "$E<PawsStmtMap>[$E ...] = $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			StmtMap const stmts = ((PawsStmtMap*)buildExpr(params[0], buildtime).value)->get();
			MincBlockExpr* const scope = stmts;
			const std::vector<MincExpr*>& stmtParamsAST = getListExprExprs((MincListExpr*)params[1]);
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
				collectParams(buildtime.parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, stmtParams, blockParams);

			setBlockExprParent(blockAST, buildtime.parentBlock);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, buildtime, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return countBlockExprExprs(exprs);
		}
	);
	defineStmt6(pkgScope, "for ($I, $I: $E<PawsExprMap>) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincExpr*>::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincExpr*>::TYPE, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr(params[2], runtime))
				return true;
			PawsExprMap* exprs = (PawsExprMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincExpr*>::TYPE, &value);
			bool cancel = false;
			iterateBlockExprExprs(exprs->get(), [&](const MincExpr* tplt, MincKernel* expr) {
				key.set(tplt);
				cancel || (cancel = runExpr((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);
	defineStmt5(pkgScope, "$E<PawsExprMap>[$E] = <$I> $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			ExprMap const exprs = ((PawsExprMap*)buildExpr(params[0], buildtime).value)->get();
			MincBlockExpr* const scope = exprs;
			MincExpr* exprParamAST = params[1];
			MincObject* exprType = (MincObject*)buildExpr(params[2], buildtime).value;
			if (exprType == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[1]), "expression type must be build time constant");
			//TODO: Check for errors
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			collectParams(buildtime.parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, exprParams, blockParams);

			setBlockExprParent(blockAST, buildtime.parentBlock);
			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(scope, exprParamAST, new PawsKernel(blockAST, exprType, buildtime, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return countBlockExprSymbols(symbols);
		}
	);
	defineStmt6(pkgScope, "for ($I, $I: $E<PawsSymbolMap>) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsSym::TYPE, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr(params[2], runtime))
				return true;
			PawsSymbolMap* symbols = (PawsSymbolMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsString key;
			PawsSym value;
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsSym::TYPE, &value);
			bool cancel = false;
			iterateBlockExprSymbols(symbols->get(), [&](const std::string& name, const MincSymbol& symbol) {
				key.set(name);
				value.set(symbol);
				cancel || (cancel = runExpr((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);
	defineStmt6(pkgScope, "$E<PawsSymbolMap>[$I] = $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[2], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			SymbolMap const symbols = ((PawsSymbolMap*)runtime.result.value)->get();
			MincBlockExpr* const scope = symbols;
			MincIdExpr* symbolNameAST = (MincIdExpr*)params[1];
			if (runExpr(params[2], runtime))
				return true;

			defineSymbol(scope, getIdExprName(symbolNameAST), runtime.result.type, runtime.result.value);
			return false;
		}
	);

	defineExpr(pkgScope, "stmtreg.stats",
		+[]() -> std::string {
			std::string stats = "resolved " + std::to_string(EXPR_RESOLVE_COUNTER) + " expressions and " + std::to_string(STMT_RESOLVE_COUNTER) + " statements";
			EXPR_RESOLVE_COUNTER = STMT_RESOLVE_COUNTER = 0;
			return stats;
		}
	);
});