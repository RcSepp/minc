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

struct StmtContext : public CodegenContext
{
private:
	BlockExprAST* const stmt;
	std::vector<Variable> blockParams;
public:
	StmtContext(BlockExprAST* stmt, const std::vector<Variable>& blockParams)
		: stmt(stmt), blockParams(blockParams) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		// Set block parameters
		for (size_t i = 0; i < params.size(); ++i)
			blockParams[i].value = new PawsExprAST(params[i]);
		setBlockExprASTParams(stmt, blockParams);

		defineSymbol(stmt, "parentBlock", PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));

		// Execute statement code block
		codegenExpr((ExprAST*)stmt, parentBlock);

		return getVoid();
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return getVoid().type;
	}
};

struct ExprContext : public CodegenContext
{
private:
	BlockExprAST* const expr;
	BaseType* const type;
	std::vector<Variable> blockParams;
public:
	ExprContext(BlockExprAST* expr, BaseType* type, const std::vector<Variable>& blockParams)
		: expr(expr), type(type), blockParams(blockParams) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		// Set block parameters
		for (size_t i = 0; i < params.size(); ++i)
			blockParams[i].value = new PawsExprAST(params[i]);
		setBlockExprASTParams(expr, blockParams);

		defineSymbol(expr, "parentBlock", PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));

		// Execute expression code block
		try
		{
			codegenExpr((ExprAST*)expr, parentBlock);
		}
		catch (ReturnException err)
		{
			return err.result;
		}
		raiseCompileError("missing return statement in expression block", (ExprAST*)expr);

		return getVoid();
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};

void getBlockParameterTypes(BlockExprAST* scope, const std::vector<ExprAST*> params, std::vector<Variable>& blockParams)
{
	blockParams.reserve(params.size());
	for (ExprAST* param: params)
	{
		BaseType* paramType = PawsExprAST::TYPE;
		if (ExprASTIsPlchld(param))
		{
			PlchldExprAST* plchldParam = (PlchldExprAST*)param;
			switch (getPlchldExprASTLabel(plchldParam))
			{
			default: assert(0); //TODO: Throw exception
			case 'L': paramType = PawsLiteralExprAST::TYPE; break;
			case 'I': paramType = PawsIdExprAST::TYPE; break;
			case 'B': paramType = PawsBlockExprAST::TYPE; break;
			case 'S': break;
			case 'E':
				if (getPlchldExprASTSublabel(plchldParam) == nullptr)
					break;
				if (const Variable* var = importSymbol(scope, getPlchldExprASTSublabel(plchldParam)))
					paramType = PawsTpltType::get(PawsExprAST::TYPE, (BaseType*)var->value->getConstantValue());
			}
		}
		else if (ExprASTIsList(param))
		{
			const std::vector<ExprAST*>& listParamExprs = getExprListASTExpressions((ExprListAST*)param);
			if (listParamExprs.size() != 0)
			{
				PlchldExprAST* plchldParam = (PlchldExprAST*)listParamExprs.front();
				switch (getPlchldExprASTLabel(plchldParam))
				{
				default: assert(0); //TODO: Throw exception
				case 'L': paramType = PawsLiteralExprAST::TYPE; break;
				case 'I': paramType = PawsIdExprAST::TYPE; break;
				case 'B': paramType = PawsBlockExprAST::TYPE; break;
				case 'S': break;
				case 'E':
					if (getPlchldExprASTSublabel(plchldParam) == nullptr)
						break;
					if (const Variable* var = importSymbol(scope, getPlchldExprASTSublabel(plchldParam)))
						paramType = PawsTpltType::get(PawsExprAST::TYPE, (BaseType*)var->value->getConstantValue());
				}
				paramType = PawsTpltType::get(PawsExprListAST::TYPE, paramType);
			}
		}
		blockParams.push_back(Variable(paramType, nullptr));
	}
}

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
			iterateBlockExprASTStmts(stmts->val, [&](const ExprListAST* tplt, const CodegenContext* stmt) {
				key.val = tplt;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsStmtMap>[$E ...] = $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			StmtMap const stmts = ((PawsStmtMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = stmts;
			const std::vector<ExprAST*>& stmtParamsAST = getExprListASTExpressions((ExprListAST*)params[1]);

			// Collect parameters
			std::vector<ExprAST*> stmtParams;
			for (ExprAST* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			definePawsReturnStmt(scope, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new StmtContext((BlockExprAST*)params[2], blockParams));
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
			iterateBlockExprASTExprs(exprs->val, [&](const ExprAST* tplt, const CodegenContext* expr) {
				key.val = tplt;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsExprMap>[$E] = <$I> $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			ExprMap const exprs = ((PawsExprMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = exprs;
			ExprAST* exprParamAST = params[1];
			BaseType* exprType = (BaseType*)codegenExpr(params[2], parentBlock).value->getConstantValue();
			//TODO: Check for errors

			// Collect parameters
			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			definePawsReturnStmt(scope, exprType);

			defineExpr5(scope, exprParamAST, new ExprContext((BlockExprAST*)params[3], exprType, blockParams));
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
			iterateBlockExprASTSymbols(symbols->val, [&](const std::string& name, const Variable& symbol) {
				key.val = name;
				value.val = symbol;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(pkgScope, "$E<PawsSymbolMap>[$I] = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			SymbolMap const symbols = ((PawsSymbolMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = symbols;
			IdExprAST* symbolNameAST = (IdExprAST*)params[1];
			const Variable& symbol = codegenExpr(params[2], parentBlock);

			defineSymbol(scope, getIdExprASTName(symbolNameAST), symbol.type, symbol.value);
		}
	);
});