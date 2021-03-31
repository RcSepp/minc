#include <string>
#include <map>
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>
#include "minc_api.hpp"
#include "minc_cli.h"
#include "minc_dbg.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

static struct {} PAWS_KERNEL_ID;

MincScopeType* FILE_SCOPE_TYPE = new MincScopeType();
MincObject PAWS_RETURN_TYPE, PAWS_AWAIT_TYPE;
MincBlockExpr* pawsScope = nullptr;

PawsKernel* getKernelFromUserData(const MincBlockExpr* scope)
{
	while (scope->userType != &PAWS_KERNEL_ID)
		scope = scope->parent;
	assert(scope != nullptr);
	return (PawsKernel*)scope->user;
}

std::string PawsType::toString(MincObject* value) const
{
	static const char* HEX_DIGITS = "0123456789abcdef";
	static const size_t POINTER_SIZE = 2 * sizeof(void*);
	uint64_t ptr = (uint64_t)value;
	std::string str = "0x";

	uint8_t digit = 0, i = POINTER_SIZE;

	// Find most significant digit
	while (i-- && digit == 0)
		digit = (ptr >> (4 * i)) & 0x0F;
	
	if (digit) // If ptr != 0x0
	{
		// Append most significant digit
		str.push_back(HEX_DIGITS[digit]);

		// Append remaining digits
		while (i--)
		{
			digit = (ptr >> (4 * i)) & 0x0F;
			str.push_back(HEX_DIGITS[digit]);
		}
	}
	else // If ptr == 0x0
		str.push_back('0');

	return str;
}

std::mutex PawsTpltType::mutex;
std::set<PawsTpltType*> PawsTpltType::tpltTypes;
PawsTpltType* PawsTpltType::get(MincBlockExpr* scope, PawsType* baseType, PawsType* tpltType)
{
	std::unique_lock<std::mutex> lock(mutex);
	PawsTpltType f(baseType, tpltType);
	std::set<PawsTpltType*>::iterator iter = tpltTypes.find(&f);
	if (iter == tpltTypes.end())
	{
		iter = tpltTypes.insert(new PawsTpltType(baseType, tpltType)).first;
		PawsTpltType* t = *iter;
		t->name = baseType->name + '<' + tpltType->name + '>';
		scope->defineSymbol(t->name, PawsType::TYPE, t);
		scope->defineCast(new InheritanceCast(t, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE))); // Let baseType<tpltType> derive from PawsBase
		scope->defineCast(new InheritanceCast(t, baseType, new MincOpaqueCastKernel(baseType))); // Let baseType<tpltType> derive from baseType
	}
	return *iter;
}

void definePawsReturnStmt(MincBlockExpr* scope, const MincObject* returnType, const char* funcName)
{
	if (returnType == PawsVoid::TYPE)
	{
		// Define return statement with incorrect type in function scope
		class ReturnKernel1 : public MincKernel
		{
			const char* const funcName;
		public:
			ReturnKernel1(const char* funcName) : funcName(funcName) {}

			MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
			{
				throw CompileError(buildtime.parentBlock, params[0]->loc, "void %s should not return a value", funcName);
			} // LCOV_EXCL_LINE

			bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
			{
				return false;
			}

			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
			{
				return getVoid().type;
			}
		};
		pawsScope->defineStmt(MincBlockExpr::parseCTplt("return $E"), new ReturnKernel1(funcName), scope);

		// Define return statement without type in function scope
		struct ReturnKernel2 : public MincKernel
		{
			bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
			{
				runtime.exceptionType = &PAWS_RETURN_TYPE;
				runtime.result = nullptr;
				return true;
			}

			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
			{
				return getVoid().type;
			}
		};
		pawsScope->defineStmt(MincBlockExpr::parseCTplt("return"), new ReturnKernel2(), scope);
	}
	else
	{
		// Define return statement with incorrect type in function scope
		struct ReturnKernel3 : public MincKernel
		{
			MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
			{
				MincObject* returnType = params[0]->getType(buildtime.parentBlock);
				throw CompileError(buildtime.parentBlock, params[0]->loc, "invalid return type `%t`", returnType);
			} // LCOV_EXCL_LINE

			bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
			{
				return false;
			}

			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
			{
				return getVoid().type;
			}
		};
		pawsScope->defineStmt(MincBlockExpr::parseCTplt("return $E"), new ReturnKernel3(), scope);

		// Define return statement with correct type in function scope
		struct ReturnKernel4 : public MincKernel
		{
			MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
			{
				params[0]->build(buildtime);
				return this;
			}

			bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
			{
				if (params[0]->run(runtime))
					return true;
				runtime.exceptionType = &PAWS_RETURN_TYPE;
				return true;
			}

			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
			{
				return getVoid().type;
			}
		};
		pawsScope->defineStmt(MincBlockExpr::parseCTplt(("return $E<" + scope->lookupSymbolName(returnType, "UNKNOWN_TYPE") + ">").c_str()), new ReturnKernel4(), scope);

		// Define return statement without type in function scope
		class ReturnKernel5 : public MincKernel
		{
			const char* const funcName;
		public:
			ReturnKernel5(const char* funcName) : funcName(funcName) {}

			MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
			{
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "non-void %s should return a value", funcName);
			} // LCOV_EXCL_LINE

			bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
			{
				return false;
			}

			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
			{
				return getVoid().type;
			}
		};
		pawsScope->defineStmt(MincBlockExpr::parseCTplt("return"), new ReturnKernel5(funcName), scope);
	}
}

void getBlockParameterTypes(MincBlockExpr* scope, const std::vector<MincExpr*> params, std::vector<MincSymbol>& blockParams)
{
	blockParams.reserve(params.size());
	for (MincExpr* param: params)
	{
		PawsType* paramType = PawsExpr::TYPE;
		if (param->exprtype == MincExpr::ExprType::PLCHLD)
		{
			MincPlchldExpr* plchldParam = (MincPlchldExpr*)param;
			switch (plchldParam->p1)
			{
			default: assert(0); //TODO: Throw exception
			case 'L': paramType = PawsLiteralExpr::TYPE; break;
			case 'I': paramType = PawsIdExpr::TYPE; break;
			case 'B': paramType = PawsBlockExpr::TYPE; break;
			case 'S': break;
			case 'E':
			case 'D':
				if (plchldParam->p2 == nullptr)
					break;
				if (const MincSymbol* var = scope->importSymbol(plchldParam->p2))
					paramType = PawsTpltType::get(pawsScope, PawsExpr::TYPE, (PawsType*)var->value);
			}
		}
		else if (param->exprtype == MincExpr::ExprType::LIST)
		{
			const std::vector<MincExpr*>& listParamExprs = ((MincListExpr*)param)->exprs;
			if (listParamExprs.size() != 0)
			{
				MincPlchldExpr* plchldParam = (MincPlchldExpr*)listParamExprs.front();
				switch (plchldParam->p1)
				{
				default: assert(0); //TODO: Throw exception
				case 'L': paramType = PawsLiteralExpr::TYPE; break;
				case 'I': paramType = PawsIdExpr::TYPE; break;
				case 'B': paramType = PawsBlockExpr::TYPE; break;
				case 'S': break;
				case 'E':
				case 'D':
					if (plchldParam->p2 == nullptr)
						break;
					if (const MincSymbol* var = scope->importSymbol(plchldParam->p2))
						paramType = PawsTpltType::get(pawsScope, PawsExpr::TYPE, (PawsType*)var->value);
				}
				paramType = PawsTpltType::get(pawsScope, PawsListExpr::TYPE, paramType);
			}
		}
		blockParams.push_back(MincSymbol(paramType, nullptr));
	}
}

PawsKernel::PawsKernel(MincBlockExpr* body, MincObject* type)
	: body(body), type(type), phase(Phase::INIT), activePhase(Phase::INIT)
{
}

PawsKernel::PawsKernel(MincBlockExpr* body, MincObject* type, MincBuildtime& buildtime, const std::vector<MincSymbol>& blockParams)
	: body((MincBlockExpr*)body->clone()), type(type), blockParams(blockParams), phase(Phase::INIT), activePhase(Phase::INIT), callerScope(nullptr)
{
	// Create kernel definition scope
	// All statements within the kernel body are conditionally executed in run or build phase
	MincBlockExpr* kernelDefScope = this->body;
	kernelDefScope->user = this; // Store kernel in kernel definition block user data
	kernelDefScope->userType = &PAWS_KERNEL_ID;

	// Define build phase selector statement
	struct BuildPhaseSelectorKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase != PawsKernel::Phase::INIT)
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "build phase must start at beginning of Paws kernel");
			kernel->phase = PawsKernel::Phase::BUILD;
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			kernel->phase = PawsKernel::Phase::BUILD;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	kernelDefScope->defineStmt(MincBlockExpr::parseCTplt("build:"), new BuildPhaseSelectorKernel());

	// Define run phase selector statement
	struct RunPhaseSelectorKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::RUN)
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "redefinition of Paws kernel run phase");
			kernel->phase = PawsKernel::Phase::RUN;
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			kernel->phase = PawsKernel::Phase::RUN;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	kernelDefScope->defineStmt(MincBlockExpr::parseCTplt("run:"), new RunPhaseSelectorKernel());

	// Conditionally execute other statements
	struct DefaultKernelKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT) // If no phase was defined at beginning of Paws kernel, ...
				kernel->phase = PawsKernel::Phase::RUN; // Default to run phase

			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT)
				kernel->phase = PawsKernel::Phase::RUN;

			if (kernel->phase == kernel->activePhase)
				return params[0]->run(runtime);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	kernelDefScope->defineDefaultStmt(new DefaultKernelKernel());

	MincBlockExpr* oldParentBlock = buildtime.parentBlock;
	buildtime.parentBlock = body;
	kernelDefScope->build(buildtime);
	buildtime.parentBlock = oldParentBlock;
}

MincKernel* PawsKernel::build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
{
	PawsKernel* instanceKernel = new PawsKernel(body, type);
	instanceKernel->activePhase = Phase::BUILD; // Execute build phase statements when running kernel block
	instanceKernel->callerScope = buildtime.parentBlock;

	body->user = instanceKernel; // Store kernel instance in kernel block user data
	body->userType = &PAWS_KERNEL_ID;

	// Set block parameters
	for (size_t i = 0; i < params.size(); ++i)
		blockParams[i].value = new PawsExpr(params[i]);
	body->blockParams = blockParams;

	body->defineSymbol("parentBlock", PawsBlockExpr::TYPE, new PawsBlockExpr(buildtime.parentBlock));

	// Execute expression code block
	MincRuntime runtime(body->parent, false);
	if (body->run(runtime))
	{
		if (runtime.exceptionType != &PAWS_RETURN_TYPE)
			throw runtime.result;
		instanceKernel->buildResult = runtime.result;
		instanceKernel->hasBuildResult = true;
		return instanceKernel;
	}
	instanceKernel->hasBuildResult = false;

	return instanceKernel;
}

void PawsKernel::dispose(MincKernel* kernel)
{
	delete kernel;
}

bool PawsKernel::run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
{
	if (hasBuildResult)
	{
		runtime.result = buildResult;
		return false;
	}

	activePhase = Phase::RUN; // Execute run phase statements when running kernel block
	callerScope = runtime.parentBlock;

	// Execute expression code block
	runtime.parentBlock = body->parent;
	if (body->run(runtime))
		return runtime.exceptionType != &PAWS_RETURN_TYPE;

	if (type != getVoid().type && type != PawsVoid::TYPE)
		throw CompileError(runtime.parentBlock, body->loc, "missing return statement in expression block");
	runtime.result = getVoid().value;
	return false;
}

MincObject* PawsKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return type;
}

template<> std::string PawsValue<const MincExpr*>::Type::toString(MincObject* value) const
{
	return ((PawsValue<const MincExpr*>*)value)->get()->str();
}

bool serializePawsValue(const MincBlockExpr* scope, const MincSymbol& value, std::string* valueStr)
{
	if (scope->isInstance(value.type, PawsBase::TYPE))
	{
		*valueStr = value.value == nullptr ? "NULL" : ((PawsType*)value.type)->toString((PawsBase*)value.value);
		return true;
	}
	else
		return false;
}

MincPackage PAWS("paws", [](MincBlockExpr* pkgScope) {
	pawsScope = pkgScope;
	registerValueSerializer(serializePawsValue);
	registerType<PawsBase>(pkgScope, "PawsBase");
	registerType<PawsStatic>(pkgScope, "PawsStatic");
	registerType<PawsDynamic>(pkgScope, "PawsDynamic");
	registerType<PawsVoid>(pkgScope, "PawsVoid");
	registerType<PawsType>(pkgScope, "PawsType", true);
	registerType<PawsInt>(pkgScope, "PawsInt");
	registerType<PawsDouble>(pkgScope, "PawsDouble");
	registerType<PawsString>(pkgScope, "PawsString");
	registerType<PawsExpr>(pkgScope, "PawsExpr");
	registerType<PawsBlockExpr>(pkgScope, "PawsBlockExpr");
	registerType<PawsConstBlockExprList>(pkgScope, "PawsConstBlockExprList");
	registerType<PawsStaticBlockExpr>(pkgScope, "PawsStaticBlockExpr", true);
	pkgScope->defineCast(new TypeCast(PawsStaticBlockExpr::TYPE, PawsBlockExpr::TYPE, new MincOpaqueCastKernel(PawsBlockExpr::TYPE)));
	registerType<PawsListExpr>(pkgScope, "PawsListExpr");
	registerType<PawsLiteralExpr>(pkgScope, "PawsLiteralExpr");
	registerType<PawsIdExpr>(pkgScope, "PawsIdExpr");
	registerType<PawsSym>(pkgScope, "PawsSym");
	registerType<PawsScopeType>(pkgScope, "PawsScopeType");
	registerType<PawsStringMap>(pkgScope, "PawsStringMap");
	registerType<PawsNull>(pkgScope, "PawsNull", true);

	// Create null pointer variable
	pkgScope->defineSymbol("NULL", PawsNull::TYPE, nullptr);

	// Create data type for matching against undefined symbols
	pkgScope->defineSymbol("PawsErrorType", PawsType::TYPE, getErrorType());

	// Import builtin paws packages
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.int");
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.double");
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.string");

	// Define inherent type casts
	defineTypeCast(pkgScope, +[](int i) -> double { return (double)i; } );
	defineTypeCast(pkgScope, +[](double d) -> int { return (int)d; } );

	int argc;
	char** argv;
	getCommandLineArgs(&argc, &argv);
	pkgScope->blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		pkgScope->blockParams.push_back(MincSymbol(PawsString::TYPE, new PawsString(std::string(argv[i]))));

	struct GetFileScopeKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildtime.result = MincSymbol(PawsStaticBlockExpr::TYPE, new PawsStaticBlockExpr(getFileScope()));
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsStaticBlockExpr::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("getFileScope()")[0], new GetFileScopeKernel());

	pkgScope->defineSymbol("FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	struct SingleExprKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return params[0]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E"), new SingleExprKernel());

	// Define context-free block statement
	struct ContextFreeBlockKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return params[0]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$B"), new ContextFreeBlockKernel());

	// Define general bracketed expression
	struct BracketedExprKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return params[0]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return params[0]->getType(parentBlock);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("($E)")[0], new BracketedExprKernel());

	// Define return statement
	definePawsReturnStmt(pkgScope, PawsInt::TYPE);

	// Overwrite return statement with correct type in function scope to call quit() instead of raising PAWS_RETURN_TYPE
	defineStmt(pkgScope, "return $E<PawsInt>",
		+[](int returnCode) {
			quit(returnCode);
		} // LCOV_EXCL_LINE
	);

	// Define build-time variable lookup
	class BuildtimeVariableLookupKernel : public MincKernel
	{
		const MincSymbol symbol;
	public:
		BuildtimeVariableLookupKernel() : symbol() {}
		BuildtimeVariableLookupKernel(const MincSymbol& symbol) : symbol(symbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const MincSymbol* var = buildtime.parentBlock->importSymbol(((MincIdExpr*)params[0])->name);
			if (var == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new BuildtimeVariableLookupKernel(buildtime.result = *var);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = symbol.value;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const MincSymbol* var = parentBlock->lookupSymbol(((MincIdExpr*)params[0])->name);
			return var != nullptr ? var->type : getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsStatic>")[0], new BuildtimeVariableLookupKernel());

	// Define stack variable lookup
	class StackVariableLookupKernel : public MincKernel
	{
		const MincStackSymbol* const stackSymbol;
	public:
		StackVariableLookupKernel(const MincStackSymbol* stackSymbol=nullptr) : stackSymbol(stackSymbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& name = ((MincIdExpr*)params[0])->name;
			const MincStackSymbol* stackSymbol = buildtime.parentBlock->lookupStackSymbol(name);
			if (stackSymbol != nullptr)
			{
				buildtime.result = MincSymbol(stackSymbol->type, nullptr);
				return new StackVariableLookupKernel(stackSymbol);
			}
			const MincSymbol* var = buildtime.parentBlock->importSymbol(name);
			if (var != nullptr)
				return new BuildtimeVariableLookupKernel(buildtime.result = *var);
			throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", name);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = runtime.parentBlock->getStackSymbol(runtime, stackSymbol);
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const std::string& name = ((MincIdExpr*)params[0])->name;
			const MincSymbol* var = parentBlock->lookupSymbol(name);
			if (var != nullptr)
				return var->type;
			const MincStackSymbol* stackSymbol = parentBlock->lookupStackSymbol(name);
			if (stackSymbol != nullptr)
				return stackSymbol->type;
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDynamic>")[0], new StackVariableLookupKernel());

	// Define literal definition
	class LiteralDefinitionKernel : public MincKernel
	{
		MincObject* const val;
	public:
		LiteralDefinitionKernel() : val(nullptr) {}
		LiteralDefinitionKernel(MincObject* val) : val(val) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& value = ((MincLiteralExpr*)params[0])->value;

			if (value.back() == '"' || value.back() == '\'')
			{
				auto valueStart = value.find(value.back()) + 1;
				buildtime.result.type = PawsString::TYPE;
				return new LiteralDefinitionKernel(buildtime.result.value = new PawsString(value.substr(valueStart, value.size() - valueStart - 1)));
			}

			if (value.find('.') != std::string::npos)
			{
				double doubleValue = std::stod(value);
				buildtime.result.type = PawsDouble::TYPE;
				return new LiteralDefinitionKernel(buildtime.result.value = new PawsDouble(doubleValue));
			}
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			buildtime.result.type = PawsInt::TYPE;
			return new LiteralDefinitionKernel(buildtime.result.value = new PawsInt(intValue));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = val;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const std::string& value = ((MincLiteralExpr*)params[0])->value;
			if (value.back() == '"' || value.back() == '\'')
				return PawsString::TYPE;
			if (value.find('.') != std::string::npos)
				return PawsDouble::TYPE;
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$L")[0], new LiteralDefinitionKernel());

	// Define variable (re)assignment
	class StackVariableAssignmentKernel : public MincKernel
	{
		const MincStackSymbol* const stackSymbol;
	public:
		StackVariableAssignmentKernel(const MincStackSymbol* stackSymbol=nullptr) : stackSymbol(stackSymbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1] = params[1]->getDerivedExpr();
			params[1]->build(buildtime);
			PawsType* valType = (PawsType*)params[1]->getType(buildtime.parentBlock);

			const std::string& name = ((MincIdExpr*)params[0]->getSourceExpr())->name;

			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(name);
			if (varId == nullptr) // If `name` is undefined, ...
				// Define `name`
				varId = buildtime.parentBlock->allocStackSymbol(name, valType, ((PawsType*)valType)->size);
			else if (varId->type != valType) // If `name` is defined with a different type, ...
				// Redefine `name` with new type
				varId = varId->scope->allocStackSymbol(name, valType, ((PawsType*)valType)->size);
			return new StackVariableAssignmentKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[1]->run(runtime))
				return true;

			MincObject* value = runtime.parentBlock->getStackSymbol(runtime, stackSymbol);
			((PawsType*)stackSymbol->type)->copyToNew(runtime.result, value);
			runtime.result = value;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return params[1]->getDerivedExpr()->getType(parentBlock);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDynamic> = $E<PawsDynamic>")[0], new StackVariableAssignmentKernel());
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsErrorType> = $E<PawsDynamic>")[0], new StackVariableAssignmentKernel());

	// Define build-time variable reassignment
	struct BuildtimeVariableReassignmentKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildtime.result = params[1]->getDerivedExpr()->build(buildtime);
			buildtime.result.value = ((PawsType*)buildtime.result.type)->copy((PawsBase*)buildtime.result.value);

			MincExpr* varExpr = params[0]->getSourceExpr();
			MincSymbol* var = buildtime.parentBlock->importSymbol(((MincIdExpr*)varExpr)->name);
			if (var == nullptr)
				buildtime.parentBlock->defineSymbol(((MincIdExpr*)varExpr)->name, buildtime.result.type, buildtime.result.value);
			else
			{
				var->value = buildtime.result.value;
				var->type = buildtime.result.type;
			}
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return params[1]->getDerivedExpr()->getType(parentBlock);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsStatic> = $E<PawsStatic>")[0], new BuildtimeVariableReassignmentKernel());

	// Define initial build-time variable assignment
	struct BuildtimeVariableAssignmentKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildtime.result = params[1]->getDerivedExpr()->build(buildtime);
			buildtime.result.value = ((PawsType*)buildtime.result.type)->copy((PawsBase*)buildtime.result.value);

			buildtime.parentBlock->defineSymbol(((MincIdExpr*)params[0])->name, buildtime.result.type, buildtime.result.value);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return params[1]->getDerivedExpr()->getType(parentBlock);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsErrorType> = $E<PawsStatic>")[0], new BuildtimeVariableAssignmentKernel());

	// Define general equivalence operators
	struct EquivalenceKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const MincObject* a = runtime.result;
			if (params[1]->run(runtime))
				return true;
			const MincObject* b = runtime.result;
			runtime.result = new PawsInt(a == b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E == $E")[0], new EquivalenceKernel());

	struct InequivalenceKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const MincObject* a = runtime.result;
			if (params[1]->run(runtime))
				return true;
			const MincObject* b = runtime.result;
			runtime.result = new PawsInt(a != b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E != $E")[0], new InequivalenceKernel());

	// Define if statement
	struct IfKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result;
			return condition->get() ? params[1]->run(runtime) : false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("if($E<PawsInt>) $S"), new IfKernel());

	// Define if/else statement
	struct IfElseKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result;
			return params[condition->get() ? 1 : 2]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("if($E<PawsInt>) $S else $S"), new IfElseKernel());

	// Define inline if expression
	struct InlineIfKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			MincObject* ta = params[1]->build(buildtime).type;
			MincObject* tb = params[2]->build(buildtime).type;
			if (ta != tb)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "operands to ?: have different types <%T> and <%T>", params[1], params[2]);
			buildtime.result.type = ta;
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result;
			return params[condition->get() ? 1 : 2]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			MincObject* ta = params[1]->getType(parentBlock);
			MincObject* tb = params[2]->getType(parentBlock);
			return ta == tb ? ta : getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsInt> ? $E : $E")[0], new InlineIfKernel());

	// Define while statement
	struct WhileKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			size_t cs = runtime.parentBlock->resultCacheIdx;

			// Run condition expression
			if (params[0]->run(runtime))
				return true;

			while (((PawsInt*)runtime.result)->get())
			{
				// Run loop block
				if (params[1]->run(runtime))
					return true;
				runtime.parentBlock->clearCache(cs); // Reset result cache to the state before the while loop to avoid rerunning
													 // previous loop iterations when resuming a coroutine within the loop block

				// Run condition expression
				if (params[0]->run(runtime))
					return true;
			}
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("while($E<PawsInt>) $S"), new WhileKernel());

	// Define for statement
	struct ForKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];
			MincBlockExpr* parentBlock = buildtime.parentBlock;

			// Inherent global scope into loop block scope
			forBlock->parent = parentBlock;

			// Build init expression in loop block scope
			buildtime.parentBlock = forBlock;
			params[0]->build(buildtime);

			// Rebuild condition and update expressions to take loop variable into account
			params[1]->resolve(forBlock);
			params[1]->build(buildtime);
			params[2]->resolve(forBlock);
			params[2]->build(buildtime);

			// Cast condition expression to PawsInt
			MincExpr* condExpr = params[1];
			MincObject* condType = condExpr->getType(forBlock);
			if (condType != PawsInt::TYPE)
			{
				const MincCast* cast = buildtime.parentBlock->lookupCast(condType, PawsInt::TYPE);
				if (cast == nullptr)
					throw CompileError(
						parentBlock, params[1]->loc, "invalid for condition type: %E<%t>, expected: <%t>",
						params[1], condType, PawsInt::TYPE
					);
				// Build condition expression in loop block scope
				(params[1] = new MincCastExpr(cast, condExpr))->build(buildtime);
			}

			// Build loop block in parent scope
			buildtime.parentBlock = parentBlock;
			forBlock->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];
			const MincBlockExpr* parentBlock = runtime.parentBlock;

			// Inherent global scope into loop block scope
			MincEnteredBlockExpr entered(runtime, forBlock);

			// Run init expression in loop block scope
			if (params[0]->run(runtime))
				return true;

			// Run condition expression in loop block scope
			if (params[1]->run(runtime))
				return true;

			while (((PawsInt*)runtime.result)->get())
			{
				// Run loop block in parent scope
				runtime.parentBlock = parentBlock;
				if (entered.run())
					return true;

				// Run update expression in loop block scope
				runtime.parentBlock = forBlock;
				if (params[2]->run(runtime))
					return true;

				// Run condition expression in loop block scope
				if (params[1]->run(runtime))
					return true;
			}
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for($E; $D; $D) $B"), new ForKernel());

	class StringConversionKernel : public MincKernel
	{
		PawsType* const valueType;
	public:
		StringConversionKernel(PawsType* valueType=nullptr) : valueType(valueType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = params[0]->getDerivedExpr())->build(buildtime);
			return new StringConversionKernel((PawsType*)params[0]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (runtime.result == getErrorType())
				runtime.result = new PawsString("ERROR");
			else if (valueType == PawsString::TYPE)
				runtime.result = new PawsString(((PawsString*)runtime.result)->get());
			else if (runtime.result != nullptr)
				runtime.result = new PawsString(valueType->toString((PawsBase*)runtime.result));
			else
				runtime.result = new PawsString("NULL");
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsString::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("str($E<PawsBase>)")[0], new StringConversionKernel());

	defineExpr(pkgScope, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);

	class ParameterizedPrintKernel : public MincKernel
	{
		PawsType* const valueType;
	public:
		ParameterizedPrintKernel(PawsType* valueType=nullptr) : valueType(valueType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = params[0]->getDerivedExpr())->build(buildtime);
			return new ParameterizedPrintKernel((PawsType*)params[0]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (runtime.result == getErrorType())
				std::cout << "ERROR\n";
			else if (valueType == PawsString::TYPE)
				std::cout << ((PawsString*)runtime.result)->get() << '\n';
			else if (runtime.result != nullptr)
				std::cout << valueType->toString((PawsBase*)runtime.result) << '\n';
			else
				std::cout << "NULL\n";

			runtime.result = nullptr;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsVoid::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("print($E<PawsBase>)")[0], new ParameterizedPrintKernel());

	struct UndefinedParameterizedPrintKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincObject* type = params[0]->getType(buildtime.parentBlock);
			throw CompileError(buildtime.parentBlock, params[0]->loc, "print() is undefined for expression of type <%t>", type);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("print($E)")[0], new UndefinedParameterizedPrintKernel());

	defineExpr(pkgScope, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);

	class ParameterizedPrintErrKernel : public MincKernel
	{
		PawsType* const valueType;
	public:
		ParameterizedPrintErrKernel(PawsType* valueType=nullptr) : valueType(valueType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = params[0]->getDerivedExpr())->build(buildtime);
			return new ParameterizedPrintErrKernel((PawsType*)params[0]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (runtime.result == getErrorType())
				std::cerr << "ERROR\n";
			else if (valueType == PawsString::TYPE)
				std::cerr << ((PawsString*)runtime.result)->get() << '\n';
			else if (runtime.result != nullptr)
				std::cerr << valueType->toString((PawsBase*)runtime.result) << '\n';
			else
				std::cerr << "NULL\n";

			runtime.result = nullptr;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsVoid::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("printerr($E<PawsType>)")[0], new ParameterizedPrintErrKernel());

	struct TypeOfKernel : public MincKernel
	{
		const MincSymbol symbol;
	public:
		TypeOfKernel() : symbol() {}
		TypeOfKernel(const MincSymbol& symbol) : symbol(symbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincObject* type = params[0]->getDerivedExpr()->getType(buildtime.parentBlock);
			return new TypeOfKernel(buildtime.result = MincSymbol(PawsType::TYPE, type));
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = symbol.value;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("type($E<PawsBase>)")[0], new TypeOfKernel());

	struct IsInstanceKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			MincObject* fromType = runtime.result;
			if (params[1]->run(runtime))
				return true;
			MincObject* toType = runtime.result;
			runtime.result = new PawsInt(runtime.parentBlock->isInstance(fromType, toType) != 0);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("isInstance($E<PawsType>, $E<PawsType>)")[0], new IsInstanceKernel());

	struct SizeOfKernel : public MincKernel
	{
		const MincSymbol symbol;
	public:
		SizeOfKernel() : symbol() {}
		SizeOfKernel(const MincSymbol& symbol) : symbol(symbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* type = (PawsType*)params[0]->getDerivedExpr()->getType(buildtime.parentBlock);
			return new SizeOfKernel(buildtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(type->size)));
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = symbol.value;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("sizeof($E<PawsBase>)")[0], new SizeOfKernel());

	defineExpr(pkgScope, "parseCFile($E<PawsString>)",
		+[](std::string filename) -> MincBlockExpr* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return MincBlockExpr::parseCFile(fname);
		}
	);

	defineExpr(pkgScope, "parseCCode($E<PawsString>)",
		+[](std::string code) -> MincBlockExpr* {
			return MincBlockExpr::parseCCode(code.c_str());
		}
	);

	defineExpr(pkgScope, "parsePythonFile($E<PawsString>)",
		+[](std::string filename) -> MincBlockExpr* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return MincBlockExpr::parsePythonFile(fname);
		}
	);

	defineExpr(pkgScope, "parsePythonCode($E<PawsString>)",
		+[](std::string code) -> MincBlockExpr* {
			return MincBlockExpr::parsePythonCode(code.c_str());
		}
	);

	class PawsExprTypeKernel : public MincKernel
	{
		PawsType* const type;
	public:
		PawsExprTypeKernel(PawsType* type=nullptr) : type(type) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			PawsType* returnType = (PawsType*)buildtime.result.value;
			buildtime.result = MincSymbol(PawsType::TYPE, PawsTpltType::get(buildtime.parentBlock, PawsExpr::TYPE, returnType));
			return new PawsExprTypeKernel((PawsType*)buildtime.result.value);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = type;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("PawsExpr<$E<PawsType>>")[0], new PawsExprTypeKernel());

	class PawsConstExprTypeKernel : public MincKernel
	{
		PawsType* const type;
	public:
		PawsConstExprTypeKernel(PawsType* type=nullptr) : type(type) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			PawsType* returnType = (PawsType*)buildtime.result.value;
			buildtime.result = MincSymbol(PawsType::TYPE, PawsTpltType::get(buildtime.parentBlock, PawsValue<const MincExpr*>::TYPE, returnType));
			return new PawsConstExprTypeKernel((PawsType*)buildtime.result.value);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}


		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			runtime.result = type;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("PawsConstExpr<$E<PawsType>>")[0], new PawsConstExprTypeKernel());

	defineExpr(pkgScope, "$E<PawsConstExpr>.filename",
		+[](const MincExpr* expr) -> std::string {
			return expr->loc.filename;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.line",
		+[](const MincExpr* expr) -> int {
			return expr->loc.begin_line;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.column",
		+[](const MincExpr* expr) -> int {
			return expr->loc.begin_column;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.endLine",
		+[](const MincExpr* expr) -> int {
			return expr->loc.end_line;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.endColumn",
		+[](const MincExpr* expr) -> int {
			return expr->loc.end_column;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExpr>.parent",
		+[](const MincBlockExpr* pkgScope) -> MincBlockExpr* {
			return pkgScope->parent;
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.parent = $E<PawsBlockExpr>",
		+[](MincBlockExpr* pkgScope, MincBlockExpr* parent) -> void {
			pkgScope->parent = parent;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExpr>.references",
		+[](const MincBlockExpr* pkgScope) -> const std::vector<MincBlockExpr*>& {
			return pkgScope->references;
		}
	);

	// Define getType
	struct ParameterizedGetTypeKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			MincBlockExpr* scope = ((PawsBlockExpr*)runtime.result)->get();
			runtime.result = expr->getType(scope);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsConstExpr>.getType($E<PawsBlockExpr>)")[0], new ParameterizedGetTypeKernel());

	struct ParameterlessGetTypeKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsType* paramType = (PawsType*)params[0]->getDerivedExpr()->getType(runtime.parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
			{
				runtime.result = PawsVoid::TYPE;
				return false;
			}

			if (params[0]->run(runtime))
				return true;
			const MincExpr* expr = ((PawsValue<const MincExpr*>*)runtime.result)->get();
			MincBlockExpr* scope = runtime.parentBlock->parent;
			runtime.result = expr->getType(scope);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsConstExpr>.getType()")[0], new ParameterlessGetTypeKernel());

	// Define build()
	struct BuildKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result)->get();
			const MincBlockExpr* scope = getKernelFromUserData(runtime.parentBlock)->callerScope;
			MincBuildtime buildtime = { const_cast<MincBlockExpr*>(scope) };
			expr->build(buildtime);
			runtime.result = nullptr;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsVoid::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsExpr>.build()")[0], new BuildKernel());

	// Define run()
	struct ParameterizedRunKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsType* paramType = (PawsType*)params[0]->getDerivedExpr()->getType(runtime.parentBlock);

			if (params[0]->run(runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			runtime.parentBlock = ((PawsBlockExpr*)runtime.result)->get();
			if (expr->run(runtime))
				return true;

			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				runtime.result = nullptr;
			// Else, runtime.result is result of expr->run(runtime)

			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* paramType = (PawsType*)params[0]->getDerivedExpr()->getType(parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return PawsVoid::TYPE;
			return ((PawsTpltType*)paramType)->tpltType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsExpr>.run($E<PawsBlockExpr>)")[0], new ParameterizedRunKernel());

	struct ParameterlessRunKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsType* paramType = (PawsType*)params[0]->getDerivedExpr()->getType(runtime.parentBlock);
			
			if (params[0]->run(runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result)->get();
			runtime.parentBlock = getKernelFromUserData(runtime.parentBlock)->callerScope;
			if (expr->run(runtime))
				return true;

			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				runtime.result = nullptr;
			// Else, runtime.result is result of expr->run(runtime)

			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* paramType = (PawsType*)params[0]->getDerivedExpr()->getType(parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return PawsVoid::TYPE;
			return ((PawsTpltType*)paramType)->tpltType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsExpr>.run()")[0], new ParameterlessRunKernel());

	defineExpr(pkgScope, "$E<PawsBlockExpr>.run($E<PawsBlockExpr>)",
		+[](MincExpr* expr, MincBlockExpr* scope) -> void {
			MincRuntime runtime(scope, false);
			expr->run(runtime);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.run($E<PawsNull>)",
		+[](MincBlockExpr* pkgScope) -> void {
			MincRuntime runtime(nullptr, false);
			pkgScope->run(runtime);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.import($E<PawsBlockExpr>)",
		+[](MincBlockExpr* scope, MincBlockExpr* pkgScope) -> void {
			scope->import(pkgScope);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.scopeType",
		+[](MincBlockExpr* scope) -> MincScopeType* {
			return scope->scopeType;
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.scopeType = $E<PawsScopeType>",
		+[](MincBlockExpr* scope, MincScopeType* scopeType) -> MincScopeType* {
			scope->scopeType = scopeType;
			return scopeType;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprList>[$E<PawsInt>]",
		+[](const std::vector<MincBlockExpr*>& blocks, int idx) -> MincBlockExpr* {
			return blocks[idx];
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprList>.length",
		+[](const std::vector<MincBlockExpr*>& blocks) -> int {
			return blocks.size();
		}
	);

	struct ExprListIteratorKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			params[1]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			body->defineSymbol(iterExpr->name, PawsBlockExpr::TYPE, nullptr);
			body->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			if(params[1]->run(runtime))
				return true;
			const std::vector<MincBlockExpr*>& exprs = ((PawsConstBlockExprList*)runtime.result)->get();
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsBlockExpr iter;
			body->defineSymbol(iterExpr->name, PawsBlockExpr::TYPE, &iter);
			for (MincBlockExpr* expr: exprs)
			{
				iter.set(expr);
				if(body->run(runtime))
					return true;
			}
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I: $E<PawsConstBlockExprList>) $B"), new ExprListIteratorKernel());

	defineExpr(pkgScope, "$E<PawsConstLiteralExpr>.value",
		+[](const MincLiteralExpr* expr) -> std::string {
			return expr->value;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstIdExpr>.name",
		+[](const MincIdExpr* expr) -> std::string {
			return expr->name;
		}
	);

	class ExprListGetterKernel : public MincKernel
	{
		PawsTpltType* const exprListType;
	public:
		ExprListGetterKernel(PawsTpltType* exprListType=nullptr) : exprListType(exprListType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = params[0]->getDerivedExpr())->build(buildtime);
			params[1]->build(buildtime);
			return new ExprListGetterKernel((PawsTpltType*)params[0]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;
			MincListExpr* exprs = ((PawsListExpr*)runtime.result)->get();
			if(params[1]->run(runtime))
				return true;
			int idx = ((PawsInt*)runtime.result)->get();
			runtime.result = new PawsExpr(exprs->exprs[idx]);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return ((PawsTpltType*)params[0]->getDerivedExpr()->getType(parentBlock))->tpltType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsListExpr>[$E<PawsInt>]")[0], new ExprListGetterKernel());

	class ListExprIteratorKernel : public MincKernel
	{
		PawsType* const exprType;
	public:
		ListExprIteratorKernel(PawsType* exprType=nullptr) : exprType(exprType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			(params[1] = params[1]->getDerivedExpr())->build(buildtime);
			PawsType* exprType = ((PawsTpltType*)params[1]->getType(buildtime.parentBlock))->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			body->defineSymbol(iterExpr->name, exprType, nullptr);
			body->build(buildtime);
			return new ListExprIteratorKernel(exprType);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			assert(params[1]->exprtype == MincExpr::ExprType::CAST);
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			if(params[1]->run(runtime))
				return true;
			MincListExpr* exprs = ((PawsListExpr*)runtime.result)->get();
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsExpr iter;
			body->defineSymbol(iterExpr->name, exprType, &iter);
			for (MincExpr* expr: exprs->exprs)
			{
				iter.set(expr);
				if(body->run(runtime))
					return true;
			}
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I: $E<PawsListExpr>) $B"), new ListExprIteratorKernel());

	defineExpr(pkgScope, "$E<PawsSym>.type",
		+[](MincSymbol var) -> MincObject* {
			return var.type;
		}
	);

	struct RealpathKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;
			const std::string& path = ((PawsString*)runtime.result)->get();
			char* realPath = realpath(path.c_str(), nullptr);
			if (realPath == nullptr)
				throw CompileError(runtime.parentBlock, params[0]->loc, "%S: No such file or directory", path);
			PawsString* realPathStr = new PawsString(realPath);
			free(realPath);
			runtime.result = realPathStr;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsString::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("realpath($E<PawsString>)")[0], new RealpathKernel());

	// Define MINC package manager import with target scope
	struct ImportKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincBlockExpr* block = ((PawsStaticBlockExpr*)params[0]->build(buildtime).value)->get();
			MincPackageManager* pkgMgr = (MincPackageManager*)&MINC_PACKAGE_MANAGER();
			std::vector<MincExpr*>& pkgPath = ((MincListExpr*)params[1])->exprs;
			std::string pkgName = ((MincIdExpr*)pkgPath[0])->name;
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + '.' + ((MincIdExpr*)pkgPath[i])->name;

			// Import package
			if (!pkgMgr->tryImportPackage(block, pkgName))
				throw CompileError(buildtime.parentBlock, params[0]->loc, "unknown package %S", pkgName);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsVoid::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsStaticBlockExpr>.import($I. ...)")[0], new ImportKernel());

	// Define address-of expression
	struct AddressOfKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = params[0]->getDerivedExpr())->build(buildtime);
			return this;
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if(params[0]->run(runtime))
				return true;
			MincObject* ptr = new PawsValue<uint8_t*>(&((PawsValue<uint8_t>*)runtime.result)->get());
			runtime.result = ptr;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return ((PawsType*)params[0]->getDerivedExpr()->getType(parentBlock))->ptrType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("& $E<PawsBase>")[0], new AddressOfKernel());
});