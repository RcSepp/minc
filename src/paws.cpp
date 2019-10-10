#include <string>
#include <map>
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>
#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

BaseScopeType* FILE_SCOPE_TYPE = new BaseScopeType();

template<> uint64_t PawsType<BaseType*>::getConstantValue() { return (uint64_t)val; }
std::set<PawsTpltType> PawsTpltType::tpltTypes;
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs)
{
	return lhs.baseType < rhs.baseType
		|| lhs.baseType == rhs.baseType && lhs.tpltType < rhs.tpltType;
}

void definePawsReturnStmt(BlockExprAST* scope, const BaseType* returnType, const char* funcName)
{
	if (returnType == PawsVoid::TYPE)
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				BaseType* returnType = getType(params[0], parentBlock);
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
				BaseType* returnType = getType(params[0], parentBlock);
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

struct StmtContext : public CodegenContext
{
private:
	BlockExprAST* const stmt;
	std::vector<Variable> blockParams;
public:
	StmtContext(BlockExprAST* stmt, const std::vector<Variable>& blockParams)
		: stmt(stmt), blockParams(blockParams) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		// Set block parameters
		for (size_t i = 0; i < params.size(); ++i)
			blockParams[i].value = new PawsExprAST(params[i]);
		setBlockExprASTParams(stmt, blockParams);

		defineSymbol(stmt, "parentBlock", PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));

		// Execute statement code block
		codegenExpr((ExprAST*)stmt, parentBlock);

		return getVoid();
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return getVoid().type;
	}
};

struct ExprContext : public CodegenContext
{
private:
	BlockExprAST* const expr;
	BaseType* const type;
	std::vector<Variable> blockParams;
public:
	ExprContext(BlockExprAST* expr, BaseType* type, const std::vector<Variable>& blockParams)
		: expr(expr), type(type), blockParams(blockParams) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		// Set block parameters
		for (size_t i = 0; i < params.size(); ++i)
			blockParams[i].value = new PawsExprAST(params[i]);
		setBlockExprASTParams(expr, blockParams);

		defineSymbol(expr, "parentBlock", PawsBlockExprAST::TYPE, new PawsBlockExprAST(parentBlock));

		// Execute expression code block
		try
		{
			codegenExpr((ExprAST*)expr, parentBlock);
		}
		catch (ReturnException err)
		{
			return err.result;
		}
		raiseCompileError("missing return statement in expression block", (ExprAST*)expr);

		return getVoid();
	}
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const
	{
		return type;
	}
};

void getBlockParameterTypes(BlockExprAST* scope, const std::vector<ExprAST*> params, std::vector<Variable>& blockParams)
{
	blockParams.reserve(params.size());
	for (ExprAST* param: params)
	{
		BaseType* paramType = PawsExprAST::TYPE;
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
					paramType = PawsTpltType::get(PawsExprAST::TYPE, (BaseType*)var->value->getConstantValue());
			}
		}
		else if (ExprASTIsList(param))
		{
			const std::vector<ExprAST*>& listParamExprs = getExprListASTExpressions((ExprListAST*)param);
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
						paramType = PawsTpltType::get(PawsExprAST::TYPE, (BaseType*)var->value->getConstantValue());
				}
				paramType = PawsTpltType::get(PawsExprListAST::TYPE, paramType);
			}
		}
		blockParams.push_back(Variable(paramType, nullptr));
	}
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
void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(), BaseType* (*exprTypeFunc)())
{
	using ExprFunc = Variable (*)();
	using ExprTypeFunc = BaseType* (*)();
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first();
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second();
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

void importFile(BlockExprAST* parentBlock, std::string importPath)
{
	char buf[1024];
	realpath(importPath.c_str(), buf);
	char* importRealPath = new char[strlen(buf) + 1];
	strcpy(importRealPath, buf);
	importPath = importPath.substr(std::max(importPath.rfind("/"), importPath.rfind("\\")) + 1);
	const size_t dt = importPath.rfind(".");
	if (dt != -1) importPath = importPath.substr(0, dt);

	// Parse imported source code //TODO: Implement file caching
	BlockExprAST* importBlock = parseCFile(importRealPath);

	// Codegen imported module
	const int outputDebugSymbols = ((PawsInt*)importSymbol(parentBlock, "outputDebugSymbols")->value)->val;
	IModule* importModule = createModule(importRealPath, importPath + ":main", outputDebugSymbols);
	setScopeType(importBlock, FILE_SCOPE_TYPE);
	defineSymbol(importBlock, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(importBlock));
	codegenExpr((ExprAST*)importBlock, parentBlock);
	importModule->finalize();

	// Codegen a call to the imported module's main function
	importModule->buildRun();

	// Import imported module into importing scope
	::importBlock(parentBlock, importBlock);

	// Execute command on imported module
	const std::string& command = ((PawsString*)importSymbol(parentBlock, "command")->value)->val;
	if (command == "parse" || command == "debug")
		importModule->print(importPath + ".ll");
	if (command == "build")
	{
		std::string errstr = "";
		if (!importModule->compile(importPath + ".o", errstr))
			std::cerr << errstr;
	}
}

int PAWRun(BlockExprAST* block, int argc, char **argv)
{
	registerType<PawsBase>(block, "PawsBase");
	registerType<PawsVoid>(block, "PawsVoid");
	registerType<PawsMetaType>(block, "PawsMetaType");
	registerType<PawsInt>(block, "PawsInt");
	registerType<PawsDouble>(block, "PawsDouble");
	registerType<PawsString>(block, "PawsString");
	registerType<PawsExprAST>(block, "PawsExprAST");
	registerType<PawsBlockExprAST>(block, "PawsBlockExprAST");
	registerType<PawsConstBlockExprASTList>(block, "PawsConstBlockExprASTList");
	registerType<PawsExprListAST>(block, "PawsExprListAST");
	registerType<PawsLiteralExprAST>(block, "PawsLiteralExprAST");
	registerType<PawsIdExprAST>(block, "PawsIdExprAST");
	registerType<PawsModule>(block, "PawsModule");
	registerType<PawsVariable>(block, "PawsVariable");
	registerType<PawsScopeType>(block, "PawsScopeType");
	registerType<PawsStringMap>(block, "PawsStringMap");
	registerType<PawsStmtMap>(block, "PawsStmtMap");
	registerType<PawsExprMap>(block, "PawsExprMap");
	registerType<PawsSymbolMap>(block, "PawsSymbolMap");

	std::vector<Variable> blockParams;
	blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		blockParams.push_back(Variable(PawsString::TYPE, new PawsString(std::string(argv[i]))));
	setBlockExprASTParams(block, blockParams);

	PAWS_PACKAGE_MANAGER().import(block);

	defineSymbol(block, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(block));
	defineSymbol(block, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(FILE_SCOPE_TYPE));

	// Define single-expr statement
	defineStmt2(block, "$E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define context-free block statement
	defineStmt2(block, "$B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define general bracketed expression
	defineExpr3(block, "($E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[0], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return getType(params[0], parentBlock);
		}
	);

	// Define return statement
	definePawsReturnStmt(block, PawsInt::TYPE);

	// Define variable lookup
	defineExpr3(block, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *var;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(block, "$L",
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
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
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
	defineExpr3(block, "$I<PawsBase> = $E<PawsBase>",
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
				defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)varAST), expr.type, expr.value);
			else
			{
				var->value = expr.value;
				var->type = expr.type;
			}
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return getType(exprAST, parentBlock);
		}
	);
defineSymbol(block, "_NULL", nullptr, new PawsVoid()); //TODO: Use one `NULL` for both paws and builtin.cpp
	defineExpr3(block, "$I<_NULL> = $E<PawsBase>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			Variable expr = codegenExpr(exprAST, parentBlock);

			defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), expr.type, expr.value);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* exprAST = params[1];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return getType(exprAST, parentBlock);
		}
	);

	// Define string concatenation
	defineExpr(block, "$E<PawsString> + $E<PawsString>",
		+[](std::string a, std::string b) -> std::string {
			return a + b;
		}
	);

	// Define string length getter
	defineExpr(block, "$E<PawsString>.length",
		+[](std::string a) -> int {
			return a.length();
		}
	);

	// Define substring
	defineExpr(block, "$E<PawsString>.substr($E<PawsInt>)",
		+[](std::string a, int b) -> std::string {
			return a.substr(b);
		}
	);
	defineExpr(block, "$E<PawsString>.substr($E<PawsInt>, $E<PawsInt>)",
		+[](std::string a, int b, int c) -> std::string {
			return a.substr(b, c);
		}
	);

	// Define substring finder
	defineExpr(block, "$E<PawsString>.find($E<PawsString>)",
		+[](std::string a, std::string b) -> int {
			return a.find(b);
		}
	);
	defineExpr(block, "$E<PawsString>.rfind($E<PawsString>)",
		+[](std::string a, std::string b) -> int {
			return a.rfind(b);
		}
	);

	// Define string concatenation
	defineExpr(block, "$E<PawsInt> * $E<PawsString>",
		+[](int a, std::string b) -> std::string {
			std::string result;
			for (int i = 0; i < a; ++i)
				result += b;
			return result;
		}
	);
	defineExpr(block, "$E<PawsString> * $E<PawsInt>",
		+[](std::string a, int b) -> std::string {
			std::string result;
			for (int i = 0; i < b; ++i)
				result += a;
			return result;
		}
	);

	// Define string relations
	defineExpr(block, "$E<PawsString> == $E<PawsString>",
		+[](std::string a, std::string b) -> int {
			return a == b;
		}
	);
	defineExpr(block, "$E<PawsString> != $E<PawsString>",
		+[](std::string a, std::string b) -> int {
			return a != b;
		}
	);

	// Define integer addition
	defineExpr(block, "$E<PawsInt> + $E<PawsInt>",
		+[](int a, int b) -> int {
			return a + b;
		}
	);

	// Define integer subtraction
	defineExpr(block, "$E<PawsInt> - $E<PawsInt>",
		+[](int a, int b) -> int {
			return a - b;
		}
	);

	// Define integer minimum
	defineExpr(block, "min($E<PawsInt>, $E<PawsInt>)",
		+[](int a, int b) -> int {
			return a < b ? a : b;
		}
	);

	// Define integer maximum
	defineExpr(block, "max($E<PawsInt>, $E<PawsInt>)",
		+[](int a, int b) -> int {
			return a > b ? a : b;
		}
	);

	// Define integer relations
	defineExpr(block, "$E<PawsInt> == $E<PawsInt>",
		+[](int a, int b) -> int {
			return a == b;
		}
	);
	defineExpr(block, "$E<PawsInt> != $E<PawsInt>",
		+[](int a, int b) -> int {
			return a != b;
		}
	);
	defineExpr(block, "$E<PawsInt> <= $E<PawsInt>",
		+[](int a, int b) -> int {
			return a <= b;
		}
	);
	defineExpr(block, "$E<PawsInt> >= $E<PawsInt>",
		+[](int a, int b) -> int {
			return a >= b;
		}
	);

	// Define logical operators
	defineExpr2(block, "$E<PawsInt> && $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->val &&
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->val
			));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E<PawsInt> || $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->val ||
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->val
			));
		},
		PawsInt::TYPE
	);

	// Define boolean negation
	defineExpr(block, "!$E<PawsInt>",
		+[](int a) -> int {
			return !a;
		}
	);

	// Define is-NULL
	defineExpr2(block, "$E == NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) == nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E != NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) != nullptr)); //TODO: Checking if type == nullptr only detectes undefined variables and void
		},
		PawsInt::TYPE
	);

	// Define pointer equivalence operators
	//TODO: Generalize this beyond PawsConstExprAST
	defineExpr(block, "$E<PawsConstExprAST> == NULL",
		+[](const ExprAST* a) -> int {
			return a == nullptr;
		}
	);
	defineExpr(block, "$E<PawsConstExprAST> != NULL",
		+[](const ExprAST* a) -> int {
			return a != nullptr;
		}
	);
	defineExpr(block, "$E<PawsConstExprAST> == $E<PawsConstExprAST>",
		+[](const ExprAST* a, const ExprAST* b) -> int {
			return a == b;
		}
	);
	defineExpr(block, "$E<PawsConstExprAST> != $E<PawsConstExprAST>",
		+[](const ExprAST* a, const ExprAST* b) -> int {
			return a != b;
		}
	);

	// Define if statement
	defineStmt2(block, "if($E<PawsInt>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->val)
				codegenExpr(params[1], parentBlock);
		}
	);

	// Define if/else statement
	defineStmt2(block, "if($E<PawsInt>) $S else $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsInt* condition = (PawsInt*)codegenExpr(params[0], parentBlock).value;
			if (condition->val)
				codegenExpr(params[1], parentBlock);
			else
				codegenExpr(params[2], parentBlock);
		}
	);

	// Define inline if expression
	defineExpr3(block, "$E<PawsInt> ? $E : $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return codegenExpr(params[((PawsInt*)codegenExpr(params[0], parentBlock).value)->val ? 1 : 2], parentBlock);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			BaseType* ta = getType(params[1], parentBlock);
			BaseType* tb = getType(params[2], parentBlock);
			if (ta != tb)
				raiseCompileError("TODO", params[0]);
			return ta;
		}
	);

	// Define map iterating for statement
	defineStmt2(block, "for ($I, $I: $E<PawsStringMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsStringMap* map = (PawsStringMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsString key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsString::TYPE, &value);
			for (std::pair<const std::string, std::string> pair: map->val)
			{
				key.val = pair.first;
				value.val = pair.second;
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	defineExpr2(block, "str($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PawsString::TYPE, new PawsString(getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val)));
		},
		PawsString::TYPE
	);
	defineExpr(block, "str($E<PawsInt>)",
		+[](int value) -> std::string {
			return std::to_string(value);
		}
	);
	defineExpr(block, "str($E<PawsString>)",
		+[](std::string value) -> std::string {
			return value;
		}
	);
	defineExpr(block, "str($E<PawsConstExprAST>)",
		+[](const ExprAST* value) -> std::string {
			return ExprASTToString(value);
		}
	);
	defineExpr(block, "str($E<PawsStringMap>)",
		+[](std::map<std::string, std::string> value) -> std::string {
			//TODO: Use stringstream instead
			std::string str = "{";
			for (std::pair<const std::string, std::string>& pair: value)
				str += "TODO ";
			str += "}";
			return str;
		}
	);
	defineExpr(block, "print()",
		+[]() -> void {
			std::cout << '\n';
		}
	);
	defineExpr(block, "printerr()",
		+[]() -> void {
			std::cerr << '\n';
		}
	);
	defineExpr2(block, "print($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			std::cout << getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val) << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr2(block, "printerr($E<PawsMetaType>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			std::cerr << getTypeName(((PawsMetaType*)codegenExpr(exprAST, parentBlock).value)->val) << '\n';
			return Variable(PawsVoid::TYPE, nullptr);
		},
		PawsVoid::TYPE
	);
	defineExpr(block, "print($E<PawsString>)",
		+[](std::string value) -> void {
			std::cout << value << '\n';
		}
	);
	defineExpr(block, "printerr($E<PawsString>)",
		+[](std::string value) -> void {
			std::cerr << value << '\n';
		}
	);
	defineExpr(block, "print($E<PawsInt>)",
		+[](int value) -> void {
			std::cout << value << '\n';
		}
	);
	defineExpr(block, "printerr($E<PawsInt>)",
		+[](int value) -> void {
			std::cerr << value << '\n';
		}
	);
	defineExpr2(block, "type($E<PawsBase>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* exprAST = params[0];
			if (ExprASTIsCast(exprAST))
				exprAST = getCastExprASTSource((CastExprAST*)exprAST);
			return Variable(PawsMetaType::TYPE, new PawsMetaType(getType(exprAST, parentBlock)));
		},
		PawsMetaType::TYPE
	);

	defineExpr2(block, "map($E<PawsString>: $E<PawsString>, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& keys = getExprListASTExpressions((ExprListAST*)params[0]);
			std::vector<ExprAST*>& values = getExprListASTExpressions((ExprListAST*)params[1]);
			std::map<std::string, std::string> map;
			for (size_t i = 0; i < keys.size(); ++i)
				map[((PawsString*)codegenExpr(keys[i], parentBlock).value)->val] = ((PawsString*)codegenExpr(values[i], parentBlock).value)->val;
			return Variable(PawsStringMap::TYPE, new PawsStringMap(map));
		},
		PawsStringMap::TYPE
	);

	defineExpr(block, "$E<PawsStringMap>.contains($E<PawsString>)",
		+[](std::map<std::string, std::string> map, std::string key) -> int {
			return map.find(key) != map.end();
		}
	);
	defineExpr(block, "$E<PawsStringMap>[$E<PawsString>]",
		+[](std::map<std::string, std::string> map, std::string key) -> std::string {
			auto pair = map.find(key);
			return pair == map.end() ? nullptr : pair->second;
		}
	);

	defineExpr(block, "parseCFile($E<PawsString>)",
		+[](std::string filename) -> BlockExprAST* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parseCFile(fname);
		}
	);

	defineExpr(block, "parsePythonFile($E<PawsString>)",
		+[](std::string filename) -> BlockExprAST* {
			// Unbind parseCFile filename parameter lifetime from local filename parameter
			char* fname = new char[filename.size() + 1];
			strcpy(fname, filename.c_str());
			return parsePythonFile(fname);
		}
	);

	defineExpr(block, "$E<PawsConstExprAST>.filename",
		+[](const ExprAST* expr) -> std::string {
			return getExprFilename(expr);
		}
	);
	defineExpr(block, "$E<PawsConstExprAST>.line",
		+[](const ExprAST* expr) -> int {
			return getExprLine(expr);
		}
	);
	defineExpr(block, "$E<PawsConstExprAST>.column",
		+[](const ExprAST* expr) -> int {
			return getExprColumn(expr);
		}
	);
	defineExpr(block, "$E<PawsConstExprAST>.endLine",
		+[](const ExprAST* expr) -> int {
			return getExprEndLine(expr);
		}
	);
	defineExpr(block, "$E<PawsConstExprAST>.endColumn",
		+[](const ExprAST* expr) -> int {
			return getExprEndColumn(expr);
		}
	);

	defineExpr(block, "$E<PawsStmtMap>.length",
		+[](StmtMap stmts) -> int {
			return countBlockExprASTStmts(stmts);
		}
	);
	defineStmt2(block, "for ($I, $I: $E<PawsStmtMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsStmtMap* stmts = (PawsStmtMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsType<const ExprListAST*> key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsType<const ExprListAST*>::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsType<const ExprListAST*>::TYPE, &value);
			iterateBlockExprASTStmts(stmts->val, [&](const ExprListAST* tplt, const CodegenContext* stmt) {
				key.val = tplt;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(block, "$E<PawsStmtMap>[$E ...] = $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			StmtMap const stmts = ((PawsStmtMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = stmts;
			const std::vector<ExprAST*>& stmtParamsAST = getExprListASTExpressions((ExprListAST*)params[1]);

			// Collect parameters
			std::vector<ExprAST*> stmtParams;
			for (ExprAST* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, stmtParams, blockParams);

			definePawsReturnStmt(scope, PawsVoid::TYPE);

			defineStmt3(scope, stmtParamsAST, new StmtContext((BlockExprAST*)params[2], blockParams));
		}
	);

	defineExpr(block, "$E<PawsExprMap>.length",
		+[](ExprMap exprs) -> int {
			return countBlockExprASTExprs(exprs);
		}
	);
	defineStmt2(block, "for ($I, $I: $E<PawsExprMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsExprMap* exprs = (PawsExprMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsType<const ExprAST*> key, value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsType<const ExprAST*>::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsType<const ExprAST*>::TYPE, &value);
			iterateBlockExprASTExprs(exprs->val, [&](const ExprAST* tplt, const CodegenContext* expr) {
				key.val = tplt;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(block, "$E<PawsExprMap>[$E] = <$I> $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			ExprMap const exprs = ((PawsExprMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = exprs;
			ExprAST* exprParamAST = params[1];
			BaseType* exprType = (BaseType*)codegenExpr(params[2], parentBlock).value->getConstantValue();
			//TODO: Check for errors

			// Collect parameters
			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprParamAST, exprParamAST, exprParams);

			// Get block parameter types
			std::vector<Variable> blockParams;
			getBlockParameterTypes(parentBlock, exprParams, blockParams);

			definePawsReturnStmt(scope, exprType);

			defineExpr5(scope, exprParamAST, new ExprContext((BlockExprAST*)params[3], exprType, blockParams));
		}
	);

	defineExpr(block, "$E<PawsSymbolMap>.length",
		+[](SymbolMap symbols) -> int {
			return countBlockExprASTSymbols(symbols);
		}
	);
	defineStmt2(block, "for ($I, $I: $E<PawsSymbolMap>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* keyExpr = (IdExprAST*)params[0];
			IdExprAST* valueExpr = (IdExprAST*)params[1];
			PawsSymbolMap* symbols = (PawsSymbolMap*)codegenExpr(params[2], parentBlock).value;
			BlockExprAST* body = (BlockExprAST*)params[3];
			PawsString key;
			PawsVariable value;
			defineSymbol(body, getIdExprASTName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprASTName(valueExpr), PawsVariable::TYPE, &value);
			iterateBlockExprASTSymbols(symbols->val, [&](const std::string& name, const Variable& symbol) {
				key.val = name;
				value.val = symbol;
				codegenExpr((ExprAST*)body, parentBlock);
			});
		}
	);
	defineStmt2(block, "$E<PawsSymbolMap>[$I] = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			SymbolMap const symbols = ((PawsSymbolMap*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* const scope = symbols;
			IdExprAST* symbolNameAST = (IdExprAST*)params[1];
			const Variable& symbol = codegenExpr(params[2], parentBlock);

			defineSymbol(scope, getIdExprASTName(symbolNameAST), symbol.type, symbol.value);
		}
	);

	defineExpr(block, "$E<PawsConstBlockExprAST>.parent",
		+[](const BlockExprAST* block) -> BlockExprAST* {
			return getBlockExprASTParent(block);
		}
	);
	defineExpr(block, "$E<PawsBlockExprAST>.parent = $E<PawsBlockExprAST>",
		+[](BlockExprAST* block, BlockExprAST* parent) -> void {
			setBlockExprASTParent(block, parent);
		}
	);

	defineExpr(block, "$E<PawsConstBlockExprAST>.references",
		+[](const BlockExprAST* block) -> const std::vector<BlockExprAST*>& {
			return getBlockExprASTReferences(block);
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.stmts",
		+[](BlockExprAST* block) -> StmtMap {
			return StmtMap{block};
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.exprs",
		+[](BlockExprAST* block) -> ExprMap {
			return ExprMap{block};
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.symbols",
		+[](BlockExprAST* block) -> SymbolMap {
			return SymbolMap{block};
		}
	);

	// Define codegen
	defineExpr3(block, "$E<PawsExprAST>.codegen($E<PawsBlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* expr = ((PawsExprAST*)codegenExpr(params[0], parentBlock).value)->val;
			BlockExprAST* scope = ((PawsBlockExprAST*)codegenExpr(params[1], parentBlock).value)->val;
			if (!ExprASTIsCast(params[0]))
			{
				codegenExpr(expr, scope);
				return Variable(PawsVoid::TYPE, nullptr);
			}
			return codegenExpr(expr, scope);
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			if (!ExprASTIsCast(params[0]))
				return PawsVoid::TYPE;//raiseCompileError("can't infer codegen type from non-templated ExprAST", params[0]);
			BaseType* type = getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			return ((PawsTpltType*)type)->tpltType;
		}
	);
	defineExpr(block, "$E<PawsBlockExprAST>.codegen($E<PawsBlockExprAST>)",
		+[](ExprAST* expr, BlockExprAST* scope) -> void {
			codegenExpr(expr, scope);
		}
	);
	defineExpr(block, "$E<PawsBlockExprAST>.codegen(NULL)",
		+[](BlockExprAST* block) -> void {
			codegenExpr((ExprAST*)block, nullptr);
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.import($E<PawsBlockExprAST>)",
		+[](BlockExprAST* scope, BlockExprAST* block) -> void {
			importBlock(scope, block);
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.scopeType",
		+[](BlockExprAST* scope) -> BaseScopeType* {
			return getScopeType(scope);
		}
	);
	defineExpr(block, "$E<PawsBlockExprAST>.scopeType = $E<PawsScopeType>",
		+[](BlockExprAST* scope, BaseScopeType* scopeType) -> BaseScopeType* {
			setScopeType(scope, scopeType);
			return scopeType;
		}
	);

	defineExpr(block, "$E<PawsConstBlockExprASTList>[$E<PawsInt>]",
		+[](const std::vector<BlockExprAST*>& blocks, int idx) -> BlockExprAST* {
			return blocks[idx];
		}
	);

	defineExpr(block, "$E<PawsConstBlockExprASTList>.length",
		+[](const std::vector<BlockExprAST*>& blocks) -> int {
			return blocks.size();
		}
	);

	defineStmt2(block, "for ($I: $E<PawsConstBlockExprASTList>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* iterExpr = (IdExprAST*)params[0];
			Variable exprsVar = codegenExpr(params[1], parentBlock);
			const std::vector<BlockExprAST*>& exprs = ((PawsConstBlockExprASTList*)exprsVar.value)->val;
			BlockExprAST* body = (BlockExprAST*)params[2];
			PawsBlockExprAST iter;
			defineSymbol(body, getIdExprASTName(iterExpr), PawsBlockExprAST::TYPE, &iter);
			for (BlockExprAST* expr: exprs)
			{
				iter.val = expr;
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	defineExpr(block, "$E<PawsConstLiteralExprAST>.value",
		+[](const LiteralExprAST* expr) -> std::string {
			return getLiteralExprASTValue(expr);
		}
	);

	defineExpr(block, "$E<PawsConstIdExprAST>.name",
		+[](const IdExprAST* expr) -> std::string {
			return getIdExprASTName(expr);
		}
	);

	defineExpr3(block, "$E<PawsExprListAST>[$E<PawsInt>]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			assert(ExprASTIsCast(params[0]));
			Variable exprsVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			ExprListAST* exprs = ((PawsExprListAST*)exprsVar.value)->val;
			int idx = ((PawsInt*)codegenExpr(params[1], parentBlock).value)->val;
			return Variable(((PawsTpltType*)exprsVar.type)->tpltType, new PawsExprAST(getExprListASTExpressions(exprs)[idx]));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);

	defineStmt2(block, "for ($I: $E<PawsExprListAST>) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			assert(ExprASTIsCast(params[1]));
			IdExprAST* iterExpr = (IdExprAST*)params[0];
			Variable exprsVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[1]), parentBlock);
			ExprListAST* exprs = ((PawsExprListAST*)exprsVar.value)->val;
			BaseType* exprType = ((PawsTpltType*)exprsVar.type)->tpltType;
			BlockExprAST* body = (BlockExprAST*)params[2];
			PawsExprAST iter;
			defineSymbol(body, getIdExprASTName(iterExpr), exprType, &iter);
			for (ExprAST* expr: getExprListASTExpressions(exprs))
			{
				iter.val = expr;
				codegenExpr((ExprAST*)body, parentBlock);
			}
		}
	);

	defineExpr(block, "initCompiler()",
		+[]() -> void {
			initCompiler();
		}
	);

	defineExpr(block, "createModule($E<PawsString>, $E<PawsString>, $E<PawsInt>)",
		+[](std::string sourcePath, std::string moduleFuncName, int outputDebugSymbols) -> IModule* {
			return createModule(sourcePath, moduleFuncName, outputDebugSymbols);
		}
	);

	defineExpr(block, "$E<PawsModule>.print($E<PawsString>)",
		+[](IModule* module, std::string outputPath) -> void {
			module->print(outputPath);
		}
	);

	defineExpr(block, "$E<PawsModule>.print()",
		+[](IModule* module) -> void {
			module->print();
		}
	);

	defineExpr(block, "$E<PawsModule>.compile($E<PawsString>, $E<PawsString>)",
		+[](IModule* module, std::string outputPath, std::string errStr) -> int {
			return module->compile(outputPath, errStr);
		}
	);

	defineExpr(block, "$E<PawsModule>.run()",
		+[](IModule* module) -> int {
			return module->run();
		}
	);

	defineExpr(block, "$E<PawsModule>.buildRun()",
		+[](IModule* module) -> void {
			module->buildRun();
		}
	);

	defineExpr(block, "$E<PawsModule>.finalize()",
		+[](IModule* module) -> void {
			module->finalize();
		}
	);

	defineExpr(block, "realpath($E<PawsString>)",
		+[](std::string path) -> std::string {
			char realPath[1024];
			realpath(path.c_str(), realPath);
			return realPath;
		}
	);

	// Define import statement
	defineStmt2(block, "import $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::string importPath = ((PawsString*)codegenExpr(params[0], parentBlock).value)->val;
			importFile(parentBlock, importPath);
		}
	);

	// Define library import statement
	defineStmt2(block, "import <$I.$I>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::string importPath = "../lib/" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "." + std::string(getIdExprASTName((IdExprAST*)params[1]));
			importFile(parentBlock, importPath);
		}
	);

	try
	{
		codegenExpr((ExprAST*)block, nullptr);
	}
	catch (ReturnException err)
	{
		return ((PawsInt*)err.result.value)->val;
	}
	return 0;
}