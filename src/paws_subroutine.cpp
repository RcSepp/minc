#include <cassert>
#include <fstream>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

Variable PawsFunc::call(BlockExprAST* parentBlock, const std::vector<ExprAST*>& argExprs) const
{
	// Define arguments in function body
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(body, argNames[i].c_str(), argTypes[i], codegenExpr(argExprs[i], parentBlock).value);

	try
	{
		codegenExpr((ExprAST*)body, parentBlock);
	}
	catch (ReturnException err)
	{
		resetBlockExprAST(body);
		return err.result;
	}
	return Variable(PawsVoid::TYPE, nullptr);
}

MincPackage PAWS_SUBROUTINE("paws.subroutine", [](BlockExprAST* pkgScope) {
	registerType<PawsFunction>(pkgScope, "PawsFunction");

	// Define function definition
	defineStmt2(pkgScope, "$E<PawsMetaType> $I($E<PawsMetaType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			const std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			const std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);
			BlockExprAST* block = (BlockExprAST*)params[4];

			PawsFunc* func = new PawsFunc();
			func->returnType = returnType;
			func->argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				func->argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->get());
			func->argNames.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				func->argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
			func->body = block;

			PawsType* funcType = PawsTpltType::get(PawsFunction::TYPE, returnType);
			defineSymbol(parentBlock, funcName, funcType, new PawsFunction(func));
		}
	);

	// Define function call
	defineExpr3(pkgScope, "$E<PawsFunction>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const PawsFunc* func = ((PawsFunction*)codegenExpr(params[0], parentBlock).value)->get();
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);

			// Check number of arguments
			if (func->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of function arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = func->argTypes[i], *gotType = getType(argExpr, parentBlock);

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
					argExprs[i] = castExpr;
				}
			}

			// Call function
			return func->call(parentBlock, argExprs);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);

	defineExpr(pkgScope, "PawsFunction<$E<PawsMetaType>>",
		+[](PawsType* returnType) -> BaseType* {
			return PawsTpltType::get(PawsFunction::TYPE, returnType);
		}
	);
});