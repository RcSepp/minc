#include <cassert>
#include <fstream>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsSubroutineScope = nullptr;

template<> std::string PawsFunction::Type::toString(MincObject* value) const
{
	PawsRegularFunc* regularFunc = dynamic_cast<PawsRegularFunc*>(((PawsFunction*)value)->get());
	if (regularFunc != nullptr)
		return getBlockExprName(regularFunc->body);
	else
		return ""; //TODO
}

bool PawsRegularFunc::call(MincRuntime& runtime, const std::vector<MincExpr*>& argExprs, const MincSymbol* self) const
{
	MincBlockExpr* instance = cloneBlockExpr(body);

	// Define arguments in function instance
	for (size_t i = 0; i < argExprs.size(); ++i)
	{
		if (runExpr(argExprs[i], runtime))
			return true;
		MincSymbol argSym = runtime.result;
		MincSymbol* var = getSymbol(instance, args[i]);
		assert(var->type == argSym.type);
		var->value = ((PawsType*)argSym.type)->copy((PawsBase*)argSym.value);
	}

	try
	{
		runtime.parentBlock = getBlockExprParent(body);
		if (runExpr((MincExpr*)instance, runtime))
		{
			if (runtime.result.type != &PAWS_RETURN_TYPE)
				return true;
			removeBlockExpr(instance);
			runtime.result = MincSymbol(returnType, runtime.result.value);
			return false;
		}
	}
	catch (ReturnException err)
	{
		removeBlockExpr(instance);
		runtime.result = err.result;
		return false;
	}
	removeBlockExpr(instance);
	if (returnType != PawsVoid::TYPE)
		throw CompileError("non-void function should return a value", getLocation((MincExpr*)body));
	runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
	return false;
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
	class FunctionDefinitionKernel : public MincKernel
	{
		const MincSymbolId varId;
		PawsFunction* const func;
		PawsType* const funcType;
	public:
		FunctionDefinitionKernel() : varId(MincSymbolId::NONE), func(nullptr), funcType(nullptr) {}
		FunctionDefinitionKernel(MincSymbolId varId, PawsType* funcType, PawsFunction* func) : varId(varId), func(func), funcType(funcType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)buildExpr(params[0], buildtime).value;
			const char* name = getIdExprName((MincIdExpr*)params[1]);
			const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
			const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			// Set function parent to function definition scope
			setBlockExprParent(block, buildtime.parentBlock);

			// Define return statement in function scope
			definePawsReturnStmt(block, returnType);

			PawsRegularFunc* func = new PawsRegularFunc();
			func->returnType = returnType;
			func->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				func->argTypes.push_back((PawsType*)buildExpr(argTypeExpr, buildtime).value);
			func->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				func->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
			func->body = block;

			// Define arguments in function scope
			func->args.reserve(func->argTypes.size());
			for (size_t i = 0; i < func->argTypes.size(); ++i)
			{
				defineSymbol(block, func->argNames[i].c_str(), func->argTypes[i], nullptr);
				func->args.push_back(lookupSymbolId(block, func->argNames[i].c_str()));
			}

			// Name function block
			std::string signature(name);
			signature += '(';
			if (func->argTypes.size())
			{
				signature += func->argTypes[0]->name;
				for (size_t i = 1; i != func->argTypes.size(); ++i)
					signature += ", " + func->argTypes[i]->name;
			}
			signature += ')';
			setBlockExprName(block, signature.c_str());

			// Define function symbol in calling scope
			PawsType* funcType = PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, returnType);
			defineSymbol(buildtime.parentBlock, name, funcType, nullptr);

			buildExpr((MincExpr*)block, buildtime);

			return new FunctionDefinitionKernel(lookupSymbolId(buildtime.parentBlock, name), funcType, new PawsFunction(func));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			// Set function parent to function definition scope (the parent may have changed during function cloning)
			MincBlockExpr* block = (MincBlockExpr*)params[4];
			setBlockExprParent(block, runtime.parentBlock);

			MincSymbol* varFromId = getSymbol(runtime.parentBlock, varId);
			varFromId->value = func;
			varFromId->type = funcType;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "$E<PawsType> $I($E<PawsType> $I, ...) $B", new FunctionDefinitionKernel());

	// Define function call
	defineExpr10(pkgScope, "$E<PawsFunction>($E, ...)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);
			for (MincExpr* argExpr: argExprs)
				buildExpr(argExpr, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const PawsFunc* func = ((PawsFunction*)runtime.result.value)->get();
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (func->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of function arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = func->argTypes[i], *gotType = getType(argExpr, runtime.parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(runtime.parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(runtime.parentBlock, getLocation(argExpr), "invalid function argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					MincBuildtime buildtime = { runtime.parentBlock }; //TODO: Do parameter matching and casting at runtime
					buildExpr(castExpr, buildtime);
					argExprs[i] = castExpr;
				}
			}

			// Call function
			return func->call(runtime, argExprs);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	);
	// Define function call on non-function expression
	defineExpr7(pkgScope, "$E($E, ...)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			getType(params[0], buildtime.parentBlock); // Raise expression errors if any
			raiseCompileError("expression cannot be used as a function", params[0]);
		},
		getErrorType()
	);
	// Define function call on non-function identifier
	defineExpr7(pkgScope, "$I($E, ...)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			const char* name = getIdExprName((MincIdExpr*)params[0]);
			if (lookupSymbol(buildtime.parentBlock, name) == nullptr)
				raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
			else
				raiseCompileError(('`' + std::string(name) + "` cannot be used as a function").c_str(), params[0]);
		},
		getErrorType()
	);

	defineExpr7(pkgScope, "PawsFunction<$E<PawsType>>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			PawsType* returnType = (PawsType*)buildExpr(params[0], buildtime).value;
			buildtime.result = MincSymbol(PawsType::TYPE, PawsTpltType::get(pawsSubroutineScope, PawsFunction::TYPE, returnType));
		},
		PawsType::TYPE
	);
});