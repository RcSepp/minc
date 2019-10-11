#include <cassert>
#include <fstream>
#include <sstream>
#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

struct PawsFunc
{
	BaseType* returnType;
	std::vector<BaseType*> argTypes;
	std::vector<std::string> argNames;
	BlockExprAST* body;
};
typedef PawsType<PawsFunc> PawsFunction;

PawsPackage PAWS_SUBROUTINE("subroutine", [](BlockExprAST* pkgScope) {
	registerType<PawsFunction>(pkgScope, "PawsFunction");

	// Define function definition
	defineStmt2(pkgScope, "$E<PawsMetaType> $I($E<PawsMetaType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BaseType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->val;
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			const std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			const std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);
			BlockExprAST* block = (BlockExprAST*)params[4];

			PawsFunction* function = new PawsFunction();
			function->val.returnType = returnType;
			function->val.argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				function->val.argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->val);
			function->val.argNames.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				function->val.argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
			function->val.body = block;

			BaseType* funcType = PawsTpltType::get(PawsFunction::TYPE, returnType);
			defineSymbol(parentBlock, funcName, funcType, function);
		}
	);

	// Define function call
	defineExpr3(pkgScope, "$E<PawsFunction>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const PawsFunc& func = ((PawsFunction*)codegenExpr(params[0], parentBlock).value)->val;
			const std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);

			if (func.argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of function arguments", params[0]);

			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = func.argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						raiseCompileError(
							("invalid function argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExpr
						);
					}
					argExpr = castExpr;
				}

				defineSymbol(func.body, func.argNames[i].c_str(), func.argTypes[i], codegenExpr(argExpr, parentBlock).value);
			}

			try
			{
				codegenExpr((ExprAST*)func.body, parentBlock);
			}
			catch (ReturnException err)
			{
				return err.result;
			}
			return Variable(PawsVoid::TYPE, nullptr);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);

	defineExpr(pkgScope, "PawsFunction<$E<PawsMetaType>>",
		+[](BaseType* returnType) -> BaseType* {
			return PawsTpltType::get(PawsFunction::TYPE, returnType);
		}
	);
});