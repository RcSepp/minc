#include <string>
#include <map>
#include <cstring>
#include <iostream>
#include <functional>
#include "api.h"
#include "builtin.h"

template<typename T> struct PawsType : BaseValue
{
	T val;
	static inline BaseType* TYPE = new BaseType();
	PawsType() {}
	PawsType(const T& val) : val(val) {}
	uint64_t getConstantValue() { return 0; }
};
template<> struct PawsType<void> : BaseValue
{
	static inline BaseType* TYPE = new BaseType();
	PawsType() {}
	uint64_t getConstantValue() { return 0; }
};
template<> uint64_t PawsType<BaseType*>::getConstantValue() { return (uint64_t)val; }
namespace std
{
	template<typename T> struct less<PawsType<T>*>
	{
		bool operator()(const PawsType<T>* lhs, const PawsType<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

template<int T> struct PawsOpaqueType
{
	static inline BaseType* TYPE = new BaseType();
};

typedef PawsOpaqueType<0> PawsBase;
typedef PawsType<void> PawsVoid;
typedef PawsType<BaseType*> PawsMetaType;
typedef PawsType<int> PawsInt;
typedef PawsType<std::string> PawsString;
typedef PawsType<ExprAST*> PawsExprAST;
typedef PawsType<BlockExprAST*> PawsBlockExprAST;
typedef PawsType<IModule*> PawsModule;
typedef PawsType<std::map<std::string, std::string>> PawsStringMap;
typedef PawsType<std::map<PawsExprAST*, PawsExprAST*>> PawsExprASTMap;


struct ReturnException
{
	const int result;
	ReturnException(int result) : result(result) {}
};

// Templated version of defineStmt2():
// defineStmt() codegen's all inputs and wraps the output in a Variable
void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)())
{
	using StmtFunc = void (*)();
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		(*(StmtFunc*)stmtArgs)();
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0))
{
	using StmtFunc = void (*)(P0);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->val);
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->val, p1->val);
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1, class P2> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0, P1, P2))
{
	using StmtFunc = void (*)(P0, P1, P2);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->val, p1->val, p2->val);
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}

// Templated version of defineExpr2():
// defineExpr() codegen's all inputs and wraps the output in a Variable
template<class R> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)())
{
	using ExprFunc = R (*)();
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)();
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)()));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0))
{
	using ExprFunc = R (*)(P0);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1))
{
	using ExprFunc = R (*)(P0, P1);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2))
{
	using ExprFunc = R (*)(P0, P1, P2);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}

// Templated version of defineExpr3():
// defineExpr() codegen's all inputs
void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(), BaseType* (*exprTypeFunc)())
{
	using ExprFunc = Variable (*)();
	using ExprTypeFunc = BaseType* (*)();
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first();
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second();
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class P0> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0), BaseType* (*exprTypeFunc)(BaseType*))
{
	using ExprFunc = Variable (*)(P0);
	using ExprTypeFunc = BaseType* (*)(BaseType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->val);
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		BaseType* p0 = getType(params[0], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class P0, class P1> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0, P1), BaseType* (*exprTypeFunc)(BaseType*, BaseType*))
{
	using ExprFunc = Variable (*)(P0, P1);
	using ExprTypeFunc = BaseType* (*)(BaseType*, BaseType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->val, p1->val);
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		BaseType* p0 = getType(params[0], parentBlock);
		BaseType* p1 = getType(params[1], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0, P1, P2), BaseType* (*exprTypeFunc)(BaseType*, BaseType*, BaseType*))
{
	using ExprFunc = Variable (*)(P0, P1, P2);
	using ExprTypeFunc = BaseType* (*)(BaseType*, BaseType*, BaseType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
 		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->val, p1->val, p2->val);
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		BaseType* p0 = getType(params[0], parentBlock);
		BaseType* p1 = getType(params[1], parentBlock);
		BaseType* p2 = getType(params[2], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1, p2);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

int PAWRun(BlockExprAST* block, int argc, char **argv)
{
	defineType("PawsBase", PawsBase::TYPE);
	defineSymbol(block, "PawsBase", PawsBase::TYPE, new PawsMetaType(PawsBase::TYPE));

	defineType("PawsVoid", PawsVoid::TYPE);
	defineSymbol(block, "PawsVoid", PawsMetaType::TYPE, new PawsMetaType(PawsVoid::TYPE));
	defineOpaqueCast(block, PawsVoid::TYPE, PawsBase::TYPE);

	defineType("PawsMetaType", PawsMetaType::TYPE);
	defineSymbol(block, "PawsMetaType", PawsMetaType::TYPE, new PawsMetaType(PawsMetaType::TYPE));
	defineOpaqueCast(block, PawsMetaType::TYPE, PawsBase::TYPE);

	defineType("PawsInt", PawsInt::TYPE);
	defineSymbol(block, "PawsInt", PawsMetaType::TYPE, new PawsMetaType(PawsInt::TYPE));
	defineOpaqueCast(block, PawsInt::TYPE, PawsBase::TYPE);

	defineType("PawsString", PawsString::TYPE);
	defineSymbol(block, "PawsString", PawsMetaType::TYPE, new PawsMetaType(PawsString::TYPE));
	defineOpaqueCast(block, PawsString::TYPE, PawsBase::TYPE);

	defineType("PawsExprAST", PawsExprAST::TYPE);
	defineSymbol(block, "PawsExprAST", PawsMetaType::TYPE, new PawsMetaType(PawsExprAST::TYPE));
	defineOpaqueCast(block, PawsExprAST::TYPE, PawsBase::TYPE);

	defineType("PawsBlockExprAST", PawsBlockExprAST::TYPE);
	defineSymbol(block, "PawsBlockExprAST", PawsMetaType::TYPE, new PawsMetaType(PawsBlockExprAST::TYPE));
	defineOpaqueCast(block, PawsBlockExprAST::TYPE, PawsBase::TYPE);
	defineOpaqueCast(block, PawsBlockExprAST::TYPE, PawsExprAST::TYPE);

	defineType("PawsModule", PawsModule::TYPE);
	defineSymbol(block, "PawsModule", PawsMetaType::TYPE, new PawsMetaType(PawsModule::TYPE));
	defineOpaqueCast(block, PawsModule::TYPE, PawsBase::TYPE);

	defineType("PawsStringMap", PawsStringMap::TYPE);
	defineSymbol(block, "PawsStringMap", PawsMetaType::TYPE, new PawsMetaType(PawsStringMap::TYPE));
	defineOpaqueCast(block, PawsStringMap::TYPE, PawsBase::TYPE);

	std::vector<Variable> blockParams;
	blockParams.reserve(argc);
	for (int i = 0; i < argc; ++i)
		blockParams.push_back(Variable(PawsString::TYPE, new PawsString(std::string(argv[i]))));
	setBlockExprASTParams(block, blockParams);

	defineSymbol(block, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(block));

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
	defineStmt(block, "return $E<PawsInt>",
		+[](int result) {
			throw ReturnException(result);
		}
	);

	// Define variable lookup
	defineExpr3(block, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			if (var == nullptr)
				raiseCompileError(("`" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "` was not declared in this scope").c_str(), params[0]);
			return *var;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(block, "$L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);

			if (value[0] == '"' || value[0] == '\'')
				return Variable(PawsString::TYPE, new PawsString(std::string(value + 1, strlen(value) - 2)));
			
			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);
			return Variable(PawsInt::TYPE, new PawsInt(intValue));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			return value[0] == '"' || value[0] == '\'' ? PawsString::TYPE : PawsInt::TYPE;
		}
	);

	// Define variable assignment
	defineExpr3(block, "$I = $E<PawsBase>",
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
	defineExpr(block, "$E<PawsString>.length()",
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
	defineExpr(block, "$E<PawsInt> && $E<PawsInt>",
		+[](int a, int b) -> int {
			return a && b;
		}
	);
	defineExpr(block, "$E<PawsInt> || $E<PawsInt>",
		+[](int a, int b) -> int {
			return a || b;
		}
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
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) == nullptr));
		},
		PawsInt::TYPE
	);
	defineExpr2(block, "$E != NULL",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(getType(params[0], parentBlock) != nullptr));
		},
		PawsInt::TYPE
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
	defineExpr(block, "str($E<PawsExprAST>)",
		+[](ExprAST* value) -> std::string {
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
			return parseCFile(filename.c_str());
		}
	);

	defineExpr(block, "$E<PawsBlockExprAST>.exprs",
		+[](BlockExprAST* a) -> void {
			//TODO
		}
	);

	defineExpr(block, "$E<PawsExprAST>.codegen($E<PawsBlockExprAST>)",
		+[](ExprAST* expr, BlockExprAST* scope) -> void {
			codegenExpr(expr, scope);
		}
	);

	defineExpr(block, "initCompiler()",
		+[]() -> void {
			initCompiler();
		}
	);

	defineExpr(block, "initBuiltinSymbols()",
		+[]() -> void {
			initBuiltinSymbols();
		}
	);

	defineExpr(block, "defineBuiltinSymbols($E<PawsBlockExprAST>)",
		+[](BlockExprAST* block) -> void {
			defineBuiltinSymbols(block);
		}
	);

	defineExpr(block, "createModule($E<PawsString>, $E<PawsBlockExprAST>, $E<PawsInt>)",
		+[](std::string sourcePath, BlockExprAST* moduleBlock, int outputDebugSymbols) -> IModule* {
			return createModule(sourcePath, moduleBlock, outputDebugSymbols);
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
		+[](IModule* module) -> void {
			module->run();
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

// defineStmt2(block, "foo $E<PawsMetaType>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );
// defineStmt2(block, "foo $E<PawsBase>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );
// defineStmt2(block, "foo $E<PawsInt>",
// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 		int abc = 0;
// 	}
// );

/*defineStmt2(block, "$E<PawsMetaType> $I",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
		int abc = 0;
	}
);
defineExpr2(block, "$E<PawsMetaType>*",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) -> Variable {
		ExprAST* exprAST = params[0];
		if (ExprASTIsCast(exprAST))
			exprAST = getCastExprASTSource((CastExprAST*)exprAST);
		return Variable(PawsMetaType::TYPE, new PawsMetaType(getType(exprAST, parentBlock)));
	},
	PawsMetaType::TYPE
);*/

// std::map<Variable, Variable> foo;
// Variable a = Variable(PawsInt::TYPE, new PawsInt(1));
// Variable b = Variable(PawsInt::TYPE, new PawsInt(2));
// Variable c = Variable(PawsInt::TYPE, new PawsInt(1));
// Variable d = Variable(PawsString::TYPE, new PawsString("1"));
// foo[a] = b;
// foo[d] = c;
// Variable bar1 = foo.at(c);
// Variable bar2 = foo.at(Variable(PawsString::TYPE, new PawsString("1")));


	try
	{
		codegenExpr((ExprAST*)block, nullptr);
	}
	catch (ReturnException err)
	{
		return err.result;
	}
	return 0;
}