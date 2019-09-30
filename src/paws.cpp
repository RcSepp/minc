#include <string>
#include <map>
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>
#include "api.h"
#include "builtin.h"
#include "paws_types.h"

template<> uint64_t PawsType<BaseType*>::getConstantValue() { return (uint64_t)val; }
std::set<PawsTpltType> PawsTpltType::tpltTypes;
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs)
{
	return lhs.baseType < rhs.baseType
		|| lhs.baseType == rhs.baseType && lhs.tpltType < rhs.tpltType;
}

template<typename T> void registerType(BlockExprAST* scope, const char* name)
{
	const size_t nameLen = strlen(name);

	// Define type and add type symbol to scope
	defineType(name, T::TYPE);
	defineSymbol(scope, name, PawsMetaType::TYPE, new PawsMetaType(T::TYPE));

	if (T::TYPE != PawsBase::TYPE)
	{
		// Let type derive from PawsBase
		defineOpaqueCast(scope, T::TYPE, PawsBase::TYPE);

		// Define ExprAST type hierarchy
		typedef typename std::remove_pointer<typename T::CType>::type baseCType; // Pointer-less T::CType
		typedef typename std::remove_const<baseCType>::type rawCType; // Pointer-less, const-less T::CType
		if (
			std::is_same<ExprAST, rawCType>()
			|| std::is_same<IdExprAST, rawCType>()
			|| std::is_same<CastExprAST, rawCType>()
			|| std::is_same<LiteralExprAST, rawCType>()
			|| std::is_same<PlchldExprAST, rawCType>()
			|| std::is_same<ExprListAST, rawCType>()
			|| std::is_same<StmtAST, rawCType>()
			|| std::is_same<BlockExprAST, rawCType>()
		) // If rawCType derives from ExprAST
		{
			if (!std::is_const<baseCType>()) // If T::CType != ExprAST* and T::CType is not a const type
			{
				// Register const type
				typedef PawsType<typename std::add_pointer<typename std::add_const<baseCType>::type>::type> constT; // const T
				char* constExprASTName = new char[nameLen + strlen("Const") + 1];
				strcpy(constExprASTName, "PawsConst");
				strcat(constExprASTName, name + strlen("Paws"));
				registerType<constT>(scope, constExprASTName);

				// Let type derive from const type
				defineOpaqueCast(scope, T::TYPE, constT::TYPE);

				if (!std::is_same<ExprAST, baseCType>()) // If T::CType != ExprAST*
					defineOpaqueCast(scope, T::TYPE, PawsExprAST::TYPE); // Let type derive from PawsExprAST
			}
			
			if (!std::is_same<const ExprAST, baseCType>()) // If T::CType != const ExprAST*
				defineOpaqueCast(scope, T::TYPE, PawsType<const ExprAST*>::TYPE); // Let type derive from PawsConstExprAST
		}
	}
}

struct ReturnException
{
	const Variable result;
	ReturnException(const Variable& result) : result(result) {}
};

void definePawsReturnStmt(BlockExprAST* scope, const BaseType* returnType, const char* funcName = nullptr)
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

// Templated version of defineStmt2():
// defineStmt() codegen's all inputs and wraps the output in a Variable
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
template<class P0> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0))
{
	using StmtFunc = void (*)(P0);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->val);
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
template<class R, class P0, class P1, class P2, class P3> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3))
{
	using ExprFunc = R (*)(P0, P1, P2, P3);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 4)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 5)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 6)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 7)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 8)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val)));
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
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
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

	defineSymbol(block, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(block));
	defineSymbol(block, "FILE_SCOPE_TYPE", PawsScopeType::TYPE, new PawsScopeType(new BaseScopeType()));

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