#include <cassert>
#include "minc_api.hpp"
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
			return stmts.block->countStmts();
		}
	);

	class StmtIterationKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			params[2]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			body->defineSymbol(keyExpr->name, PawsValue<const MincListExpr*>::TYPE, nullptr);
			body->defineSymbol(valueExpr->name, PawsValue<const MincListExpr*>::TYPE, nullptr);
			body->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (params[2]->run(runtime))
				return true;
			PawsStmtMap* stmts = (PawsStmtMap*)runtime.result;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincListExpr*> key, value;
			body->defineSymbol(keyExpr->name, PawsValue<const MincListExpr*>::TYPE, &key);
			body->defineSymbol(valueExpr->name, PawsValue<const MincListExpr*>::TYPE, &value);
			bool cancel = false;
			stmts->get().block->iterateStmts([&](const MincListExpr* tplt, MincKernel* stmt) {
				key.set(tplt);
				cancel || (cancel = body->run(runtime));
			});
			return cancel;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I, $I: $E<PawsStmtMap>) $B"), new StmtIterationKernel());

	class StmtDefinitionKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			StmtMap const stmts = ((PawsStmtMap*)params[0]->build(buildtime).value)->get();
			MincBlockExpr* const scope = stmts;
			const std::vector<MincExpr*>& stmtParamsAST = ((MincListExpr*)params[1])->exprs;
			MincBlockExpr* blockAST = (MincBlockExpr*)params[2];

			// Collect parameters
			std::vector<MincExpr*> stmtParams;
			for (MincExpr* stmtParam: stmtParamsAST)
			{
				size_t paramIdx = stmtParams.size();
				stmtParam->collectParams(buildtime.parentBlock, stmtParam, stmtParams, paramIdx);
			}

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, stmtParams, blockParams);

			blockAST->parent = buildtime.parentBlock;
			definePawsReturnStmt(blockAST, PawsVoid::TYPE);

			std::vector<MincExpr*> tplt(stmtParamsAST);
			tplt.push_back(new MincStopExpr(MincLocation{}));
			scope->defineStmt(tplt, new PawsKernel(blockAST, getVoid().type, buildtime, blockParams));
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
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsStmtMap>[$E ...] = $B"), new StmtDefinitionKernel());

	defineExpr(pkgScope, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return exprs.block->countExprs();
		}
	);

	class ExprIterationKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			params[2]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			body->defineSymbol(keyExpr->name, PawsValue<const MincExpr*>::TYPE, nullptr);
			body->defineSymbol(valueExpr->name, PawsValue<const MincExpr*>::TYPE, nullptr);
			body->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (params[2]->run(runtime))
				return true;
			PawsExprMap* exprs = (PawsExprMap*)runtime.result;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsValue<const MincExpr*> key, value;
			body->defineSymbol(keyExpr->name, PawsValue<const MincExpr*>::TYPE, &key);
			body->defineSymbol(valueExpr->name, PawsValue<const MincExpr*>::TYPE, &value);
			bool cancel = false;
			exprs->get().block->iterateExprs([&](const MincExpr* tplt, MincKernel* expr) {
				key.set(tplt);
				cancel || (cancel = body->run(runtime));
			});
			return cancel;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I, $I: $E<PawsExprMap>) $B"), new ExprIterationKernel());

	class ExprDefinitionKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			ExprMap const exprs = ((PawsExprMap*)params[0]->build(buildtime).value)->get();
			MincBlockExpr* const scope = exprs;
			MincExpr* exprParamAST = params[1];
			MincObject* exprType = (MincObject*)params[2]->build(buildtime).value;
			if (exprType == nullptr)
				throw CompileError(buildtime.parentBlock, params[1]->loc, "expression type must be build time constant");
			//TODO: Check for errors
			MincBlockExpr* blockAST = (MincBlockExpr*)params[3];

			// Collect parameters
			std::vector<MincExpr*> exprParams;
			size_t paramIdx = exprParams.size();
			exprParamAST->collectParams(buildtime.parentBlock, exprParamAST, exprParams, paramIdx);

			// Get block parameter types
			std::vector<MincSymbol> blockParams;
			getBlockParameterTypes(buildtime.parentBlock, exprParams, blockParams);

			blockAST->parent = buildtime.parentBlock;
			definePawsReturnStmt(blockAST, exprType);

			scope->defineExpr(exprParamAST, new PawsKernel(blockAST, exprType, buildtime, blockParams));
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
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsExprMap>[$E] = <$I> $B"), new ExprDefinitionKernel());

	defineExpr(pkgScope, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return symbols.block->countSymbols();
		}
	);

	class SymbolIterationKernel : public MincKernel
	{
	public:
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			params[2]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			body->defineSymbol(keyExpr->name, PawsString::TYPE, nullptr);
			body->defineSymbol(valueExpr->name, PawsSym::TYPE, nullptr);
			body->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (params[2]->run(runtime))
				return true;
			PawsSymbolMap* symbols = (PawsSymbolMap*)runtime.result;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsString key;
			PawsSym value;
			body->defineSymbol(keyExpr->name, PawsString::TYPE, &key);
			body->defineSymbol(valueExpr->name, PawsSym::TYPE, &value);
			bool cancel = false;
			symbols->get().block->iterateBuildtimeSymbols([&](const std::string& name, const MincSymbol& symbol) {
				key.set(name);
				value.set(symbol);
				cancel || (cancel = body->run(runtime));
			});
			return cancel;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I, $I: $E<PawsSymbolMap>) $B"), new SymbolIterationKernel());

	class SymbolDefinitionKernel : public MincKernel
	{
		MincObject* const symbolType;
	public:
		SymbolDefinitionKernel(MincObject* symbolType=nullptr) : symbolType(symbolType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[2]->build(buildtime);
			return new SymbolDefinitionKernel(params[2]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			SymbolMap const symbols = ((PawsSymbolMap*)runtime.result)->get();
			MincBlockExpr* const scope = symbols;
			MincIdExpr* symbolNameAST = (MincIdExpr*)params[1];
			if (params[2]->run(runtime))
				return true;

			scope->defineSymbol(symbolNameAST->name, symbolType, runtime.result);
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsSymbolMap>[$I] = $E"), new SymbolDefinitionKernel());

	defineExpr(pkgScope, "stmtreg.stats",
		+[]() -> std::string {
			std::string stats = "resolved " + std::to_string(EXPR_RESOLVE_COUNTER) + " expressions and " + std::to_string(STMT_RESOLVE_COUNTER) + " statements";
			EXPR_RESOLVE_COUNTER = STMT_RESOLVE_COUNTER = 0;
			return stats;
		}
	);
});