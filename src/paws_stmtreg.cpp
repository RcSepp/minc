#include <cassert>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

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
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsStmtMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			PawsStmtMap* stmts = (PawsStmtMap*)codegenExpr(params[2], parentBlock).value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincListExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincListExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincListExpr*>::TYPE, &value);
			iterateBlockExprStmts(stmts->get(), [&](const MincListExpr* tplt, const MincKernel* stmt) {
				key.set(tplt);
				codegenExpr((MincExpr*)body, parentBlock);
			});
		}
	);

	defineStmt2(pkgScope, "$E<PawsStmtMap>[$E ...] = $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			StmtMap const stmts = ((PawsStmtMap*)codegenExpr(params[0], parentBlock).value)->get();
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

			setBlockExprParent(blockAST, scope);
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new PawsKernel(blockAST, getVoid().type, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return countBlockExprExprs(exprs);
		}
	);
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsExprMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			PawsExprMap* exprs = (PawsExprMap*)codegenExpr(params[2], parentBlock).value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincExpr*> key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsValue<const MincExpr*>::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsValue<const MincExpr*>::TYPE, &value);
			iterateBlockExprExprs(exprs->get(), [&](const MincExpr* tplt, const MincKernel* expr) {
				key.set(tplt);
				codegenExpr((MincExpr*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsExprMap>[$E] = <$I> $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			ExprMap const exprs = ((PawsExprMap*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* const scope = exprs;
			MincExpr* exprParamAST = params[1];
			MincObject* exprType = (MincObject*)codegenExpr(params[2], parentBlock).value;
			//TODO: Check for errors
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			setBlockExprParent(blockAST, scope);
			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(scope, exprParamAST, new PawsKernel(blockAST, exprType, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return countBlockExprSymbols(symbols);
		}
	);
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsSymbolMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			PawsSymbolMap* symbols = (PawsSymbolMap*)codegenExpr(params[2], parentBlock).value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsString key;
			PawsSym value;
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsSym::TYPE, &value);
			iterateBlockExprSymbols(symbols->get(), [&](const std::string& name, const MincSymbol& symbol) {
				key.set(name);
				value.set(symbol);
				codegenExpr((MincExpr*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsSymbolMap>[$I] = $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			SymbolMap const symbols = ((PawsSymbolMap*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* const scope = symbols;
			MincIdExpr* symbolNameAST = (MincIdExpr*)params[1];
			const MincSymbol& symbol = codegenExpr(params[2], parentBlock);

			defineSymbol(scope, getIdExprName(symbolNameAST), symbol.type, symbol.value);
		}
	);
});