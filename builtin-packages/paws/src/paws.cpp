#include <string>
#include <map>
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>
#include "minc_api.h"
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
	while (getBlockExprUserType(scope) != &PAWS_KERNEL_ID)
		scope = getBlockExprParent(scope);
	assert(scope != nullptr);
	return (PawsKernel*)getBlockExprUser(scope);
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
		defineSymbol(scope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(scope, t, PawsBase::TYPE); // Let baseType<tpltType> derive from PawsBase
		defineOpaqueInheritanceCast(scope, t, baseType); // Let baseType<tpltType> derive from baseType
	}
	return *iter;
}

void definePawsReturnStmt(MincBlockExpr* scope, const MincObject* returnType, const char* funcName)
{
	if (returnType == PawsVoid::TYPE)
	{
		// Define return statement with incorrect type in function scope
		defineStmt5(scope, "return $E",
			[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("void " + std::string(funcName) + " should not return a value").c_str(), params[0]);
			}, // LCOV_EXCL_LINE
			(void*)funcName
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
				runtime.result = MincSymbol(&PAWS_RETURN_TYPE, nullptr);
				return true;
			}
		);
	}
	else
	{
		// Define return statement with incorrect type in function scope
		defineStmt5(scope, "return $E",
			[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
				MincObject* returnType = getType(params[0], buildtime.parentBlock);
				raiseCompileError(("invalid return type `" + lookupSymbolName2(buildtime.parentBlock, returnType, "UNKNOWN_TYPE") + "`").c_str(), params[0]);
			} // LCOV_EXCL_LINE
		);

		// Define return statement with correct type in function scope
		defineStmt6(scope, ("return $E<" + lookupSymbolName2(scope, returnType, "UNKNOWN_TYPE") + ">").c_str(),
			[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
				buildExpr(params[0], buildtime);
			},
			[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
				if (runExpr(params[0], runtime))
					return true;
				runtime.result.type = &PAWS_RETURN_TYPE;
				return true;
			}
		);

		// Define return statement without type in function scope
		defineStmt5(scope, "return",
			[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("non-void " + std::string(funcName) + " should return a value").c_str(), (MincExpr*)buildtime.parentBlock);
			}, // LCOV_EXCL_LINE
			(void*)funcName
		);
	}
}

void getBlockParameterTypes(MincBlockExpr* scope, const std::vector<MincExpr*> params, std::vector<MincSymbol>& blockParams)
{
	blockParams.reserve(params.size());
	for (MincExpr* param: params)
	{
		PawsType* paramType = PawsExpr::TYPE;
		if (ExprIsPlchld(param))
		{
			MincPlchldExpr* plchldParam = (MincPlchldExpr*)param;
			switch (getPlchldExprLabel(plchldParam))
			{
			default: assert(0); //TODO: Throw exception
			case 'L': paramType = PawsLiteralExpr::TYPE; break;
			case 'I': paramType = PawsIdExpr::TYPE; break;
			case 'B': paramType = PawsBlockExpr::TYPE; break;
			case 'S': break;
			case 'E':
			case 'D':
				if (getPlchldExprSublabel(plchldParam) == nullptr)
					break;
				if (const MincSymbol* var = importSymbol(scope, getPlchldExprSublabel(plchldParam)))
					paramType = PawsTpltType::get(pawsScope, PawsExpr::TYPE, (PawsType*)var->value);
			}
		}
		else if (ExprIsList(param))
		{
			const std::vector<MincExpr*>& listParamExprs = getListExprExprs((MincListExpr*)param);
			if (listParamExprs.size() != 0)
			{
				MincPlchldExpr* plchldParam = (MincPlchldExpr*)listParamExprs.front();
				switch (getPlchldExprLabel(plchldParam))
				{
				default: assert(0); //TODO: Throw exception
				case 'L': paramType = PawsLiteralExpr::TYPE; break;
				case 'I': paramType = PawsIdExpr::TYPE; break;
				case 'B': paramType = PawsBlockExpr::TYPE; break;
				case 'S': break;
				case 'E':
				case 'D':
					if (getPlchldExprSublabel(plchldParam) == nullptr)
						break;
					if (const MincSymbol* var = importSymbol(scope, getPlchldExprSublabel(plchldParam)))
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
	: body(cloneBlockExpr(body)), type(type), blockParams(blockParams), phase(Phase::INIT), activePhase(Phase::INIT), instance(nullptr), callerScope(nullptr)
{
	// Create kernel definition scope
	// All statements within the kernel body are conditionally executed in run or build phase
	MincBlockExpr* kernelDefScope = this->body;
	setBlockExprUser(kernelDefScope, this); // Store kernel in kernel definition block user data
	setBlockExprUserType(kernelDefScope, &PAWS_KERNEL_ID);

	// Define build phase selector statement
	defineStmt6(kernelDefScope, "build:",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase != PawsKernel::Phase::INIT)
				throw CompileError(buildtime.parentBlock, getLocation((MincExpr*)buildtime.parentBlock), "build phase must start at beginning of Paws kernel");
			kernel->phase = PawsKernel::Phase::BUILD;
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			kernel->phase = PawsKernel::Phase::BUILD;
			return false;
		}
	);

	// Define run phase selector statement
	defineStmt6(kernelDefScope, "run:",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::RUN)
				throw CompileError(buildtime.parentBlock, getLocation((MincExpr*)buildtime.parentBlock), "redefinition of Paws kernel run phase");
			kernel->phase = PawsKernel::Phase::RUN;
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			kernel->phase = PawsKernel::Phase::RUN;
			return false;
		}
	);

	// Conditionally execute other statements
	defineDefaultStmt6(kernelDefScope,
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(buildtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT) // If no phase was defined at beginning of Paws kernel, ...
				kernel->phase = PawsKernel::Phase::RUN; // Default to run phase

			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			PawsKernel* kernel = getKernelFromUserData(runtime.parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT)
				kernel->phase = PawsKernel::Phase::RUN;

			if (kernel->phase == kernel->activePhase)
				return runExpr(params[0], runtime);
			return false;
		}
	);

	MincBlockExpr* oldParentBlock = buildtime.parentBlock;
	buildtime.parentBlock = body;
	buildExpr((MincExpr*)kernelDefScope, buildtime);
	buildtime.parentBlock = oldParentBlock;
}

MincKernel* PawsKernel::build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
{
	MincBlockExpr* instance = cloneBlockExpr(body);

	PawsKernel* instanceKernel = new PawsKernel(body, type);
	instanceKernel->activePhase = Phase::BUILD; // Execute build phase statements when running instance
	instanceKernel->instance = instance;
	instanceKernel->callerScope = buildtime.parentBlock;

	setBlockExprUser(instance, instanceKernel); // Store kernel instance in block instance user data
	setBlockExprUserType(instance, &PAWS_KERNEL_ID);

	// Set block parameters
	for (size_t i = 0; i < params.size(); ++i)
		blockParams[i].value = new PawsExpr(params[i]);
	setBlockExprParams(instance, blockParams);

	defineSymbol(instance, "parentBlock", PawsBlockExpr::TYPE, new PawsBlockExpr(buildtime.parentBlock));

	// Execute expression code block
	try
	{
		MincRuntime runtime(getBlockExprParent(body), false);
		if (runExpr((MincExpr*)instance, runtime))
		{
			//TODO: Check if runtime.result.type == &PAWS_RETURN_TYPE
			instanceKernel->buildResult = MincSymbol(type, runtime.result.value); //TODO: Consider changing type of PawsKernel::buildResult to MincObject*, since it should always return PawsKernel::type
			instanceKernel->hasBuildResult = true;
			return instanceKernel;
		}
	}
	catch (ReturnException err)
	{
		instanceKernel->buildResult = err.result;
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

bool PawsKernel::run(MincRuntime& runtime, std::vector<MincExpr*>& params)
{
	if (hasBuildResult)
	{
		runtime.result = buildResult;
		return false;
	}

	activePhase = Phase::RUN; // Execute run phase statements when running instance
	callerScope = runtime.parentBlock;

	// Execute expression code block
	runtime.parentBlock = getBlockExprParent(body);
	try
	{
		if (runExpr((MincExpr*)instance, runtime))
		{
			//TODO: Check if runtime.result.type == &PAWS_RETURN_TYPE
			runtime.result = MincSymbol(type, runtime.result.value); //TODO: Consider changing type of PawsKernel::buildResult to MincObject*, since it should always return PawsKernel::type
			return false;
		}
	}
	catch (ReturnException err)
	{
		runtime.result = err.result;
		return false;
	}

	if (type != getVoid().type && type != PawsVoid::TYPE)
		raiseCompileError("missing return statement in expression block", (MincExpr*)instance);
	runtime.result = getVoid();
	return false;
}

MincObject* PawsKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return type;
}

void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)())
{
	using StmtFunc = void (*)();
	RunBlock codeBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)runtime.parentBlock);
		(*(StmtFunc*)stmtArgs)();
		return false;
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(), PawsType* (*exprTypeFunc)())
{
	using ExprFunc = MincSymbol (*)();
	using ExprTypeFunc = PawsType* (*)();
	RunBlock codeBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)runtime.parentBlock);
		runtime.result = ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first();
		return false;
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second();
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

template<> std::string PawsValue<const MincExpr*>::Type::toString(MincObject* value) const
{
	char* cstr = ExprToString(((PawsValue<const MincExpr*>*)value)->get());
	std::string str(cstr);
	delete[] cstr;
	return str;
}

bool serializePawsValue(const MincBlockExpr* scope, const MincSymbol& value, std::string* valueStr)
{
	if (isInstance(scope, value.type, PawsBase::TYPE))
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
	defineOpaqueTypeCast(pkgScope, PawsStaticBlockExpr::TYPE, PawsBlockExpr::TYPE);
	registerType<PawsListExpr>(pkgScope, "PawsListExpr");
	registerType<PawsLiteralExpr>(pkgScope, "PawsLiteralExpr");
	registerType<PawsIdExpr>(pkgScope, "PawsIdExpr");
	registerType<PawsSym>(pkgScope, "PawsSym");
	registerType<PawsScopeType>(pkgScope, "PawsScopeType");
	registerType<PawsStringMap>(pkgScope, "PawsStringMap");
	registerType<PawsNull>(pkgScope, "PawsNull", true);

	// Create null pointer variable
	defineSymbol(pkgScope, "NULL", PawsNull::TYPE, nullptr);

	// Create data type for matching against undefined symbols
	defineSymbol(pkgScope, "PawsErrorType", PawsType::TYPE, getErrorType());

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
	std::vector<MincSymbol> blockParams;
	blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		blockParams.push_back(MincSymbol(PawsString::TYPE, new PawsString(std::string(argv[i]))));
	setBlockExprParams(pkgScope, blockParams);

	defineExpr7(pkgScope, "getFileScope()",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildtime.result = MincSymbol(PawsStaticBlockExpr::TYPE, new PawsStaticBlockExpr(getFileScope()));
		},
		PawsStaticBlockExpr::TYPE
	);

	defineSymbol(pkgScope, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	defineStmt6(pkgScope, "$E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			return runExpr(params[0], runtime);
		}
	);

	// Define context-free pkgScope statement
	defineStmt6(pkgScope, "$B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			return runExpr(params[0], runtime);
		}
	);

	// Define general bracketed expression
	defineExpr10(pkgScope, "($E)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			return runExpr(params[0], runtime);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(params[0], parentBlock);
		}
	);

	// Define return statement
	definePawsReturnStmt(pkgScope, PawsInt::TYPE);

	// Overwrite return statement with correct type in function scope to call quit() instead of raising ReturnException
	defineStmt(pkgScope, "return $E<PawsInt>",
		+[](int returnCode) {
			quit(returnCode);
		} // LCOV_EXCL_LINE
	);

	// Define variable lookup
	class VariableLookupKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		VariableLookupKernel() : varId(MincSymbolId::NONE) {}
		VariableLookupKernel(MincSymbolId varId) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == MincSymbolId::NONE)
				raiseCompileError(("`" + std::string(getIdExprName((MincIdExpr*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			buildtime.result = *getSymbol(buildtime.parentBlock, varId);
			return new VariableLookupKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* varFromId = getSymbol(runtime.parentBlock, varId);
			assert(varFromId != nullptr);
			runtime.result = *varFromId;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const MincSymbol* var = lookupSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return var != nullptr ? var->type : getErrorType();
		}
	};
	defineExpr6(pkgScope, "$I<PawsDynamic>", new VariableLookupKernel());

	// Define build-time variable lookup
	defineExpr8(pkgScope, "$I<PawsStatic>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			const MincSymbol* var = importSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprName((MincIdExpr*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			buildtime.result = *var;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			const MincSymbol* var = lookupSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return var != nullptr ? var->type : getErrorType();
		}
	);

	// Define literal definition
	class LiteralDefinitionKernel : public MincKernel
	{
		const MincSymbol var;
	public:
		LiteralDefinitionKernel() : var(nullptr, nullptr) {}
		LiteralDefinitionKernel(const MincSymbol& var) : var(var) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				return new LiteralDefinitionKernel(buildtime.result = MincSymbol(PawsString::TYPE, new PawsString(std::string(valueStart, valueEnd - valueStart))));
			}

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return new LiteralDefinitionKernel(buildtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(doubleValue)));
			}
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return new LiteralDefinitionKernel(buildtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(intValue)));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			runtime.result = var;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;
			if (*valueEnd == '"' || *valueEnd == '\'')
				return PawsString::TYPE;
			if (strchr(value, '.'))
				return PawsDouble::TYPE;
			return PawsInt::TYPE;
		}
	};
	defineExpr6(pkgScope, "$L", new LiteralDefinitionKernel());

	// Define variable (re)assignment
	class VariableAssignmentKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		VariableAssignmentKernel() : varId(MincSymbolId::NONE) {}
		VariableAssignmentKernel(MincSymbolId varId) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1] = getDerivedExpr(params[1]);
			buildExpr(params[1], buildtime);
			MincObject* valType = ::getType(params[1], buildtime.parentBlock);

			MincExpr* varExpr = params[0];
			if (ExprIsCast(varExpr))
				varExpr = getCastExprSource((MincCastExpr*)varExpr);
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)varExpr));
			if (varId == MincSymbolId::NONE) // If variable is undefined
			{
				// Define new variable
				defineSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)varExpr), valType, nullptr);
				varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)varExpr));
			}
			else // If variable already exists
				getSymbol(buildtime.parentBlock, varId)->type = valType; // Update type
			return new VariableAssignmentKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[1], runtime))
				return true;
			MincSymbol* varFromId = getSymbol(runtime.parentBlock, varId);
			varFromId->value = runtime.result.value = ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value);
			varFromId->type = runtime.result.type;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return ::getType(getDerivedExpr(params[1]), parentBlock);
		}
	};
	defineExpr6(pkgScope, "$I<PawsDynamic> = $E<PawsDynamic>", new VariableAssignmentKernel());
	defineExpr6(pkgScope, "$I<PawsErrorType> = $E<PawsDynamic>", new VariableAssignmentKernel());

	// Define build-time variable reassignment
	defineExpr8(pkgScope, "$I<PawsStatic> = $E<PawsStatic>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildtime.result = buildExpr(getDerivedExpr(params[1]), buildtime);
			buildtime.result.value = ((PawsType*)buildtime.result.type)->copy((PawsBase*)buildtime.result.value);

			MincExpr* varExpr = params[0];
			if (ExprIsCast(varExpr))
				varExpr = getCastExprSource((MincCastExpr*)varExpr);
			MincSymbol* var = importSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)varExpr));
			if (var == nullptr)
				defineSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)varExpr), buildtime.result.type, buildtime.result.value);
			else
			{
				var->value = buildtime.result.value;
				var->type = buildtime.result.type;
			}
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(getDerivedExpr(params[1]), parentBlock);
		}
	);

	// Define initial build-time variable assignment
	defineExpr8(pkgScope, "$I<PawsErrorType> = $E<PawsStatic>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildtime.result = buildExpr(getDerivedExpr(params[1]), buildtime);
			buildtime.result.value = ((PawsType*)buildtime.result.type)->copy((PawsBase*)buildtime.result.value);

			defineSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]), buildtime.result.type, buildtime.result.value);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(getDerivedExpr(params[1]), parentBlock);
		}
	);

	// Define general equivalence operators
	defineExpr9(pkgScope, "$E == $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const MincObject* a = runtime.result.value;
			if (runExpr(params[1], runtime))
				return true;
			const MincObject* b = runtime.result.value;
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a == b));
			return false;
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E != $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const MincObject* a = runtime.result.value;
			if (runExpr(params[1], runtime))
				return true;
			const MincObject* b = runtime.result.value;
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a != b));
			return false;
		},
		PawsInt::TYPE
	);

	// Define if statement
	defineStmt6(pkgScope, "if($E<PawsInt>) $S",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result.value;
			return condition->get() ? runExpr(params[1], runtime) : false;
		}
	);

	// Define if/else statement
	defineStmt6(pkgScope, "if($E<PawsInt>) $S else $S",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result.value;
			return runExpr(params[condition->get() ? 1 : 2], runtime);
		}
	);

	// Define inline if expression
	defineExpr10(pkgScope, "$E<PawsInt> ? $E : $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			MincObject* ta = buildExpr(params[1], buildtime).type;
			MincObject* tb = buildExpr(params[2], buildtime).type;
			if (ta != tb)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "operands to ?: have different types <%T> and <%T>", params[1], params[2]);
			buildtime.result.type = ta;
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const PawsInt* condition = (PawsInt*)runtime.result.value;
			return runExpr(params[condition->get() ? 1 : 2], runtime);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			return ta == tb ? ta : getErrorType();
		}
	);

	// Define while statement
	defineStmt6(pkgScope, "while($E<PawsInt>) $S",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			size_t cs = getBlockExprCacheState(runtime.parentBlock);

			// Run condition expression
			if (runExpr(params[0], runtime))
				return true;

			while (((PawsInt*)runtime.result.value)->get())
			{
				// Run loop block
				if (runExpr(params[1], runtime))
					return true;
				resetBlockExprCache(runtime.parentBlock, cs); // Reset result cache to the state before the while loop to avoid rerunning
															  // previous loop iterations when resuming a coroutine within the loop block

				// Run condition expression
				if (runExpr(params[0], runtime))
					return true;
			}
			return false;
		}
	);

	// Define for statement
	defineStmt6(pkgScope, "for($E; $D; $D) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];
			MincBlockExpr* parentBlock = buildtime.parentBlock;

			// Inherent global scope into loop block scope
			setBlockExprParent(forBlock, parentBlock);

			// Build init expression in loop block scope
			buildtime.parentBlock = forBlock;
			buildExpr(params[0], buildtime);

			// Rebuild condition and update expressions to take loop variable into account
			resolveExpr(params[1], forBlock);
			buildExpr(params[1], buildtime);
			resolveExpr(params[2], forBlock);
			buildExpr(params[2], buildtime);

			// Cast condition expression to PawsInt
			MincExpr* condExpr = params[1];
			MincObject* condType = getType(condExpr, forBlock);
			if (condType != PawsInt::TYPE)
			{
				condExpr = lookupCast(parentBlock, condExpr, PawsInt::TYPE);
				if (condExpr == nullptr)
					throw CompileError(
						parentBlock, getLocation(params[1]), "invalid for condition type: %E<%t>, expected: <%t>",
						params[1], condType, PawsInt::TYPE
					);
				// Build condition expression in loop block scope
				buildExpr(params[1] = condExpr, buildtime);
			}

			// Build loop block in parent scope
			buildtime.parentBlock = parentBlock;
			buildExpr((MincExpr*)forBlock, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];
			MincBlockExpr* parentBlock = runtime.parentBlock;

			// Inherent global scope into loop block scope
			setBlockExprParent(forBlock, parentBlock);

			// Run init expression in loop block scope
			runtime.parentBlock = forBlock;
			if (runExpr(params[0], runtime))
				return true;

			// Run condition expression in loop block scope
			if (runExpr(params[1], runtime))
				return true;

			while (((PawsInt*)runtime.result.value)->get())
			{
				// Run loop block in parent scope
				runtime.parentBlock = parentBlock;
				if (runExpr((MincExpr*)forBlock, runtime))
					return true;

				// Run update expression in loop block scope
				runtime.parentBlock = forBlock;
				if (runExpr(params[2], runtime))
					return true;

				// Run condition expression in loop block scope
				if (runExpr(params[1], runtime))
					return true;
			}
			return false;
		}
	);

	defineExpr9(pkgScope, "str($E<PawsBase>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(getDerivedExpr(params[0]), runtime))
				return true;
			const MincSymbol& symbol = runtime.result;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.value == getErrorType())
				runtime.result = MincSymbol(PawsString::TYPE, new PawsString("ERROR"));
			else if (symbol.type == PawsString::TYPE)
				runtime.result = MincSymbol(PawsString::TYPE, new PawsString(((PawsString*)symbol.value)->get()));
			else if (symbol.value != nullptr)
				runtime.result = MincSymbol(PawsString::TYPE, new PawsString(((PawsType*)symbol.type)->toString((PawsBase*)symbol.value)));
			else
				runtime.result = MincSymbol(PawsString::TYPE, new PawsString("NULL"));
			return false;
		},
		PawsString::TYPE
	);

	defineExpr(pkgScope, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);
	defineExpr9(pkgScope, "print($E<PawsBase>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(params[0], runtime))
				return true;
			const MincSymbol& symbol = runtime.result;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.value == getErrorType())
				std::cout << "ERROR\n";
			else if (symbol.type == PawsString::TYPE)
				std::cout << ((PawsString*)symbol.value)->get() << '\n';
			else if (symbol.value != nullptr)
				std::cout << ((PawsType*)symbol.type)->toString((PawsBase*)symbol.value) << '\n';
			else
				std::cout << "NULL\n";

			runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
			return false;
		},
		PawsVoid::TYPE
	);
	defineExpr7(pkgScope, "print($E)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincObject* type = getType(params[0], buildtime.parentBlock);
			throw CompileError(buildtime.parentBlock, getLocation(params[0]), "print() is undefined for expression of type <%t>", type);
		},
		getErrorType()
	);

	defineExpr(pkgScope, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);
	defineExpr2(pkgScope, "printerr($E<PawsType>)",
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(getDerivedExpr(params[0]), runtime))
				return true;
			const MincSymbol& symbol = runtime.result;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.type == PawsString::TYPE)
				std::cerr << ((PawsString*)symbol.value)->get() << '\n';
			else
				std::cerr << ((PawsType*)symbol.type)->toString((PawsBase*)symbol.value) << '\n';

			runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
			return false;
		},
		PawsVoid::TYPE
	);

	defineExpr7(pkgScope, "type($E<PawsBase>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincObject* type = getType(getDerivedExpr(params[0]), buildtime.parentBlock);
			buildtime.result = MincSymbol(PawsType::TYPE, type);
		},
		PawsType::TYPE
	);

	defineExpr9(pkgScope, "isInstance($E<PawsType>, $E<PawsType>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			MincObject* fromType = runtime.result.value;
			if (runExpr(params[1], runtime))
				return true;
			MincObject* toType = runtime.result.value;
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(isInstance(runtime.parentBlock, fromType, toType) != 0));
			return false;
		},
		PawsInt::TYPE
	);

	defineExpr7(pkgScope, "sizeof($E<PawsBase>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), buildtime.parentBlock);
			buildtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(type->size));
		},
		PawsInt::TYPE
	);

	defineExpr(pkgScope, "parseCFile($E<PawsString>)",
		+[](std::string filename) -> MincBlockExpr* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parseCFile(fname);
		}
	);

	defineExpr(pkgScope, "parseCCode($E<PawsString>)",
		+[](std::string code) -> MincBlockExpr* {
			return parseCCode(code.c_str());
		}
	);

	defineExpr(pkgScope, "parsePythonFile($E<PawsString>)",
		+[](std::string filename) -> MincBlockExpr* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parsePythonFile(fname);
		}
	);

	defineExpr(pkgScope, "parsePythonCode($E<PawsString>)",
		+[](std::string code) -> MincBlockExpr* {
			return parsePythonCode(code.c_str());
		}
	);

	defineExpr9(pkgScope, "PawsExpr<$E<PawsType>>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			PawsType* returnType = (PawsType*)runtime.result.value;
			runtime.result = MincSymbol(PawsType::TYPE, PawsTpltType::get(runtime.parentBlock, PawsExpr::TYPE, returnType));
			return false;
		},
		PawsType::TYPE
	);
	defineExpr9(pkgScope, "PawsConstExpr<$E<PawsType>>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			PawsType* returnType = (PawsType*)runtime.result.value;
			runtime.result = MincSymbol(PawsType::TYPE, PawsTpltType::get(runtime.parentBlock, PawsValue<const MincExpr*>::TYPE, returnType));
			return false;
		},
		PawsType::TYPE
	);

	defineExpr(pkgScope, "$E<PawsConstExpr>.filename",
		+[](const MincExpr* expr) -> std::string {
			return getExprFilename(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.line",
		+[](const MincExpr* expr) -> int {
			return getExprLine(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.column",
		+[](const MincExpr* expr) -> int {
			return getExprColumn(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.endLine",
		+[](const MincExpr* expr) -> int {
			return getExprEndLine(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr>.endColumn",
		+[](const MincExpr* expr) -> int {
			return getExprEndColumn(expr);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExpr>.parent",
		+[](const MincBlockExpr* pkgScope) -> MincBlockExpr* {
			return getBlockExprParent(pkgScope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.parent = $E<PawsBlockExpr>",
		+[](MincBlockExpr* pkgScope, MincBlockExpr* parent) -> void {
			setBlockExprParent(pkgScope, parent);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExpr>.references",
		+[](const MincBlockExpr* pkgScope) -> const std::vector<MincBlockExpr*>& {
			return getBlockExprReferences(pkgScope);
		}
	);

	// Define getType
	defineExpr9(pkgScope, "$E<PawsConstExpr>.getType($E<PawsBlockExpr>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result.value)->get();
			if (runExpr(params[1], runtime))
				return true;
			MincBlockExpr* scope = ((PawsBlockExpr*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsType::TYPE, getType(expr, scope));
			return false;
		},
		PawsType::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsConstExpr>.getType()",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, runtime.parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
			{
				runtime.result = MincSymbol(PawsType::TYPE, PawsVoid::TYPE);
				return false;
			}

			if (runExpr(params[0], runtime))
				return true;
			const MincExpr* expr = ((PawsValue<const MincExpr*>*)runtime.result.value)->get();
			MincBlockExpr* scope = getBlockExprParent(runtime.parentBlock);
			runtime.result = MincSymbol(PawsType::TYPE, getType(expr, scope));
			return false;
		},
		PawsType::TYPE
	);

	// Define build()
	defineExpr9(pkgScope, "$E<PawsExpr>.build()",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result.value)->get();
			MincBlockExpr* scope = getKernelFromUserData(runtime.parentBlock)->callerScope;
			MincBuildtime buildtime = { scope };
			buildExpr(expr, buildtime);
			runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
			return false;
		},
		PawsVoid::TYPE
	);

	// Define run()
	defineExpr10(pkgScope, "$E<PawsExpr>.run($E<PawsBlockExpr>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, runtime.parentBlock);

			if (runExpr(params[0], runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result.value)->get();
			if (runExpr(params[1], runtime))
				return true;
			runtime.parentBlock = ((PawsBlockExpr*)runtime.result.value)->get();
			if (runExpr(expr, runtime))
				return true;

			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
			// Else, runtime.result is result of runExpr(expr, runtime)

			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return PawsVoid::TYPE;
			return ((PawsTpltType*)paramType)->tpltType;
		}
	);
	defineExpr10(pkgScope, "$E<PawsExpr>.run()",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, runtime.parentBlock);
			
			if (runExpr(params[0], runtime))
				return true;
			MincExpr* expr = ((PawsExpr*)runtime.result.value)->get();
			runtime.parentBlock = getKernelFromUserData(runtime.parentBlock)->callerScope;
			if (runExpr(expr, runtime))
				return true;

			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);
			// Else, runtime.result is result of runExpr(expr, runtime)

			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return PawsVoid::TYPE;
			return ((PawsTpltType*)paramType)->tpltType;
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.run($E<PawsBlockExpr>)",
		+[](MincExpr* expr, MincBlockExpr* scope) -> void {
			MincRuntime runtime(scope, false);
			runExpr(expr, runtime);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.run($E<PawsNull>)",
		+[](MincBlockExpr* pkgScope) -> void {
			MincRuntime runtime(nullptr, false);
			runExpr((MincExpr*)pkgScope, runtime);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.import($E<PawsBlockExpr>)",
		+[](MincBlockExpr* scope, MincBlockExpr* pkgScope) -> void {
			importBlock(scope, pkgScope);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExpr>.scopeType",
		+[](MincBlockExpr* scope) -> MincScopeType* {
			return getScopeType(scope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.scopeType = $E<PawsScopeType>",
		+[](MincBlockExpr* scope, MincScopeType* scopeType) -> MincScopeType* {
			setScopeType(scope, scopeType);
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

	defineStmt6(pkgScope, "for ($I: $E<PawsConstBlockExprList>) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			buildExpr(params[1], buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(iterExpr), PawsBlockExpr::TYPE, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			if(runExpr(params[1], runtime))
				return true;
			const std::vector<MincBlockExpr*>& exprs = ((PawsConstBlockExprList*)runtime.result.value)->get();
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsBlockExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), PawsBlockExpr::TYPE, &iter);
			for (MincBlockExpr* expr: exprs)
			{
				iter.set(expr);
				if(runExpr((MincExpr*)body, runtime))
					return true;
			}
			return false;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstLiteralExpr>.value",
		+[](const MincLiteralExpr* expr) -> std::string {
			return getLiteralExprValue(expr);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstIdExpr>.name",
		+[](const MincIdExpr* expr) -> std::string {
			return getIdExprName(expr);
		}
	);

	defineExpr10(pkgScope, "$E<PawsListExpr>[$E<PawsInt>]",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(params[0], runtime))
				return true;
			PawsTpltType* exprsType = (PawsTpltType*)runtime.result.type;
			MincListExpr* exprs = ((PawsListExpr*)runtime.result.value)->get();
			if(runExpr(params[1], runtime))
				return true;
			int idx = ((PawsInt*)runtime.result.value)->get();
			runtime.result = MincSymbol(exprsType->tpltType, new PawsExpr(getListExprExprs(exprs)[idx]));
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return ((PawsTpltType*)getType(getDerivedExpr(params[0]), parentBlock))->tpltType;
		}
	);

	defineStmt6(pkgScope, "for ($I: $E<PawsListExpr>) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			buildExpr(params[1] = getDerivedExpr(params[1]), buildtime);
			PawsType* exprType = ((PawsTpltType*)getType(params[1], buildtime.parentBlock))->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(iterExpr), exprType, nullptr);
			buildExpr((MincExpr*)body, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			assert(ExprIsCast(params[1]));
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			if(runExpr(params[1], runtime))
				return true;
			MincListExpr* exprs = ((PawsListExpr*)runtime.result.value)->get();
			PawsType* exprType = ((PawsTpltType*)runtime.result.type)->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), exprType, &iter);
			for (MincExpr* expr: getListExprExprs(exprs))
			{
				iter.set(expr);
				if(runExpr((MincExpr*)body, runtime))
					return true;
			}
			return false;
		}
	);

	defineExpr(pkgScope, "$E<PawsSym>.type",
		+[](MincSymbol var) -> MincObject* {
			return var.type;
		}
	);

	defineExpr9(pkgScope, "realpath($E<PawsString>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(params[0], runtime))
				return true;
			const std::string& path = ((PawsString*)runtime.result.value)->get();
			char* realPath = realpath(path.c_str(), nullptr);
			if (realPath == nullptr)
				raiseCompileError((path + ": No such file or directory").c_str(), params[0]);
			PawsString* realPathStr = new PawsString(realPath);
			free(realPath);
			runtime.result = MincSymbol(PawsString::TYPE, realPathStr);
			return false;
		}, PawsString::TYPE
	);

	// Define MINC package manager import with target scope
	defineExpr7(pkgScope, "$E<PawsStaticBlockExpr>.import($I. ...)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			MincBlockExpr* block = ((PawsStaticBlockExpr*)buildExpr(params[0], buildtime).value)->get();
			MincPackageManager* pkgMgr = (MincPackageManager*)exprArgs;
			std::vector<MincExpr*>& pkgPath = getListExprExprs((MincListExpr*)params[1]);
			std::string pkgName = getIdExprName((MincIdExpr*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + '.' + getIdExprName((MincIdExpr*)pkgPath[i]);

			// Import package
			if (!pkgMgr->tryImportPackage(block, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
		}, PawsVoid::TYPE, &MINC_PACKAGE_MANAGER()
	);

	// Define address-of expression
	defineExpr10(pkgScope, "& $E<PawsBase>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if(runExpr(params[0], runtime))
				return true;
			MincObject* ptr = new PawsValue<uint8_t*>(&((PawsValue<uint8_t>*)runtime.result.value)->get());
			runtime.result = MincSymbol(((PawsType*)runtime.result.type)->ptrType, ptr);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return ((PawsType*)getType(getDerivedExpr(params[0]), parentBlock))->ptrType;
		}
	);
});