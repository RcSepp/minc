#include <cassert>
#include <fstream>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

MincBlockExpr* pawsSubroutineScope = nullptr;

PawsFunctionType::PawsFunctionType(PawsType* returnType, const std::vector<PawsType*>& argTypes)
	: PawsType(PawsFunction::TYPE->size), returnType(returnType), argTypes(argTypes)
{
}
PawsFunctionType* PawsFunctionType::get(const MincBlockExpr* scope, PawsType* returnType, const std::vector<PawsType*>& argTypes)
{
	std::unique_lock<std::recursive_mutex> lock(mutex);
	std::set<PawsFunctionType>::iterator iter = functionTypes.find(PawsFunctionType(returnType, argTypes));
	if (iter == functionTypes.end())
	{
		iter = functionTypes.insert(PawsFunctionType(returnType, argTypes)).first; //TODO: Avoid reconstruction of PawsFunctionType(returnType, argTypes)
		PawsFunctionType* t = const_cast<PawsFunctionType*>(&*iter); //TODO: Find a way to avoid const_cast
		t->name = "PawsFunction<" + returnType->name + '(';
		if (argTypes.size())
		{
			t->name += argTypes[0]->name;
			for (size_t i = 1; i != argTypes.size(); ++i)
				t->name += ", " + argTypes[i]->name;
		}
		t->name += ")>";
		defineSymbol(pawsSubroutineScope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(pawsSubroutineScope, t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(pawsSubroutineScope, t, PawsFunction::TYPE);
	}
	return const_cast<PawsFunctionType*>(&*iter); //TODO: Find a way to avoid const_cast
}
MincObject* PawsFunctionType::copy(MincObject* value)
{
	return value;
}
void PawsFunctionType::copyTo(MincObject* src, MincObject* dest)
{
	((PawsFunction*)src)->copyTo(dest);
}
void PawsFunctionType::copyToNew(MincObject* src, MincObject* dest)
{
	((PawsFunction*)src)->copyToNew(src, dest);
}
MincObject* PawsFunctionType::alloc()
{
	return new PawsFunction(new PawsRegularFunc()); //TODO: Support other types of PawsFunc
}
MincObject* PawsFunctionType::allocTo(MincObject* memory)
{
	return new(memory) PawsFunction(new PawsRegularFunc()); //TODO: Support other types of PawsFunc
}
std::string PawsFunctionType::toString(MincObject* value) const
{
	PawsFunc* func = ((PawsFunction*)value)->get();
	std::string str = func->returnType->name + ' ' + func->name + '(';
	if (argTypes.size())
	{
		str += argTypes[0]->name + ' ' + func->argNames[0];
		for (size_t i = 1; i != argTypes.size(); ++i)
			str += ", " + argTypes[i]->name + ' ' + func->argNames[i];
	}
	str += ')';
	return str;
}
std::recursive_mutex PawsFunctionType::mutex;
std::set<PawsFunctionType> PawsFunctionType::functionTypes;
bool operator<(const PawsFunctionType& lhs, const PawsFunctionType& rhs)
{
	return lhs.returnType < rhs.returnType || (lhs.returnType == rhs.returnType && lhs.argTypes < rhs.argTypes);
}

bool PawsRegularFunc::call(MincRuntime& runtime, const std::vector<MincExpr*>& argExprs, const MincSymbol* self) const
{
	// Assign arguments in function body
	for (size_t i = 0; i < argExprs.size(); ++i)
	{
		if (runExpr(argExprs[i], runtime))
			return true;
		assert(args[i]->type == runtime.result.type);
		MincObject* var = getStackSymbolOfNextStackFrame(body, runtime, args[i]);
		((PawsType*)runtime.result.type)->allocTo(var);
		((PawsType*)runtime.result.type)->copyTo(runtime.result.value, var);
	}

	runtime.parentBlock = getBlockExprParent(body);
	if (runExpr((MincExpr*)body, runtime))
	{
		if (runtime.result.type != &PAWS_RETURN_TYPE)
			return true;
		runtime.result.type = returnType;
		return false;
	}
	if (returnType != PawsVoid::TYPE)
		throw CompileError("non-void function should return a value", getLocation((MincExpr*)body));
	runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
	return false;
}

void defineFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
{
	PawsFunc* pawsFunc = new PawsRegularFunc(name, returnType, argTypes, argNames, body);
	defineSymbol(scope, name, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes), new PawsFunction(pawsFunc));
}

void defineConstantFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs)
{
	PawsFunc* pawsFunc = new PawsConstFunc(name, returnType, argTypes, argNames, body, funcArgs);
	defineSymbol(scope, name, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes), new PawsFunction(pawsFunc));
}

MincPackage PAWS_SUBROUTINE("paws.subroutine", [](MincBlockExpr* pkgScope) {
	pawsSubroutineScope = pkgScope;
	registerType<PawsFunction>(pkgScope, "PawsFunction");

	// Define function definition
	defineStmt5(pkgScope, "$E<PawsType> $I($E<PawsType> $I, ...) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
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
			func->name = name;
			func->returnType = returnType;
			func->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
			{
				PawsType* argType = (PawsType*)buildExpr(argTypeExpr, buildtime).value;
				if (argType == nullptr)
					throw CompileError(buildtime.parentBlock, getLocation(argTypeExpr), "%E is not a vailid type", argTypeExpr);
				func->argTypes.push_back(argType);
			}
			func->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				func->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
			func->body = block;

			// Define arguments in function scope
			func->args.reserve(func->argTypes.size());
			for (size_t i = 0; i < func->argTypes.size(); ++i)
				func->args.push_back(allocStackSymbol(block, func->argNames[i].c_str(), func->argTypes[i], func->argTypes[i]->size));

			// Define function symbol in calling scope
			PawsType* funcType = PawsFunctionType::get(pawsSubroutineScope, returnType, func->argTypes);
			PawsFunction* funcValue = new PawsFunction(func);
			defineSymbol(buildtime.parentBlock, name, funcType, funcValue);

			// Name function block
			setBlockExprName(block, funcType->toString(funcValue).c_str());

			// Define function block
			buildExpr((MincExpr*)block, buildtime);
		}
	);

	// Define function call
	class FunctionCallKernel : public MincKernel
	{
		const MincStackSymbol* const result;
	public:
		FunctionCallKernel(const MincStackSymbol* result=nullptr) : result(result) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsFunctionType* funcType = (PawsFunctionType*)buildExpr(params[0] = getDerivedExpr(params[0]), buildtime).type;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (funcType->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of function arguments", params[0]);

			// Check argument types, perform inherent type casts and build argument expressions
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = funcType->argTypes[i], *gotType = ::getType(argExpr, buildtime.parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(buildtime.parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(buildtime.parentBlock, getLocation(argExpr), "invalid function argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					buildExpr(argExprs[i] = castExpr, buildtime);
				}
				else
					buildExpr(argExpr, buildtime);
			}

			// Allocate anonymous symbol for return value in calling scope
			return new FunctionCallKernel(allocAnonymousStackSymbol(buildtime.parentBlock, funcType->returnType, funcType->returnType->size));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime))
				return true;
			const PawsFunc* func = ((PawsFunction*)runtime.result.value)->get();
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Call function
			if (func->call(runtime, argExprs))
				return true;
			MincObject* resultValue = getStackSymbol(runtime.parentBlock, runtime, result);
			func->returnType->copyTo(runtime.result.value, resultValue);
			runtime.result.value = resultValue;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(ExprIsCast(params[0])); //TODO: This fails when debugging python37.paws
			return ((PawsFunctionType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->returnType;
		}
	};
	defineExpr6(pkgScope, "$E<PawsFunction>($E, ...)", new FunctionCallKernel());

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
			if (lookupSymbol(buildtime.parentBlock, name) == nullptr && lookupStackSymbol(buildtime.parentBlock, name) == nullptr)
				raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
			else
				raiseCompileError(('`' + std::string(name) + "` cannot be used as a function").c_str(), params[0]);
		},
		getErrorType()
	);

	defineExpr7(pkgScope, "PawsFunction<$E<PawsType>($E<PawsType>, ...)>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			PawsType* returnType = (PawsType*)buildExpr(params[0], buildtime).value;
			const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[1]);
			std::vector<PawsType*> argTypes;
			argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				argTypes.push_back((PawsType*)buildExpr(argTypeExpr, buildtime).value);
			buildtime.result = MincSymbol(PawsType::TYPE, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes));
		},
		PawsType::TYPE
	);
});