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
	registerType<PawsStmtMap>(pkgScope, "PawsStmtMap");
	registerType<PawsExprMap>(pkgScope, "PawsExprMap");
	registerType<PawsSymbolMap>(pkgScope, "PawsSymbolMap");

	defineExpr(pkgScope, "$E<PawsBlockExpr>.stmts",
		+[](MincBlockExpr* block) -> StmtMap {
			return StmtMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.exprs",
		+[](MincBlockExpr* block) -> ExprMap {
			return ExprMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.symbols",
		+[](MincBlockExpr* block) -> SymbolMap {
			return SymbolMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsStmtMap>.length",
		+[](StmtMap stmts) -> int {
			return countBlockExprStmts(stmts);
		}
	);
	defineStmt6_2(pkgScope, "for ($I, $I: $E<PawsStmtMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincListExpr*>::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincListExpr*>::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr2(params[2], runtime))
				return true;
			PawsStmtMap* stmts = (PawsStmtMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincListExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincListExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincListExpr*>::TYPE, &value);
			bool cancel = false;
			iterateBlockExprStmts(stmts->get(), [&](const MincListExpr* tplt, MincKernel* stmt) {
				key.set(tplt);
				cancel || (cancel = runExpr2((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);

	defineStmt6_2(pkgScope, "$E<PawsStmtMap>[$E ...] = $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			StmtMap const stmts = ((PawsStmtMap*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* const scope = stmts;
			const std::vector<MincExpr*>& stmtParamsAST = getListExprExprs((MincListExpr*)params[1]);
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			setBlockExprParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, blockParams));
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			// Set expr parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];
			setBlockExprParent(blockAST, runtime.parentBlock);
			return false;
		}
	);

	defineExpr(pkgScope, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return countBlockExprExprs(exprs);
		}
	);
	defineStmt6_2(pkgScope, "for ($I, $I: $E<PawsExprMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincExpr*>::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincExpr*>::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr2(params[2], runtime))
				return true;
			PawsExprMap* exprs = (PawsExprMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincExpr*>::TYPE, &value);
			bool cancel = false;
			iterateBlockExprExprs(exprs->get(), [&](const MincExpr* tplt, MincKernel* expr) {
				key.set(tplt);
				cancel || (cancel = runExpr2((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);
	defineStmt6_2(pkgScope, "$E<PawsExprMap>[$E] = <$I> $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			ExprMap const exprs = ((PawsExprMap*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* const scope = exprs;
			MincExpr* exprParamAST = params[1];
			buildExpr(params[2], parentBlock);
			MincObject* exprType = (MincObject*)runExpr(params[2], parentBlock).value;
			//TODO: Check for errors
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			setBlockExprParent(blockAST, parentBlock);
			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(scope, exprParamAST, new PawsKernel(blockAST, exprType, blockParams));
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			// Set expr parent (the parent may have changed during function cloning)
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];
			setBlockExprParent(blockAST, runtime.parentBlock);
			return false;
		}
	);

	defineExpr(pkgScope, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return countBlockExprSymbols(symbols);
		}
	);
	defineStmt6_2(pkgScope, "for ($I, $I: $E<PawsSymbolMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsSym::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr2(params[2], runtime))
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
				cancel || (cancel = runExpr2((MincExpr*)body, runtime));
			});
			return cancel;
		}
	);
	defineStmt6_2(pkgScope, "$E<PawsSymbolMap>[$I] = $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[2], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			SymbolMap const symbols = ((PawsSymbolMap*)runtime.result.value)->get();
			MincBlockExpr* const scope = symbols;
			MincIdExpr* symbolNameAST = (MincIdExpr*)params[1];
			if (runExpr2(params[2], runtime))
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