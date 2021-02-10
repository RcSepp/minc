#include <cassert>
#include <fstream>
#include <sstream>
#include "minc_api.hpp"
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
		pawsSubroutineScope->defineSymbol(t->name, PawsType::TYPE, t);
		pawsSubroutineScope->defineCast(new InheritanceCast(t, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE)));
		pawsSubroutineScope->defineCast(new InheritanceCast(t, PawsFunction::TYPE, new MincOpaqueCastKernel(PawsFunction::TYPE)));
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
		if (argExprs[i]->run(runtime))
			return true;
		assert(args[i]->type == runtime.result.type);
		MincObject* var = body->getStackSymbolOfNextStackFrame(runtime, args[i]);
		((PawsType*)runtime.result.type)->copyToNew(runtime.result.value, var);
	}

	runtime.parentBlock = body->parent;
	if (body->run(runtime))
	{
		if (runtime.result.type != &PAWS_RETURN_TYPE)
			return true;
		runtime.result.type = returnType;
		return false;
	}
	if (returnType != PawsVoid::TYPE)
		throw CompileError("non-void function should return a value", body->loc);
	runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
	return false;
}

void defineFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
{
	PawsFunc* pawsFunc = new PawsRegularFunc(name, returnType, argTypes, argNames, body);
	scope->defineSymbol(name, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes), new PawsFunction(pawsFunc));
}

void defineConstantFunction(MincBlockExpr* scope, const char* name, PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, FuncBlock body, void* funcArgs)
{
	PawsFunc* pawsFunc = new PawsConstFunc(name, returnType, argTypes, argNames, body, funcArgs);
	scope->defineSymbol(name, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes), new PawsFunction(pawsFunc));
}

MincPackage PAWS_SUBROUTINE("paws.subroutine", [](MincBlockExpr* pkgScope) {
	pawsSubroutineScope = pkgScope;
	registerType<PawsFunction>(pkgScope, "PawsFunction");

	// Define function definition
	struct FunctionDefinitionKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)params[0]->build(buildtime).value;
			const std::string& name = ((MincIdExpr*)params[1])->name;
			const std::vector<MincExpr*>& argTypeExprs = ((MincListExpr*)params[2])->exprs;
			const std::vector<MincExpr*>& argNameExprs = ((MincListExpr*)params[3])->exprs;
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			// Set function parent to function definition scope
			block->parent = buildtime.parentBlock;

			// Define return statement in function scope
			definePawsReturnStmt(block, returnType);

			PawsRegularFunc* func = new PawsRegularFunc();
			func->name = name;
			func->returnType = returnType;
			func->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
			{
				PawsType* argType = (PawsType*)argTypeExpr->build(buildtime).value;
				if (argType == nullptr)
					throw CompileError(buildtime.parentBlock, argTypeExpr->loc, "%E is not a vailid type", argTypeExpr);
				func->argTypes.push_back(argType);
			}
			func->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				func->argNames.push_back(((MincIdExpr*)argNameExpr)->name);
			func->body = block;

			// Define arguments in function scope
			func->args.reserve(func->argTypes.size());
			for (size_t i = 0; i < func->argTypes.size(); ++i)
				func->args.push_back(block->allocStackSymbol(func->argNames[i], func->argTypes[i], func->argTypes[i]->size));

			// Define function symbol in calling scope
			PawsType* funcType = PawsFunctionType::get(pawsSubroutineScope, returnType, func->argTypes);
			PawsFunction* funcValue = new PawsFunction(func);
			buildtime.parentBlock->defineSymbol(name, funcType, funcValue);

			// Name function block
			block->name = funcType->toString(funcValue);

			// Define function block
			block->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsType> $I($E<PawsType> $I, ...) $B"), new FunctionDefinitionKernel());

	// Define function call
	class FunctionCallKernel : public MincKernel
	{
		const MincStackSymbol* const result;
	public:
		FunctionCallKernel(const MincStackSymbol* result=nullptr) : result(result) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsFunctionType* funcType = (PawsFunctionType*)(params[0] = ((MincCastExpr*)params[0])->getDerivedExpr())->build(buildtime).type;
			std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;

			// Check number of arguments
			if (funcType->argTypes.size() != argExprs.size())
				throw CompileError(buildtime.parentBlock, params[0]->loc, "invalid number of function arguments");

			// Check argument types, perform inherent type casts and build argument expressions
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = funcType->argTypes[i], *gotType = argExpr->getType(buildtime.parentBlock);

				if (expectedType != gotType)
				{
					const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
					if (cast == nullptr)
						throw CompileError(buildtime.parentBlock, argExpr->loc, "invalid function argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					(argExprs[i] = new MincCastExpr(cast, argExpr))->build(buildtime);
				}
				else
					argExpr->build(buildtime);
			}

			// Allocate anonymous symbol for return value in calling scope
			return new FunctionCallKernel(buildtime.parentBlock->allocStackSymbol(funcType->returnType, funcType->returnType->size));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const PawsFunc* func = ((PawsFunction*)runtime.result.value)->get();
			std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;

			// Call function
			if (func->call(runtime, argExprs))
				return true;
			MincObject* resultValue = runtime.parentBlock->getStackSymbol(runtime, result);
			func->returnType->copyTo(runtime.result.value, resultValue);
			runtime.result.value = resultValue;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(params[0]->exprtype == MincExpr::ExprType::CAST); //TODO: This fails when debugging python37.paws
			return ((PawsFunctionType*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock))->returnType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsFunction>($E, ...)")[0], new FunctionCallKernel());

	// Define function call on non-function expression
	struct NonFunctionCallKernel1 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->getType(buildtime.parentBlock); // Raise expression errors if any
			throw CompileError(buildtime.parentBlock, params[0]->loc, "expression cannot be used as a function");
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E($E, ...)")[0], new NonFunctionCallKernel1());

	// Define function call on non-function identifier
	struct NonFunctionCallKernel2 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& name = ((MincIdExpr*)params[0])->name;
			if (buildtime.parentBlock->lookupSymbol(name) == nullptr && buildtime.parentBlock->lookupStackSymbol(name) == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", name);
			else
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` cannot be used as a function", name);
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I($E, ...)")[0], new NonFunctionCallKernel2());

	struct FunctionDelegateKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)params[0]->build(buildtime).value;
			const std::vector<MincExpr*>& argTypeExprs = ((MincListExpr*)params[1])->exprs;
			std::vector<PawsType*> argTypes;
			argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				argTypes.push_back((PawsType*)argTypeExpr->build(buildtime).value);
			buildtime.result = MincSymbol(PawsType::TYPE, PawsFunctionType::get(pawsSubroutineScope, returnType, argTypes));
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("PawsFunction<$E<PawsType>($E<PawsType>, ...)>")[0], new FunctionDelegateKernel());
});