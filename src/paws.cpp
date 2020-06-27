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

BaseScopeType* FILE_SCOPE_TYPE = new BaseScopeType();

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
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs)
{
	return lhs.baseType < rhs.baseType
		|| (lhs.baseType == rhs.baseType && lhs.tpltType < rhs.tpltType);
}

void definePawsReturnStmt(BlockExprAST* scope, const MincObject* returnType, const char* funcName)
{
	if (returnType == PawsVoid::TYPE)
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				if (funcName)
					raiseCompileError(("void function '" + std::string(funcName) + "' should not return a value").c_str(), params[0]);
				else
					raiseCompileError("void function should not return a value", params[0]);
			},
			(void*)funcName
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				throw ReturnException(Variable(PawsVoid::TYPE, nullptr));
			}
		);
	}
	else
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				MincObject* returnType = getType(params[0], parentBlock);
				raiseCompileError(("invalid return type `" + getTypeName(returnType) + "`").c_str(), params[0]);
			}
		);

		// Define return statement with correct type in function scope
		defineStmt2(scope, ("return $E<" + getTypeName(returnType) + ">").c_str(),
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				throw ReturnException(codegenExpr(params[0], parentBlock));
			}
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				if (funcName)
					raiseCompileError(("non-void function '" + std::string(funcName) + "' should return a value").c_str(), (ExprAST*)parentBlock);
				else
					raiseCompileError("non-void function should return a value", (ExprAST*)parentBlock);
			},
			(void*)funcName
		);
	}
}

void getBlockParameterTypes(BlockExprAST* scope, const std::vector<ExprAST*> params, std::vector<Variable>& blockParams)
{
	blockParams.reserve(params.size());
	for (ExprAST* param: params)
	{
		PawsType* paramType = PawsExprAST::TYPE;
		if (ExprASTIsPlchld(param))
		{
			PlchldExprAST* plchldParam = (PlchldExprAST*)param;
			switch (getPlchldExprASTLabel(plchldParam))
			{
			default: assert(0); //TODO: Throw exception
			case 'L': paramType = PawsLiteralExprAST::TYPE; break;
			case 'I': paramType = PawsIdExprAST::TYPE; break;
			case 'B': paramType = PawsBlockExprAST::TYPE; break;
			case 'S': break;
			case 'E':
				if (getPlchldExprASTSublabel(plchldParam) == nullptr)
					break;
				if (const Variable* var = importSymbol(scope, getPlchldExprASTSublabel(plchldParam)))
					paramType = PawsTpltType::get(PawsExprAST::TYPE, (PawsType*)var->value);
			}
		}
		else if (ExprASTIsList(param))
		{
			const std::vector<ExprAST*>& listParamExprs = getListExprASTExprs((ListExprAST*)param);
			if (listParamExprs.size() != 0)
			{
				PlchldExprAST* plchldParam = (PlchldExprAST*)listParamExprs.front();
				switch (getPlchldExprASTLabel(plchldParam))
				{
				default: assert(0); //TODO: Throw exception
				case 'L': paramType = PawsLiteralExprAST::TYPE; break;
				case 'I': paramType = PawsIdExprAST::TYPE; break;
				case 'B': paramType = PawsBlockExprAST::TYPE; break;
				case 'S': break;
				case 'E':
					if (getPlchldExprASTSublabel(plchldParam) == nullptr)
						break;
					if (const Variable* var = importSymbol(scope, getPlchldExprASTSublabel(plchldParam)))
						paramType = PawsTpltType::get(PawsExprAST::TYPE, (PawsType*)var->value);
				}
				paramType = PawsTpltType::get(PawsListExprAST::TYPE, paramType);
			}
		}
		blockParams.push_back(Variable(paramType, nullptr));
	}
}

PawsCodegenContext::PawsCodegenContext(BlockExprAST* expr, MincObject* type, const std::vector<Variable>& blockParams)
	: expr(expr), type(type), blockParams(blockParams) {}

Variable PawsCodegenContext::codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
{
	BlockExprAST* instance = cloneBlockExprAST(expr);

	// Set block parameters
	for (size_t i = 0; i < params.size(); ++i)
		blockParams[i].value = new PawsExprAST(params[i]);
	setBlockExprASTParams(instance, blockParams);

	defineSymbol(instance, "parentBlock", PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));

	// Execute expression code block
	try
	{
		codegenExpr((ExprAST*)instance, expr);
	}
	catch (ReturnException err)
	{
		resetBlockExprAST(instance);
		return err.result;
	}

	if (type != getVoid().type && type != PawsVoid::TYPE)
		raiseCompileError("missing return statement in expression block", (ExprAST*)instance);
	return getVoid();
}

MincObject* PawsCodegenContext::getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
{
	return type;
}

void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)())
{
	using StmtFunc = void (*)();
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		(*(StmtFunc*)stmtArgs)();
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(), PawsType* (*exprTypeFunc)())
{
	using ExprFunc = Variable (*)();
	using ExprTypeFunc = PawsType* (*)();
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first();
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second();
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

const std::string PawsType::toString() const
{
	return getTypeName(this);
}

template<> const std::string PawsDouble::toString() const
{
	return std::to_string(val);
}

template<> const std::string PawsValue<const ExprAST*>::toString() const
{
	char* cstr = ExprASTToString(val);
	std::string str(cstr);
	delete[] cstr;
	return str;
}

MincPackage PAWS("paws", [](BlockExprAST* pkgScope) {
	registerValueSerializer([pkgScope](const Variable& value, std::string* valueStr) -> bool {
		if (isInstance(pkgScope, value.type, PawsBase::TYPE))
		{
			*valueStr = ((PawsBase*)value.value)->toString();
			return true;
		}
		else
			return false;
	});
	registerType<PawsBase>(pkgScope, "PawsBase");
	registerType<PawsVoid>(pkgScope, "PawsVoid");
	registerType<PawsType>(pkgScope, "PawsType");
	registerType<PawsInt>(pkgScope, "PawsInt");
	registerType<PawsDouble>(pkgScope, "PawsDouble");
	registerType<PawsString>(pkgScope, "PawsString");
	registerType<PawsExprAST>(pkgScope, "PawsExprAST");
	registerType<PawsBlockExprAST>(pkgScope, "PawsBlockExprAST");
	registerType<PawsConstBlockExprASTList>(pkgScope, "PawsConstBlockExprASTList");
	registerType<PawsListExprAST>(pkgScope, "PawsListExprAST");
	registerType<PawsLiteralExprAST>(pkgScope, "PawsLiteralExprAST");
	registerType<PawsIdExprAST>(pkgScope, "PawsIdExprAST");
	registerType<PawsVariable>(pkgScope, "PawsVariable");
	registerType<PawsScopeType>(pkgScope, "PawsScopeType");
	registerType<PawsStringMap>(pkgScope, "PawsStringMap");

	// Import builtin paws packages
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.int");
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.string");

	int argc;
	char** argv;
	getCommandLineArgs(&argc, &argv);
	std::vector<Variable> blockParams;
	blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		blockParams.push_back(Variable(PawsString::TYPE, new PawsString(std::string(argv[i]))));
	setBlockExprASTParams(pkgScope, blockParams);

	defineExpr2(pkgScope, "getFileScope()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));
		},
		PawsBlockExprAST::TYPE
	);

	defineSymbol(pkgScope, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	defineStmt2(pkgScope, "$E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define single-expr statement
	defineStmt2(pkgScope, "$E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define context-free pkgScope statement
	defineStmt2(pkgScope, "$B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define general bracketed expression
	defineExpr3(pkgScope, "($E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[0], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			return getType(params[0], parentBlock);
		}
	);

	// Define empty statement
	defineStmt2(pkgScope, "", [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {});

	// Define return statement
	definePawsReturnStmt(pkgScope, PawsInt::TYPE);

	// Overwrite return statement with correct type in function scope to call quit() instead of raising ReturnException
	defineStmt(pkgScope, "return $E<PawsInt>",
		+[](int returnCode) {
			quit(returnCode);
		}
	);

	// Define variable lookup
	defineExpr3(pkgScope, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *var;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(pkgScope, "$L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				return Variable(PawsString::TYPE, new PawsString(std::string(valueStart, valueEnd - valueStart)));
			}

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return Variable(PawsDouble::TYPE, new PawsDouble(doubleValue));
			}
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return Variable(PawsInt::TYPE, new PawsInt(intValue));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
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
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			Variable expr = codegenExpr(exprAST, parentBlock);

			ExprAST* varAST = params[0];
			if (ExprASTIsCast(varAST))
				varAST = getCastExprASTSource((CastExprAST*)varAST);
			Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)varAST));
			if (var == nullptr)
				defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)varAST), expr.type, ((PawsBase*)expr.value)->copy());
			else
			{
				var->value = ((PawsBase*)expr.value)->copy();
				var->type = expr.type;
			}
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return getType(exprAST, parentBlock);
		}
	);
defineSymbol(pkgScope, "_NULL", nullptr, nullptr); //TODO: Use one `NULL` for both paws and builtin.cpp
	defineExpr3(pkgScope, "$I<_NULL> = $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			Variable expr = codegenExpr(exprAST, parentBlock);

			defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), expr.type, ((PawsBase*)expr.value)->copy());
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return getType(exprAST, parentBlock);
		}
	);

	// Define is-NULL
	defineExpr2(pkgScope, "$E == NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) == nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
		},
		PawsInt::TYPE
	);
	defineExpr2(pkgScope, "$E != NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) != nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
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
	//TODO: Generalize this beyond PawsConstExprAST
	defineExpr(pkgScope, "$E<PawsConstExprAST> == NULL",
		+[](const ExprAST* a) -> int {
			return a == nullptr;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST> != NULL",
		+[](const ExprAST* a) -> int {
			return a != nullptr;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST> == $E<PawsConstExprAST>",
		+[](const ExprAST* a, const ExprAST* b) -> int {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST> != $E<PawsConstExprAST>",
		+[](const ExprAST* a, const ExprAST* b) -> int {
			return a != b;
		}
	);

	// Define if statement
	defineStmt2(pkgScope, "if($E<PawsInt>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->get())
				codegenExpr(params[1], parentBlock);
		}
	);

	// Define if/else statement
	defineStmt2(pkgScope, "if($E<PawsInt>) $S else $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->get())
				codegenExpr(params[1], parentBlock);
			else
				codegenExpr(params[2], parentBlock);
		}
	);

	// Define inline if expression
	defineExpr3(pkgScope, "$E<PawsInt> ? $E : $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() ? 1 : 2], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			MincObject* ta = getType(params[1], parentBlock);
			MincObject* tb = getType(params[2], parentBlock);
			if (ta != tb)
				raiseCompileError("TODO", params[0]);
			return ta;
		}
	);

	// Define while statement
	defineStmt2(pkgScope, "while($E<PawsInt>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			size_t cs = getBlockExprASTCacheState(parentBlock);
			while (((PawsInt*)codegenExpr(params[0], parentBlock).value)->get())
			{
				codegenExpr(params[1], parentBlock);
				resetBlockExprASTCache(parentBlock, cs); // Reset result cache to the state before the while loop to avoid rerunning
														 // previous loop iterations when resuming a coroutine within the loop block
			}
		}
	);

	// Define for statement
	defineStmt2(pkgScope, "for($E; $E; $E) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BlockExprAST* forBlock = (BlockExprAST*)params[3];

			// Inherent global scope into loop block scope
			setBlockExprASTParent(forBlock, parentBlock);

			// Codegen init expression in loop block scope
			codegenExpr(params[0], forBlock);

			// Reresolve condition and update expressions to take loop variable into account
			resolveExprAST(forBlock, params[1]);
			resolveExprAST(forBlock, params[2]);

			// Cast condition expression to PawsInt
			ExprAST* condExpr = params[1];
			MincObject* condType = getType(condExpr, forBlock);
			if (condType != PawsInt::TYPE)
			{
				condExpr = lookupCast(parentBlock, condExpr, PawsInt::TYPE);
				if (condExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, params[1]);
					throw CompileError(
						getLocation(params[1]), "invalid for condition type: %E<%t>, expected: <%t>\n%S",
						params[1], condType, PawsInt::TYPE, candidateReport
					);
				}
			}

			while (((PawsInt*)codegenExpr(condExpr, forBlock).value)->get()) // Codegen condition expression in loop block scope
			{
				// Codegen loop block in parent scope
				codegenExpr((ExprAST*)forBlock, parentBlock);

				// Codegen update expression in loop block scope
				codegenExpr(params[2], forBlock);
			}
		}
	);

	defineExpr2(pkgScope, "str($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			PawsBase* value = (PawsBase*)codegenExpr(exprAST, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				return Variable(PawsString::TYPE, new PawsString(strValue->get()));
			else
				return Variable(PawsString::TYPE, new PawsString(value->toString()));
		},
		PawsString::TYPE
	);

	defineExpr(pkgScope, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);
	defineExpr2(pkgScope, "print($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			PawsBase* value = (PawsBase*)codegenExpr(exprAST, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				std::cout << strValue->get() << '\n';
			else
				std::cout << value->toString() << '\n';

			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr(pkgScope, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);
	defineExpr2(pkgScope, "printerr($E<PawsType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			PawsBase* value = (PawsBase*)codegenExpr(exprAST, parentBlock).value;

			// Do not use PawsString::toString(), because it surrounds the value string with quotes
			PawsString* strValue;
			if ((strValue = dynamic_cast<PawsString*>(value)) != nullptr)
				std::cerr << strValue->get() << '\n';
			else
				std::cerr << value->toString() << '\n';

			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);

	defineExpr2(pkgScope, "type($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PawsType::TYPE, getType(exprAST, parentBlock));
		},
		PawsType::TYPE
	);

	defineStmt2(pkgScope, "assert $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			int test = ((PawsInt*)codegenExpr(params[0], parentBlock).value)->get();
			if (!test)
				raiseCompileError("Assertion failed", params[0]);
		}
	);

	defineExpr(pkgScope, "parseCFile($E<PawsString>)",
		+[](std::string filename) -> BlockExprAST* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parseCFile(fname);
		}
	);

	defineExpr(pkgScope, "parsePythonFile($E<PawsString>)",
		+[](std::string filename) -> BlockExprAST* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parsePythonFile(fname);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstExprAST>.filename",
		+[](const ExprAST* expr) -> std::string {
			return getExprFilename(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST>.line",
		+[](const ExprAST* expr) -> int {
			return getExprLine(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST>.column",
		+[](const ExprAST* expr) -> int {
			return getExprColumn(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST>.endLine",
		+[](const ExprAST* expr) -> int {
			return getExprEndLine(expr);
		}
	);
	defineExpr(pkgScope, "$E<PawsConstExprAST>.endColumn",
		+[](const ExprAST* expr) -> int {
			return getExprEndColumn(expr);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprAST>.parent",
		+[](const BlockExprAST* pkgScope) -> BlockExprAST* {
			return getBlockExprASTParent(pkgScope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExprAST>.parent = $E<PawsBlockExprAST>",
		+[](BlockExprAST* pkgScope, BlockExprAST* parent) -> void {
			setBlockExprASTParent(pkgScope, parent);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprAST>.references",
		+[](const BlockExprAST* pkgScope) -> const std::vector<BlockExprAST*>& {
			return getBlockExprASTReferences(pkgScope);
		}
	);

	// Define codegen
	defineExpr3(pkgScope, "$E<PawsExprAST>.codegen($E<PawsBlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* expr = ((PawsExprAST*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* scope = ((PawsBlockExprAST*)codegenExpr(params[1], parentBlock).value)->get();
			if (!ExprASTIsCast(params[0]))
			{
				codegenExpr(expr, scope);
				return Variable(PawsVoid::TYPE, nullptr);
			}
			return codegenExpr(expr, scope);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			if (!ExprASTIsCast(params[0]))
				return PawsVoid::TYPE;//raiseCompileError("can't infer codegen type from non-templated ExprAST", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
	defineExpr3(pkgScope, "$E<PawsExprAST>.codegen()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* expr = ((PawsExprAST*)codegenExpr(params[0], parentBlock).value)->get();
			BlockExprAST* scope = getBlockExprASTParent(parentBlock);
			if (!ExprASTIsCast(params[0]))
			{
				codegenExpr(expr, scope);
				return Variable(PawsVoid::TYPE, nullptr);
			}
			return codegenExpr(expr, scope);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			if (!ExprASTIsCast(params[0]))
				return PawsVoid::TYPE;//raiseCompileError("can't infer codegen type from non-templated ExprAST", params[0]);
			PawsType* type = (PawsType*)getType(getDerivedExprAST(params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExprAST>.codegen($E<PawsBlockExprAST>)",
		+[](ExprAST* expr, BlockExprAST* scope) -> void {
			codegenExpr(expr, scope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExprAST>.codegen(NULL)",
		+[](BlockExprAST* pkgScope) -> void {
			codegenExpr((ExprAST*)pkgScope, nullptr);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.import($E<PawsBlockExprAST>)",
		+[](BlockExprAST* scope, BlockExprAST* pkgScope) -> void {
			importBlock(scope, pkgScope);
		}
	);

	defineExpr(pkgScope, "$E<PawsBlockExprAST>.scopeType",
		+[](BlockExprAST* scope) -> BaseScopeType* {
			return getScopeType(scope);
		}
	);
	defineExpr(pkgScope, "$E<PawsBlockExprAST>.scopeType = $E<PawsScopeType>",
		+[](BlockExprAST* scope, BaseScopeType* scopeType) -> BaseScopeType* {
			setScopeType(scope, scopeType);
			return scopeType;
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprASTList>[$E<PawsInt>]",
		+[](const std::vector<BlockExprAST*>& blocks, int idx) -> BlockExprAST* {
			return blocks[idx];
		}
	);

	defineExpr(pkgScope, "$E<PawsConstBlockExprASTList>.length",
		+[](const std::vector<BlockExprAST*>& blocks) -> int {
			return blocks.size();
		}
	);

	defineStmt2(pkgScope, "for ($I: $E<PawsConstBlockExprASTList>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* iterExpr = (IdExprAST*)params[0];
			Variable exprsVar = codegenExpr(params[1], parentBlock);
			const std::vector<BlockExprAST*>& exprs = ((PawsConstBlockExprASTList*)exprsVar.value)->get();
			BlockExprAST* body = (BlockExprAST*)params[2];
			PawsBlockExprAST iter;
			defineSymbol(body, getIdExprASTName(iterExpr), PawsBlockExprAST::TYPE, &iter);
			for (BlockExprAST* expr: exprs)
			{
				iter.set(expr);
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	defineExpr(pkgScope, "$E<PawsConstLiteralExprAST>.value",
		+[](const LiteralExprAST* expr) -> std::string {
			return getLiteralExprASTValue(expr);
		}
	);

	defineExpr(pkgScope, "$E<PawsConstIdExprAST>.name",
		+[](const IdExprAST* expr) -> std::string {
			return getIdExprASTName(expr);
		}
	);

	defineExpr3(pkgScope, "$E<PawsListExprAST>[$E<PawsInt>]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			assert(ExprASTIsCast(params[0]));
			Variable exprsVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			ListExprAST* exprs = ((PawsListExprAST*)exprsVar.value)->get();
			int idx = ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get();
			return Variable(((PawsTpltType*)exprsVar.type)->tpltType, new PawsExprAST(getListExprASTExprs(exprs)[idx]));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);

	defineStmt2(pkgScope, "for ($I: $E<PawsListExprAST>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			assert(ExprASTIsCast(params[1]));
			IdExprAST* iterExpr = (IdExprAST*)params[0];
			Variable exprsVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[1]), parentBlock);
			ListExprAST* exprs = ((PawsListExprAST*)exprsVar.value)->get();
			PawsType* exprType = ((PawsTpltType*)exprsVar.type)->tpltType;
			BlockExprAST* body = (BlockExprAST*)params[2];
			PawsExprAST iter;
			defineSymbol(body, getIdExprASTName(iterExpr), exprType, &iter);
			for (ExprAST* expr: getListExprASTExprs(exprs))
			{
				iter.set(expr);
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	defineExpr(pkgScope, "$E<PawsVariable>.type",
		+[](Variable var) -> MincObject* {
			return var.type;
		}
	);

	defineExpr2(pkgScope, "realpath($E<PawsString>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const std::string& path = ((PawsString*)codegenExpr(params[0], parentBlock).value)->get();
			char* realPath = realpath(path.c_str(), nullptr);
			if (realPath == nullptr)
				raiseCompileError((path + ": No such file or directory").c_str(), params[0]);
			PawsString* realPathStr = new PawsString(realPath);
			free(realPath);
			return Variable(PawsString::TYPE, realPathStr);
		}, PawsString::TYPE
	);

	// Define MINC package manager import with target scope
	defineExpr2(pkgScope, "$E<PawsBlockExprAST>.import($I. ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			BlockExprAST* block = ((PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value)->get();
			MincPackageManager* pkgMgr = (MincPackageManager*)exprArgs;
			std::vector<ExprAST*>& pkgPath = getListExprASTExprs((ListExprAST*)params[1]);
			std::string pkgName = getIdExprASTName((IdExprAST*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + '.' + getIdExprASTName((IdExprAST*)pkgPath[i]);

			// Import package
			if (!pkgMgr->tryImportPackage(block, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
			return Variable(PawsVoid::TYPE, nullptr);
		}, PawsVoid::TYPE, &MINC_PACKAGE_MANAGER()
	);

	// Define address-of expression
	defineExpr3(pkgScope, "& $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable value = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			MincObject* ptr = new PawsValue<uint8_t*>(&((PawsValue<uint8_t>*)value.value)->get());
			return Variable(((PawsType*)value.type)->ptrType, ptr);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> MincObject* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->ptrType;
		}
	);
});