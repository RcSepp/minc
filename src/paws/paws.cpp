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

MincScopeType* FILE_SCOPE_TYPE = new MincScopeType();
MincBlockExpr* pawsScope = nullptr;

const std::string PawsBase::toString() const
{
	static const char* HEX_DIGITS = "0123456789abcdef";
	static const size_t POINTER_SIZE = 2 * sizeof(void*);
	uint64_t ptr = (uint64_t)this;
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
std::set<PawsTpltType> PawsTpltType::tpltTypes;
PawsTpltType* PawsTpltType::get(MincBlockExpr* scope, PawsValue<_Type*>* baseType, PawsValue<_Type*>* tpltType)
{
	std::unique_lock<std::mutex> lock(mutex);
	std::set<PawsTpltType>::iterator iter = tpltTypes.find(PawsTpltType(baseType, tpltType));
	if (iter == tpltTypes.end())
	{
		iter = tpltTypes.insert(PawsTpltType(baseType, tpltType)).first;
		PawsTpltType* t = const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
		t->name = baseType->name + '<' + tpltType->name + '>';
		defineSymbol(scope, t->name.c_str(), PawsValue<_Type*>::TYPE, t);
		defineOpaqueInheritanceCast(scope, t, PawsBase::TYPE); // Let baseType<tpltType> derive from PawsBase
		defineOpaqueInheritanceCast(scope, t, baseType); // Let baseType<tpltType> derive from baseType
	}
	return const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
}
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs)
{
	return lhs.baseType < rhs.baseType
		|| (lhs.baseType == rhs.baseType && lhs.tpltType < rhs.tpltType);
}

void definePawsReturnStmt(MincBlockExpr* scope, const MincObject* returnType, const char* funcName)
{
	if (returnType == PawsVoid::TYPE)
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("void " + std::string(funcName) + " should not return a value").c_str(), params[0]);
			},
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
		defineStmt2(scope, "return $E",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				MincObject* returnType = getType(params[0], parentBlock);
				if (returnType == getErrorType()) // If type is incorrect because expressions has errors
					codegenExpr(params[0], parentBlock); // Raise expression error instead of invalid return type error
				raiseCompileError(("invalid return type `" + lookupSymbolName2(parentBlock, returnType, "UNKNOWN_TYPE") + "`").c_str(), params[0]);
			}
		);

		// Define return statement with correct type in function scope
		defineStmt2(scope, ("return $E<" + lookupSymbolName2(scope, returnType, "UNKNOWN_TYPE") + ">").c_str(),
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				throw ReturnException(codegenExpr(params[0], parentBlock));
			}
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				raiseCompileError(("non-void " + std::string(funcName) + " should return a value").c_str(), (MincExpr*)parentBlock);
			},
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

PawsKernel::PawsKernel(MincBlockExpr* expr, MincObject* type, const std::vector<MincSymbol>& blockParams)
	: expr(expr), type(type), blockParams(blockParams) {}

MincSymbol PawsKernel::codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
{
	MincBlockExpr* instance = cloneBlockExpr(expr);

	// Set block parameters
	for (size_t i = 0; i < params.size(); ++i)
		blockParams[i].value = new PawsExpr(params[i]);
	setBlockExprParams(instance, blockParams);

	defineSymbol(instance, "parentBlock", PawsBlockExpr::TYPE, new PawsBlockExpr(parentBlock));

	// Execute expression code block
	try
	{
		codegenExpr((MincExpr*)instance, expr);
	}
	catch (ReturnException err)
	{
		resetBlockExpr(instance);
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

const std::string PawsType::toString() const
{
	return name;
}

template<> const std::string PawsDouble::toString() const
{
	return std::to_string(val);
}

template<> const std::string PawsValue<const MincExpr*>::toString() const
{
	char* cstr = ExprToString(val);
	std::string str(cstr);
	delete[] cstr;
	return str;
}

bool serializePawsValue(const MincBlockExpr* scope, const MincSymbol& value, std::string* valueStr)
{
	if (isInstance(scope, value.type, PawsBase::TYPE))
	{
		*valueStr = value.value == nullptr ? "NULL" : ((PawsBase*)value.value)->toString();
		return true;
	}
	else
		return false;
}

MincPackage PAWS("paws", [](MincBlockExpr* pkgScope) {
	pawsScope = pkgScope;
	registerValueSerializer(serializePawsValue);
	registerType<PawsBase>(pkgScope, "PawsBase");
	registerType<PawsVoid>(pkgScope, "PawsVoid");
	registerType<PawsType>(pkgScope, "PawsType");
	registerType<PawsInt>(pkgScope, "PawsInt");
	registerType<PawsDouble>(pkgScope, "PawsDouble");
	registerType<PawsString>(pkgScope, "PawsString");
	registerType<PawsExpr>(pkgScope, "PawsExpr");
	registerType<PawsBlockExpr>(pkgScope, "PawsBlockExpr");
	registerType<PawsConstBlockExprList>(pkgScope, "PawsConstBlockExprList");
	registerType<PawsListExpr>(pkgScope, "PawsListExpr");
	registerType<PawsLiteralExpr>(pkgScope, "PawsLiteralExpr");
	registerType<PawsIdExpr>(pkgScope, "PawsIdExpr");
	registerType<PawsSym>(pkgScope, "PawsSym");
	registerType<PawsScopeType>(pkgScope, "PawsScopeType");
	registerType<PawsStringMap>(pkgScope, "PawsStringMap");

	// Import builtin paws packages
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.int");
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
			return MincSymbol(PawsBlockExpr::TYPE, new PawsBlockExpr(parentBlock));
		},
		PawsBlockExpr::TYPE
	);

	defineSymbol(pkgScope, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	defineStmt2(pkgScope, "$E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define single-expr statement
	defineStmt2(pkgScope, "$E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define context-free pkgScope statement
	defineStmt2(pkgScope, "$B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define general bracketed expression
	defineExpr3(pkgScope, "($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return codegenExpr(params[0], parentBlock);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return getType(params[0], parentBlock);
		}
	);

	// Define empty statement
	defineStmt2(pkgScope, "", [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {});

	// Define return statement
	definePawsReturnStmt(pkgScope, PawsInt::TYPE);

	// Overwrite return statement with correct type in function scope to call quit() instead of raising ReturnException
	defineStmt(pkgScope, "return $E<PawsInt>",
		+[](int returnCode) {
			quit(returnCode);
		} // LCOV_EXCL_LINE
	);

	// Define variable lookup
	defineExpr3(pkgScope, "$I",
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
	defineExpr3(pkgScope, "$L",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				return MincSymbol(PawsString::TYPE, new PawsString(std::string(valueStart, valueEnd - valueStart)));
			}

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return MincSymbol(PawsDouble::TYPE, new PawsDouble(doubleValue));
			}
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return MincSymbol(PawsInt::TYPE, new PawsInt(intValue));
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;
			if (*valueEnd == '"' || *valueEnd == '\'')
				return PawsString::TYPE;
			if (strchr(value, '.'))
				return PawsDouble::TYPE;
			return PawsInt::TYPE;
		}
	);

	// Define variable assignment
	defineExpr3(pkgScope, "$I<PawsBase> = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* valueExpr = params[1];
			if (ExprIsCast(valueExpr))
				valueExpr = getCastExprSource((MincCastExpr*)valueExpr);
			MincSymbol value = codegenExpr(valueExpr, parentBlock);

			MincExpr* varExpr = params[0];
			if (ExprIsCast(varExpr))
				varExpr = getCastExprSource((MincCastExpr*)varExpr);
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)varExpr));
			if (var == nullptr)
				defineSymbol(parentBlock, getIdExprName((MincIdExpr*)varExpr), value.type, ((PawsBase*)value.value)->copy());
			else
			{
				var->value = ((PawsBase*)value.value)->copy();
				var->type = value.type;
			}
			return value;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* valueExpr = params[1];
			if (ExprIsCast(valueExpr))
				valueExpr = getCastExprSource((MincCastExpr*)valueExpr);
			return getType(valueExpr, parentBlock);
		}
	);
defineSymbol(pkgScope, "_NULL", nullptr, nullptr); //TODO: Use one `NULL` for both paws and builtin.cpp
	defineExpr3(pkgScope, "$I<_NULL> = $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = params[1];
			if (ExprIsCast(expr))
				expr = getCastExprSource((MincCastExpr*)expr);
			MincSymbol value = codegenExpr(expr, parentBlock);

			defineSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]), value.type, ((PawsBase*)value.value)->copy());
			return value;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* expr = params[1];
			if (ExprIsCast(expr))
				expr = getCastExprSource((MincCastExpr*)expr);
			return getType(expr, parentBlock);
		}
	);

	// Define is-NULL
	defineExpr2(pkgScope, "$E == NULL",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) == nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
		},
		PawsInt::TYPE
	);
	defineExpr2(pkgScope, "$E != NULL",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) != nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
		},
		PawsInt::TYPE
	);

	defineExpr(pkgScope, "$E<PawsType> == $E<PawsType>",
		+[](PawsType* a, PawsType* b) -> int {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsType> != $E<PawsType>",
		+[](PawsType* a, PawsType* b) -> int {
			return a != b;
		}
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
	defineStmt2(pkgScope, "if($E<PawsInt>) $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->get())
				codegenExpr(params[1], parentBlock);
		}
	);

	// Define if/else statement
	defineStmt2(pkgScope, "if($E<PawsInt>) $S else $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->get())
				codegenExpr(params[1], parentBlock);
			else
				codegenExpr(params[2], parentBlock);
		}
	);

	// Define inline if expression
	defineExpr3(pkgScope, "$E<PawsInt> ? $E : $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			if (ta != tb)
				throw CompileError(parentBlock, getLocation(params[0]), "operands to ?: have different types <%T> and <%T>", params[1], params[2]);

			return codegenExpr(params[((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() ? 1 : 2], parentBlock);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			return ta == tb ? ta : getErrorType();
		}
	);

	// Define while statement
	defineStmt2(pkgScope, "while($E<PawsInt>) $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			size_t cs = getBlockExprCacheState(parentBlock);
			while (((PawsInt*)codegenExpr(params[0], parentBlock).value)->get())
			{
				codegenExpr(params[1], parentBlock);
				resetBlockExprCache(parentBlock, cs); // Reset result cache to the state before the while loop to avoid rerunning
														 // previous loop iterations when resuming a coroutine within the loop block
			}
		}
	);

	// Define for statement
	defineStmt2(pkgScope, "for($E; $D; $D) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincBlockExpr* forBlock = (MincBlockExpr*)params[3];

			// Inherent global scope into loop block scope
			setBlockExprParent(forBlock, parentBlock);

			// Codegen init expression in loop block scope
			codegenExpr(params[0], forBlock);

			// Reresolve condition and update expressions to take loop variable into account
			resolveExpr(params[1], forBlock);
			resolveExpr(params[2], forBlock);

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
			}

			while (((PawsInt*)codegenExpr(condExpr, forBlock).value)->get()) // Codegen condition expression in loop block scope
			{
				// Codegen loop block in parent scope
				codegenExpr((MincExpr*)forBlock, parentBlock);

				// Codegen update expression in loop block scope
				codegenExpr(params[2], forBlock);
			}
		}
	);

	defineExpr2(pkgScope, "str($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* valueExpr = params[0];
			if (ExprIsCast(valueExpr))
				valueExpr = getCastExprSource((MincCastExpr*)valueExpr);
			PawsBase* value = (PawsBase*)codegenExpr(valueExpr, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				return MincSymbol(PawsString::TYPE, new PawsString(strValue->get()));
			else
				return MincSymbol(PawsString::TYPE, new PawsString(value->toString()));
		},
		PawsString::TYPE
	);

	defineExpr(pkgScope, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);
	defineExpr2(pkgScope, "print($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* valueExpr = params[0];
			PawsBase* value = (PawsBase*)codegenExpr(valueExpr, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				std::cout << strValue->get() << '\n';
			else if (value != nullptr)
				std::cout << value->toString() << '\n';
			else
				std::cout << "NULL\n";

			return MincSymbol(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr(pkgScope, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);
	defineExpr2(pkgScope, "printerr($E<PawsType>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* valueExpr = params[0];
			if (ExprIsCast(valueExpr))
				valueExpr = getCastExprSource((MincCastExpr*)valueExpr);
			PawsBase* value = (PawsBase*)codegenExpr(valueExpr, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				std::cerr << strValue->get() << '\n';
			else
				std::cerr << value->toString() << '\n';

			return MincSymbol(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(pkgScope, "type($E<PawsBase>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = params[0];
			if (ExprIsCast(expr))
				expr = getCastExprSource((MincCastExpr*)expr);
			MincObject* type = getType(expr, parentBlock);
			if (type == getErrorType())// If type is error-type because expressions has errors
				codegenExpr(expr, parentBlock); // Raise expression error
			return MincSymbol(PawsType::TYPE, type);
		},
		PawsType::TYPE
	);

	defineExpr(pkgScope, "parseCFile($E<PawsString>)",
		+[](std::string filename) -> MincBlockExpr* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parseCFile(fname);
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

	defineExpr2(pkgScope, "PawsExpr<$E<PawsType>>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
			return MincSymbol(PawsType::TYPE, PawsTpltType::get(parentBlock, PawsExpr::TYPE, returnType));
		},
		PawsType::TYPE
	);
	defineExpr2(pkgScope, "PawsConstExpr<$E<PawsType>>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
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
	defineExpr2(pkgScope, "$E<PawsConstExpr>.getType($E<PawsBlockExpr>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = ((PawsBlockExpr*)codegenExpr(params[1], parentBlock).value)->get();
			return MincSymbol(PawsType::TYPE, getType(expr, scope));
		},
		PawsType::TYPE
	);
	defineExpr2(pkgScope, "$E<PawsConstExpr>.getType()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = getBlockExprParent(parentBlock);
			return MincSymbol(PawsType::TYPE, getType(expr, scope));
		},
		PawsType::TYPE
	);

	// Define codegen
	defineExpr3(pkgScope, "$E<PawsExpr>.codegen($E<PawsBlockExpr>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = ((PawsBlockExpr*)codegenExpr(params[1], parentBlock).value)->get();
			if (!ExprIsCast(params[0]))
			{
				codegenExpr(expr, scope);
				return MincSymbol(PawsVoid::TYPE, nullptr);
			}
			return codegenExpr(expr, scope);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return PawsVoid::TYPE;
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
	defineExpr3(pkgScope, "$E<PawsExpr>.codegen()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincExpr* expr = ((PawsExpr*)codegenExpr(params[0], parentBlock).value)->get();
			MincBlockExpr* scope = getBlockExprParent(parentBlock);
			if (!ExprIsCast(params[0]))
			{
				codegenExpr(expr, scope);
				return MincSymbol(PawsVoid::TYPE, nullptr);
			}
			return codegenExpr(expr, scope);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			if (!ExprIsCast(params[0]))
				return PawsVoid::TYPE;
			PawsType* type = (PawsType*)getType(getDerivedExpr(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.codegen($E<PawsBlockExpr>)",
		+[](MincExpr* expr, MincBlockExpr* scope) -> void {
			codegenExpr(expr, scope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExpr>.codegen(NULL)",
		+[](MincBlockExpr* pkgScope) -> void {
			codegenExpr((MincExpr*)pkgScope, nullptr);
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

	defineStmt2(pkgScope, "for ($I: $E<PawsConstBlockExprList>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			MincSymbol exprsVar = codegenExpr(params[1], parentBlock);
			const std::vector<MincBlockExpr*>& exprs = ((PawsConstBlockExprList*)exprsVar.value)->get();
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsBlockExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), PawsBlockExpr::TYPE, &iter);
			for (MincBlockExpr* expr: exprs)
			{
				iter.set(expr);
				codegenExpr((MincExpr*)body, parentBlock);
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

	defineExpr3(pkgScope, "$E<PawsListExpr>[$E<PawsInt>]",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			assert(ExprIsCast(params[0]));
			MincSymbol exprsVar = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			MincListExpr* exprs = ((PawsListExpr*)exprsVar.value)->get();
			int idx = ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get();
			return MincSymbol(((PawsTpltType*)exprsVar.type)->tpltType, new PawsExpr(getListExprExprs(exprs)[idx]));
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
		}
	);

	defineStmt2(pkgScope, "for ($I: $E<PawsListExpr>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			assert(ExprIsCast(params[1]));
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			MincSymbol exprsVar = codegenExpr(getCastExprSource((MincCastExpr*)params[1]), parentBlock);
			MincListExpr* exprs = ((PawsListExpr*)exprsVar.value)->get();
			PawsType* exprType = ((PawsTpltType*)exprsVar.type)->tpltType;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsExpr iter;
			defineSymbol(body, getIdExprName(iterExpr), exprType, &iter);
			for (MincExpr* expr: getListExprExprs(exprs))
			{
				iter.set(expr);
				codegenExpr((MincExpr*)body, parentBlock);
			}
		}
	);

	defineExpr(pkgScope, "$E<PawsSym>.type",
		+[](MincSymbol var) -> MincObject* {
			return var.type;
		}
	);

	defineExpr2(pkgScope, "realpath($E<PawsString>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const std::string& path = ((PawsString*)codegenExpr(params[0], parentBlock).value)->get();
			char* realPath = realpath(path.c_str(), nullptr);
			if (realPath == nullptr)
				raiseCompileError((path + ": No such file or directory").c_str(), params[0]);
			PawsString* realPathStr = new PawsString(realPath);
			free(realPath);
			return MincSymbol(PawsString::TYPE, realPathStr);
		}, PawsString::TYPE
	);

	// Define MINC package manager import with target scope
	defineExpr2(pkgScope, "$E<PawsBlockExpr>.import($I. ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincBlockExpr* block = ((PawsBlockExpr*)codegenExpr(params[0], parentBlock).value)->get();
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
	defineExpr3(pkgScope, "& $E<PawsBase>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol value = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			MincObject* ptr = new PawsValue<uint8_t*>(&((PawsValue<uint8_t>*)value.value)->get());
			return MincSymbol(((PawsType*)value.type)->ptrType, ptr);
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			return ((PawsType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->ptrType;
		}
	);
});