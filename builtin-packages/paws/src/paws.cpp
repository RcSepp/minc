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
MincBlockExpr* pawsScope = nullptr;

struct PawsStaticBlockExpr : public PawsBlockExpr
{
public:
	static PawsType* const TYPE;
	PawsStaticBlockExpr() : PawsBlockExpr() {}
	PawsStaticBlockExpr(MincBlockExpr* val) : PawsBlockExpr(val) {}
};
inline PawsType* const PawsStaticBlockExpr::TYPE = new PawsBlockExpr::Type();

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
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("void " + std::string(funcName) + " should not return a value").c_str(), params[0]);
			}, // LCOV_EXCL_LINE
			(void*)funcName
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				throw ReturnException(MincSymbol(PawsVoid::TYPE, nullptr));
			}
		);
	}
	else
	{
		// Define return statement with incorrect type in function scope
		defineStmt5(scope, "return $E",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				MincObject* returnType = getType(params[0], parentBlock);
				raiseCompileError(("invalid return type `" + lookupSymbolName2(parentBlock, returnType, "UNKNOWN_TYPE") + "`").c_str(), params[0]);
			} // LCOV_EXCL_LINE
		);

		// Define return statement with correct type in function scope
		defineStmt6(scope, ("return $E<" + lookupSymbolName2(scope, returnType, "UNKNOWN_TYPE") + ">").c_str(),
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				buildExpr(params[0], parentBlock);
			},
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				throw ReturnException(runExpr(params[0], parentBlock));
			}
		);

		// Define return statement without type in function scope
		defineStmt5(scope, "return",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("non-void " + std::string(funcName) + " should return a value").c_str(), (MincExpr*)parentBlock);
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

PawsKernel::PawsKernel(MincBlockExpr* body, MincObject* type, const std::vector<MincSymbol>& blockParams)
	: body(cloneBlockExpr(body)), type(type), blockParams(blockParams), phase(Phase::INIT), activePhase(Phase::INIT), instance(nullptr), callerScope(nullptr)
{
	// Create kernel definition scope
	// All statements within the kernel body are conditionally executed in run or build phase
	MincBlockExpr* kernelDefScope = this->body;
	setBlockExprUser(kernelDefScope, this); // Store kernel in kernel definition block user data
	setBlockExprUserType(kernelDefScope, &PAWS_KERNEL_ID);

	// Define build phase selector statement
	defineStmt6(kernelDefScope, "build:",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			if (kernel->phase != PawsKernel::Phase::INIT)
				throw CompileError(parentBlock, getLocation((MincExpr*)parentBlock), "build phase must start at beginning of Paws kernel");
			kernel->phase = PawsKernel::Phase::BUILD;
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			kernel->phase = PawsKernel::Phase::BUILD;
		}
	);

	// Define run phase selector statement
	defineStmt6(kernelDefScope, "run:",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			if (kernel->phase == PawsKernel::Phase::RUN)
				throw CompileError(parentBlock, getLocation((MincExpr*)parentBlock), "redefinition of Paws kernel run phase");
			kernel->phase = PawsKernel::Phase::RUN;
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			kernel->phase = PawsKernel::Phase::RUN;
		}
	);

	// Conditionally execute other statements
	defineDefaultStmt6(kernelDefScope,
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT) // If no phase was defined at beginning of Paws kernel, ...
				kernel->phase = PawsKernel::Phase::RUN; // Default to run phase

			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsKernel* kernel = getKernelFromUserData(parentBlock);
			if (kernel->phase == PawsKernel::Phase::INIT)
				kernel->phase = PawsKernel::Phase::RUN;

			if (kernel->phase == kernel->activePhase)
				runExpr(params[0], parentBlock);
		}
	);

	buildExpr((MincExpr*)kernelDefScope, body);
}

MincKernel* PawsKernel::build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
{
	MincBlockExpr* instance = cloneBlockExpr(body);

	PawsKernel* instanceKernel = new PawsKernel(body, type);
	instanceKernel->activePhase = Phase::BUILD; // Execute build phase statements when running instance
	instanceKernel->instance = instance;
	instanceKernel->callerScope = parentBlock;

	setBlockExprUser(instance, instanceKernel); // Store kernel instance in block instance user data
	setBlockExprUserType(instance, &PAWS_KERNEL_ID);

	// Set block parameters
	for (size_t i = 0; i < params.size(); ++i)
		blockParams[i].value = new PawsExpr(params[i]);
	setBlockExprParams(instance, blockParams);

	defineSymbol(instance, "parentBlock", PawsBlockExpr::TYPE, new PawsBlockExpr(parentBlock));

	// Execute expression code block
	try
	{
		runExpr((MincExpr*)instance, getBlockExprParent(body));
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

MincSymbol PawsKernel::run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
{
	if (hasBuildResult)
		return buildResult;

	activePhase = Phase::RUN; // Execute run phase statements when running instance
	callerScope = parentBlock;

	// Execute expression code block
	try
	{
		runExpr((MincExpr*)instance, getBlockExprParent(body));
	}
	catch (ReturnException err)
	{
		return err.result;
	}

	if (type != getVoid().type && type != PawsVoid::TYPE)
		raiseCompileError("missing return statement in expression block", (MincExpr*)instance);
	return getVoid();
}

MincObject* PawsKernel::getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
{
	return type;
}

void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)())
{
	using StmtFunc = void (*)();
	StmtBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs){
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		(*(StmtFunc*)stmtArgs)();
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(), PawsType* (*exprTypeFunc)())
{
	using ExprFunc = MincSymbol (*)();
	using ExprTypeFunc = PawsType* (*)();
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first();
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

	defineExpr2(pkgScope, "getFileScope()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsStaticBlockExpr::TYPE, new PawsStaticBlockExpr(getFileScope()));
		},
		PawsStaticBlockExpr::TYPE
	);

	defineSymbol(pkgScope, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	defineStmt6(pkgScope, "$E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			runExpr(params[0], parentBlock);
		}
	);

	// Define context-free pkgScope statement
	defineStmt6(pkgScope, "$B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			runExpr(params[0], parentBlock);
		}
	);

	// Define general bracketed expression
	defineExpr10(pkgScope, "($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return runExpr(params[0], parentBlock);
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

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbolId varId = lookupSymbolId(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return new VariableLookupKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbol* varFromId = getSymbol(parentBlock, varId);
			if (varFromId == nullptr)
				raiseCompileError(("`" + std::string(getIdExprName((MincIdExpr*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *varFromId;
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprName((MincIdExpr*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *var;
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
		LiteralDefinitionKernel(MincObject* type, MincObject* value) : var(type, value) {}

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				return new LiteralDefinitionKernel(PawsString::TYPE, new PawsString(std::string(valueStart, valueEnd - valueStart)));
			}

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return new LiteralDefinitionKernel(PawsDouble::TYPE, new PawsDouble(doubleValue));
			}
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return new LiteralDefinitionKernel(PawsInt::TYPE, new PawsInt(intValue));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			return var;
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

		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			params[1] = getDerivedExpr(params[1]);
			buildExpr(params[1], parentBlock);
			MincObject* valType = ::getType(params[1], parentBlock);

			MincExpr* varExpr = params[0];
			if (ExprIsCast(varExpr))
				varExpr = getCastExprSource((MincCastExpr*)varExpr);
			MincSymbolId varId = lookupSymbolId(parentBlock, getIdExprName((MincIdExpr*)varExpr));
			if (varId == MincSymbolId::NONE)
			{
				defineSymbol(parentBlock, getIdExprName((MincIdExpr*)varExpr), valType, nullptr);
				varId = lookupSymbolId(parentBlock, getIdExprName((MincIdExpr*)varExpr));
			}
			return new VariableAssignmentKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbol value = runExpr(params[1], parentBlock);
			MincSymbol* varFromId = getSymbol(parentBlock, varId);
			varFromId->value = ((PawsType*)value.type)->copy((PawsBase*)value.value);
			varFromId->type = value.type;
			return value;
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			params[1] = getDerivedExpr(params[1]);
			buildExpr(params[1], parentBlock);
			MincSymbol value = runExpr(params[1], parentBlock);

			MincExpr* varExpr = params[0];
			if (ExprIsCast(varExpr))
				varExpr = getCastExprSource((MincCastExpr*)varExpr);
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)varExpr));
			if (var == nullptr)
				defineSymbol(parentBlock, getIdExprName((MincIdExpr*)varExpr), value.type, ((PawsType*)value.type)->copy((PawsBase*)value.value));
			else
			{
				var->value = ((PawsType*)value.type)->copy((PawsBase*)value.value);
				var->type = value.type;
			}
			return value;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(getDerivedExpr(params[1]), parentBlock);
		}
	);

	// Define initial build-time variable assignment
	defineExpr8(pkgScope, "$I<PawsErrorType> = $E<PawsStatic>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			buildExpr(params[1], parentBlock);
			MincSymbol value = runExpr(getDerivedExpr(params[1]), parentBlock);

			defineSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]), value.type, ((PawsType*)value.type)->copy((PawsBase*)value.value));
			return value;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(getDerivedExpr(params[1]), parentBlock);
		}
	);

	// Define is-NULL
	defineExpr9(pkgScope, "$E == NULL",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(runExpr(params[0], parentBlock).value == nullptr));
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E != NULL",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(runExpr(params[0], parentBlock).value != nullptr));
		},
		PawsInt::TYPE
	);

	// Define type relations
	defineExpr9(pkgScope, "$E<PawsType> == $E<PawsType>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(runExpr(params[0], parentBlock).value == runExpr(params[1], parentBlock).value));
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsType> != $E<PawsType>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(runExpr(params[0], parentBlock).value != runExpr(params[1], parentBlock).value));
		},
		PawsInt::TYPE
	);

	// Define pointer equivalence operators
	//TODO: Generalize this beyond PawsConstExpr
	defineExpr(pkgScope, "$E<PawsConstExpr> == NULL",
		+[](const MincExpr* a) -> int {
			return a == nullptr;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr> != NULL",
		+[](const MincExpr* a) -> int {
			return a != nullptr;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr> == $E<PawsConstExpr>",
		+[](const MincExpr* a, const MincExpr* b) -> int {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExpr> != $E<PawsConstExpr>",
		+[](const MincExpr* a, const MincExpr* b) -> int {
			return a != b;
		}
	);

	// Define if statement
	defineStmt6(pkgScope, "if($E<PawsInt>) $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)runExpr(params[0], parentBlock).value;
			if (condition->get())
				runExpr(params[1], parentBlock);
		}
	);

	// Define if/else statement
	defineStmt6(pkgScope, "if($E<PawsInt>) $S else $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
			buildExpr(params[2], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)runExpr(params[0], parentBlock).value;
			if (condition->get())
				runExpr(params[1], parentBlock);
			else
				runExpr(params[2], parentBlock);
		}
	);

	// Define inline if expression
	defineExpr10(pkgScope, "$E<PawsInt> ? $E : $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
			buildExpr(params[2], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			if (ta != tb)
				throw CompileError(parentBlock, getLocation(params[0]), "operands to ?: have different types <%T> and <%T>", params[1], params[2]);

			return runExpr(params[((PawsInt*)runExpr(params[0], parentBlock).value)->get() ? 1 : 2], parentBlock);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			return ta == tb ? ta : getErrorType();
		}
	);

	// Define while statement
	defineStmt6(pkgScope, "while($E<PawsInt>) $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			size_t cs = getBlockExprCacheState(parentBlock);
			while (((PawsInt*)runExpr(params[0], parentBlock).value)->get())
			{
				runExpr(params[1], parentBlock);
				resetBlockExprCache(parentBlock, cs); // Reset result cache to the state before the while loop to avoid rerunning
													  // previous loop iterations when resuming a coroutine within the loop block
			}
		}
	);

	// Define for statement
	defineStmt6(pkgScope, "for($E; $D; $D) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];

			// Inherent global scope into loop block scope
			setBlockExprParent(forBlock, parentBlock);

			// Build init expression in loop block scope
			buildExpr(params[0], forBlock);

			// Rebuild condition and update expressions to take loop variable into account
			resolveExpr(params[1], forBlock);
			buildExpr(params[1], forBlock);
			resolveExpr(params[2], forBlock);
			buildExpr(params[2], forBlock);

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
				buildExpr(params[1] = condExpr, forBlock);
			}

			buildExpr((MincExpr*)forBlock, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];

			// Inherent global scope into loop block scope
			setBlockExprParent(forBlock, parentBlock);

			// Run init expression in loop block scope
			runExpr(params[0], forBlock);

			while (((PawsInt*)runExpr(params[1], forBlock).value)->get()) // Run condition expression in loop block scope
			{
				// Run loop block in parent scope
				runExpr((MincExpr*)forBlock, parentBlock);

				// Run update expression in loop block scope
				runExpr(params[2], forBlock);
			}
		}
	);

	defineExpr9(pkgScope, "str($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol symbol = runExpr(getDerivedExpr(params[0]), parentBlock);

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.value == getErrorType())
				return MincSymbol(PawsString::TYPE, new PawsString("ERROR"));
			else if (symbol.type == PawsString::TYPE)
				return MincSymbol(PawsString::TYPE, new PawsString(((PawsString*)symbol.value)->get()));
			else if (symbol.value != nullptr)
				return MincSymbol(PawsString::TYPE, new PawsString(((PawsType*)symbol.type)->toString((PawsBase*)symbol.value)));
			else
				return MincSymbol(PawsString::TYPE, new PawsString("NULL"));
		},
		PawsString::TYPE
	);

	defineExpr(pkgScope, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);
	defineExpr9(pkgScope, "print($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol symbol = runExpr(params[0], parentBlock);

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.value == getErrorType())
				std::cout << "ERROR\n";
			else if (symbol.type == PawsString::TYPE)
				std::cout << ((PawsString*)symbol.value)->get() << '\n';
			else if (symbol.value != nullptr)
				std::cout << ((PawsType*)symbol.type)->toString((PawsBase*)symbol.value) << '\n';
			else
				std::cout << "NULL\n";

			return MincSymbol(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(pkgScope, "print($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincObject* type = getType(params[0], parentBlock);
			throw CompileError(parentBlock, getLocation(params[0]), "print() is undefined for expression of type <%t>", type);
		},
		getErrorType()
	);

	defineExpr(pkgScope, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);
	defineExpr2(pkgScope, "printerr($E<PawsType>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol symbol = runExpr(getDerivedExpr(params[0]), parentBlock);

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			if (symbol.type == PawsString::TYPE)
				std::cerr << ((PawsString*)symbol.value)->get() << '\n';
			else
				std::cerr << ((PawsType*)symbol.type)->toString((PawsBase*)symbol.value) << '\n';

			return MincSymbol(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(pkgScope, "type($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincObject* type = getType(getDerivedExpr(params[0]), parentBlock);
			return MincSymbol(PawsType::TYPE, type);
		},
		PawsType::TYPE
	);

	defineExpr9(pkgScope, "isInstance($E<PawsType>, $E<PawsType>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			bool isInst = isInstance(parentBlock, runExpr(params[0], parentBlock).value, runExpr(params[1], parentBlock).value);
			return MincSymbol(PawsInt::TYPE, new PawsInt(isInst != 0));
		},
		PawsInt::TYPE
	);

	defineExpr2(pkgScope, "sizeof($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return MincSymbol(PawsInt::TYPE, new PawsInt(type->size));
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)runExpr(params[0], parentBlock).value;
			return MincSymbol(PawsType::TYPE, PawsTpltType::get(parentBlock, PawsExpr::TYPE, returnType));
		},
		PawsType::TYPE
	);
	defineExpr9(pkgScope, "PawsConstExpr<$E<PawsType>>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)runExpr(params[0], parentBlock).value;
			return MincSymbol(PawsType::TYPE, PawsTpltType::get(parentBlock, PawsValue<const MincExpr*>::TYPE, returnType));
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = ((PawsBlockExpr*)runExpr(params[1], parentBlock).value)->get();
			return MincSymbol(PawsType::TYPE, getType(expr, scope));
		},
		PawsType::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsConstExpr>.getType()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return MincSymbol(PawsType::TYPE, PawsVoid::TYPE);

			const MincExpr* expr = ((PawsValue<const MincExpr*>*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = getBlockExprParent(parentBlock);
			return MincSymbol(PawsType::TYPE, getType(expr, scope));
		},
		PawsType::TYPE
	);

	// Define build()
	defineExpr9(pkgScope, "$E<PawsExpr>.build()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = getKernelFromUserData(parentBlock)->callerScope;
			buildExpr(expr, scope);
			return MincSymbol(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	// Define run()
	defineExpr10(pkgScope, "$E<PawsExpr>.run($E<PawsBlockExpr>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = ((PawsBlockExpr*)runExpr(params[1], parentBlock).value)->get();
			MincSymbol sym = runExpr(expr, scope);

			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return MincSymbol(PawsVoid::TYPE, nullptr);

			return sym;
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)runExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = getKernelFromUserData(parentBlock)->callerScope;
			MincSymbol sym = runExpr(expr, scope);

			MincExpr* param = params[0];
			while (ExprIsCast(param))
				param = getDerivedExpr(param);
			PawsType* paramType = (PawsType*)getType(param, parentBlock);
			if (paramType == PawsExpr::TYPE || paramType == PawsLiteralExpr::TYPE || paramType == PawsIdExpr::TYPE || paramType == PawsBlockExpr::TYPE)
				return MincSymbol(PawsVoid::TYPE, nullptr);

			return sym;
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
			runExpr(expr, scope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.run(NULL)",
		+[](MincBlockExpr* pkgScope) -> void {
			runExpr((MincExpr*)pkgScope, nullptr);
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			buildExpr(params[1], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(iterExpr), PawsBlockExpr::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			MincSymbol exprsVar = runExpr(params[1], parentBlock);
			const std::vector<MincBlockExpr*>& exprs = ((PawsConstBlockExprList*)exprsVar.value)->get();
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsBlockExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), PawsBlockExpr::TYPE, &iter);
			for (MincBlockExpr* expr: exprs)
			{
				iter.set(expr);
				runExpr((MincExpr*)body, parentBlock);
			}
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol exprsVar = runExpr(params[0], parentBlock);
			MincListExpr* exprs = ((PawsListExpr*)exprsVar.value)->get();
			int idx = ((PawsInt*)runExpr(params[1], parentBlock).value)->get();
			return MincSymbol(((PawsTpltType*)exprsVar.type)->tpltType, new PawsExpr(getListExprExprs(exprs)[idx]));
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return ((PawsTpltType*)getType(getDerivedExpr(params[0]), parentBlock))->tpltType;
		}
	);

	defineStmt6(pkgScope, "for ($I: $E<PawsListExpr>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			buildExpr(params[1] = getDerivedExpr(params[1]), parentBlock);
			PawsType* exprType = ((PawsTpltType*)getType(params[1], parentBlock))->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			defineSymbol(body, getIdExprName(iterExpr), exprType, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			assert(ExprIsCast(params[1]));
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			MincSymbol exprsVar = runExpr(params[1], parentBlock);
			MincListExpr* exprs = ((PawsListExpr*)exprsVar.value)->get();
			PawsType* exprType = ((PawsTpltType*)exprsVar.type)->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), exprType, &iter);
			for (MincExpr* expr: getListExprExprs(exprs))
			{
				iter.set(expr);
				runExpr((MincExpr*)body, parentBlock);
			}
		}
	);

	defineExpr(pkgScope, "$E<PawsSym>.type",
		+[](MincSymbol var) -> MincObject* {
			return var.type;
		}
	);

	defineExpr9(pkgScope, "realpath($E<PawsString>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const std::string& path = ((PawsString*)runExpr(params[0], parentBlock).value)->get();
			char* realPath = realpath(path.c_str(), nullptr);
			if (realPath == nullptr)
				raiseCompileError((path + ": No such file or directory").c_str(), params[0]);
			PawsString* realPathStr = new PawsString(realPath);
			free(realPath);
			return MincSymbol(PawsString::TYPE, realPathStr);
		}, PawsString::TYPE
	);

	// Define MINC package manager import with target scope
	defineExpr9(pkgScope, "$E<PawsBlockExpr>.import($I. ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincBlockExpr* block = ((PawsBlockExpr*)runExpr(params[0], parentBlock).value)->get();
			MincPackageManager* pkgMgr = (MincPackageManager*)exprArgs;
			std::vector<MincExpr*>& pkgPath = getListExprExprs((MincListExpr*)params[1]);
			std::string pkgName = getIdExprName((MincIdExpr*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + '.' + getIdExprName((MincIdExpr*)pkgPath[i]);

			// Import package
			if (!pkgMgr->tryImportPackage(block, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
			return MincSymbol(PawsVoid::TYPE, nullptr);
		}, PawsVoid::TYPE, &MINC_PACKAGE_MANAGER()
	);

	// Define address-of expression
	defineExpr10(pkgScope, "& $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0] = getDerivedExpr(params[0]), parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol value = runExpr(params[0], parentBlock);
			MincObject* ptr = new PawsValue<uint8_t*>(&((PawsValue<uint8_t>*)value.value)->get());
			return MincSymbol(((PawsType*)value.type)->ptrType, ptr);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return ((PawsType*)getType(getDerivedExpr(params[0]), parentBlock))->ptrType;
		}
	);
});