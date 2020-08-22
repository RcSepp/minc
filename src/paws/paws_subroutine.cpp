#include <cassert>
#include <fstream>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsSubroutineScope = nullptr;

template<> const std::string PawsFunction::toString() const
{
	PawsRegularFunc* regularFunc = dynamic_cast<PawsRegularFunc*>(get());
	if (regularFunc != nullptr)
		return getBlockExprName(regularFunc->body);
	else
		return ""; //TODO
}

MincSymbol PawsRegularFunc::call(MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, const MincSymbol* self) const
{
	MincBlockExpr* instance = cloneBlockExpr(body);

	// Define arguments in function instance
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(instance, argNames[i].c_str(), argTypes[i], ((PawsBase*)codegenExpr(argExprs[i], callerScope).value)->copy());

	// Define 'this' in function instance
	if (self != nullptr)
		defineSymbol(instance, "this", self->type, self->value);

	try
	{
		codegenExpr((MincExpr*)instance, body);
	}
	catch (ReturnException err)
	{
		resetBlockExpr(instance);
		removeBlockExpr(instance);
		return err.result;
	}
	removeBlockExpr(instance);
	return MincSymbol(PawsVoid::TYPE, nullptr);
}

void defineFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
{
	PawsFunc* pawsFunc = new PawsRegularFunc(returnType, argTypes, argNames, body);
	defineSymbol(scope, name, PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, pawsFunc->returnType), new PawsFunction(pawsFunc));
}

void defineConstantFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs)
{
	PawsFunc* pawsFunc = new PawsConstFunc(returnType, argTypes, argNames, body, funcArgs);
	defineSymbol(scope, name, PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, pawsFunc->returnType), new PawsFunction(pawsFunc));
}

MincPackage PAWS_SUBROUTINE("paws.subroutine", [](MincBlockExpr* pkgScope) {
	pawsSubroutineScope = pkgScope;
	registerType<PawsFunction>(pkgScope, "PawsFunction");

	// Define function definition
	defineStmt2(pkgScope, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
			const char* funcName = getIdExprName((MincIdExpr*)params[1]);
			const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
			const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			// Set function parent to function definition scope
			setBlockExprParent(block, parentBlock);

			// Define return statement in function scope
			definePawsReturnStmt(block, returnType);

			PawsRegularFunc* func = new PawsRegularFunc();
			func->returnType = returnType;
			func->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				func->argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
			func->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				func->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
			func->body = block;

			// Name function block
			std::string funcFullName(funcName);
			funcFullName += '(';
			if (func->argTypes.size())
			{
				funcFullName += func->argTypes[0]->name;
				for (size_t i = 1; i != func->argTypes.size(); ++i)
					funcFullName += ", " + func->argTypes[i]->name;
			}
			funcFullName += ')';
			setBlockExprName(block, funcFullName.c_str());

			// Define function symbol in calling scope
			PawsType* funcType = PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, returnType);
			defineSymbol(parentBlock, funcName, funcType, new PawsFunction(func));
		}
	);

	// Define function call
	defineExpr3(pkgScope, "$E<PawsFunction>($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const PawsFunc* func = ((PawsFunction*)codegenExpr(params[0], parentBlock).value)->get();
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (func->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of function arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = func->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(parentBlock, getLocation(argExpr), "invalid function argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = castExpr;
				}
			}

			// Call function
			return func->call(parentBlock, argExprs);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	);
	// Define function call on non-function expression
	defineExpr2(pkgScope, "$E($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			if (getType(params[0], parentBlock) == getErrorType()) // If params[0] has errors
				codegenExpr(params[0], parentBlock); // Raise expression error instead of non-function expression error
			raiseCompileError("expression cannot be used as a function", params[0]);
			return MincSymbol(getErrorType(), nullptr); // LCOV_EXCL_LINE
		},
		getErrorType()
	);
	// Define function call on non-function identifier
	defineExpr2(pkgScope, "$I($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const char* name = getIdExprName((MincIdExpr*)params[0]);
			if (lookupSymbol(parentBlock, name) == nullptr)
				raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
			else
				raiseCompileError(('`' + std::string(name) + "` cannot be used as a function").c_str(), params[0]);
			return MincSymbol(getErrorType(), nullptr); // LCOV_EXCL_LINE
		},
		getErrorType()
	);

	defineExpr2(pkgScope, "PawsFunction<$E<PawsType>>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
			return MincSymbol(PawsType::TYPE, PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, returnType));
		},
		PawsType::TYPE
	);
});