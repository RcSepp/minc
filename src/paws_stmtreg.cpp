#include <cassert>
#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

struct StmtMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<StmtMap> PawsStmtMap;

struct ExprMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<ExprMap> PawsExprMap;

struct SymbolMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<SymbolMap> PawsSymbolMap;

PawsPackage PAWS_STMTREG("stmtreg", [](BlockExprAST* pkgScope) {
	registerType<PawsStmtMap>(pkgScope, "PawsStmtMap");
	registerType<PawsExprMap>(pkgScope, "PawsExprMap");
	registerType<PawsSymbolMap>(pkgScope, "PawsSymbolMap");

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.stmts",
		+[](BlockExprAST* block) -> StmtMap {
			return StmtMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.exprs",
		+[](BlockExprAST* block) -> ExprMap {
			return ExprMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.symbols",
		+[](BlockExprAST* block) -> SymbolMap {
			return SymbolMap{block};
		}
	);

	defineExpr(pkgScope, "$E<PawsStmtMap>.length",
		+[](StmtMap stmts) -> int {
			return countBlockExprASTStmts(stmts);
		}
	);
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsStmtMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsStmtMap* stmts = (PawsStmtMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsType<const ExprListAST*> key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsType<const ExprListAST*>::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsType<const ExprListAST*>::TYPE, &value);
			iterateBlockExprASTStmts(stmts->get(), [&](const ExprListAST* tplt, const CodegenContext* stmt) {
				key.set(tplt);
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);

	defineStmt2(pkgScope, "$E<PawsStmtMap>[$E ...] = $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			StmtMap const stmts = ((PawsStmtMap*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* const scope = stmts;
			const std::vector<ExprAST*>& stmtParamsAST = getExprListASTExpressions((ExprListAST*)params[1]);
			BlockExprAST* blockAST = (BlockExprAST*)params[2];

			// Collect parameters
			std::vector<ExprAST*> stmtParams;
			for (ExprAST* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new PawsCodegenContext(blockAST, getVoid().type, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return countBlockExprASTExprs(exprs);
		}
	);
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsExprMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsExprMap* exprs = (PawsExprMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsType<const ExprAST*> key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsType<const ExprAST*>::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsType<const ExprAST*>::TYPE, &value);
			iterateBlockExprASTExprs(exprs->get(), [&](const ExprAST* tplt, const CodegenContext* expr) {
				key.set(tplt);
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsExprMap>[$E] = <$I> $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			ExprMap const exprs = ((PawsExprMap*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* const scope = exprs;
			ExprAST* exprParamAST = params[1];
			BaseType* exprType = (BaseType*)codegenExpr(params[2], parentBlock).value->getConstantValue();
			//TODO: Check for errors
			BlockExprAST* blockAST = (BlockExprAST*)params[3];

			// Collect parameters
			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			definePawsReturnStmt(blockAST, exprType);

			defineExpr5(scope, exprParamAST, new PawsCodegenContext(blockAST, exprType, blockParams));
		}
	);

	defineExpr(pkgScope, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return countBlockExprASTSymbols(symbols);
		}
	);
	defineStmt2(pkgScope, "for ($I, $I: $E<PawsSymbolMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsSymbolMap* symbols = (PawsSymbolMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsString key;
			PawsVariable value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsVariable::TYPE, &value);
			iterateBlockExprASTSymbols(symbols->get(), [&](const std::string& name, const Variable& symbol) {
				key.set(name);
				value.set(symbol);
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsSymbolMap>[$I] = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			SymbolMap const symbols = ((PawsSymbolMap*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* const scope = symbols;
			IdExprAST* symbolNameAST = (IdExprAST*)params[1];
			const Variable& symbol = codegenExpr(params[2], parentBlock);

			defineSymbol(scope, getIdExprASTName(symbolNameAST), symbol.type, symbol.value);
		}
	);
});