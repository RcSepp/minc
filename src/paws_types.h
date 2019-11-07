#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include <vector>
#include <set>
#include <map>
#include <cstring>

struct BaseScopeType;

extern BaseScopeType* FILE_SCOPE_TYPE;

template<typename T> struct PawsType : BaseValue
{
	typedef T CType;
	T val;
	static inline BaseType* TYPE = new BaseType();
	PawsType() {}
	PawsType(const T& val) : val(val) {}
	uint64_t getConstantValue() { return 0; }
};
template<> struct PawsType<void> : BaseValue
{
	typedef void CType;
	static inline BaseType* TYPE = new BaseType();
	PawsType() {}
	uint64_t getConstantValue() { return 0; }
};
namespace std
{
	template<typename T> struct less<PawsType<T>*>
	{
		bool operator()(const PawsType<T>* lhs, const PawsType<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

template<int T> struct PawsOpaqueType
{
	typedef void CType;
	static inline BaseType* TYPE = new BaseType();
};

struct PawsTpltType : BaseType
{
private:
	static std::set<PawsTpltType> tpltTypes;
	PawsTpltType(BaseType* baseType, BaseType* tpltType) : baseType(baseType), tpltType(tpltType) {}

public:
	BaseType *const baseType, *const tpltType;
	static PawsTpltType* get(BaseType* baseType, BaseType* tpltType)
	{
		std::set<PawsTpltType>::iterator iter = tpltTypes.find(PawsTpltType(baseType, tpltType));
		if (iter == tpltTypes.end())
		{
			iter = tpltTypes.insert(PawsTpltType(baseType, tpltType)).first;
			PawsTpltType* t = const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
			defineType((getTypeName(baseType) + '<' + getTypeName(tpltType) + '>').c_str(), t);
			defineOpaqueInheritanceCast(getRootScope(), t, PawsOpaqueType<0>::TYPE); // Let baseType<tpltType> derive from PawsBase
			defineOpaqueInheritanceCast(getRootScope(), t, baseType); // Let baseType<tpltType> derive from baseType
		}
		return const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
	}
};
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs);

typedef PawsOpaqueType<0> PawsBase;
typedef PawsType<void> PawsVoid;
typedef PawsType<BaseType*> PawsMetaType;
typedef PawsType<int> PawsInt;
typedef PawsType<double> PawsDouble;
typedef PawsType<std::string> PawsString;
typedef PawsType<ExprAST*> PawsExprAST;
typedef PawsType<BlockExprAST*> PawsBlockExprAST;
typedef PawsType<const std::vector<BlockExprAST*>&> PawsConstBlockExprASTList;
typedef PawsType<ExprListAST*> PawsExprListAST;
typedef PawsType<LiteralExprAST*> PawsLiteralExprAST;
typedef PawsType<IdExprAST*> PawsIdExprAST;
typedef PawsType<IModule*> PawsModule;
typedef PawsType<Variable> PawsVariable;
typedef PawsType<BaseScopeType*> PawsScopeType;
typedef PawsType<std::map<std::string, std::string>> PawsStringMap;

template<typename T> void registerType(BlockExprAST* scope, const char* name)
{
	const size_t nameLen = strlen(name);

	// Define type and add type symbol to scope
	defineType(name, T::TYPE);
	defineSymbol(scope, name, PawsMetaType::TYPE, new PawsMetaType(T::TYPE));

	if (T::TYPE != PawsBase::TYPE)
	{
		// Let type derive from PawsBase
		defineOpaqueInheritanceCast(scope, T::TYPE, PawsBase::TYPE);

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
			if (std::is_const<baseCType>()) // If T::CType is a const type
			{
				if (!std::is_same<ExprAST, rawCType>()) // If T::CType != const ExprAST*
					defineOpaqueInheritanceCast(scope, T::TYPE, PawsType<const ExprAST*>::TYPE); // Let type derive from PawsConstExprAST
			}
			else // If T::CType is not a const type
			{
				// Register const type
				typedef PawsType<typename std::add_pointer<typename std::add_const<baseCType>::type>::type> constT; // const T
				char* constExprASTName = new char[nameLen + strlen("Const") + 1];
				strcpy(constExprASTName, "PawsConst");
				strcat(constExprASTName, name + strlen("Paws"));
				registerType<constT>(scope, constExprASTName);

				// Let type derive from const type
				defineOpaqueInheritanceCast(scope, T::TYPE, constT::TYPE);

				if (!std::is_same<ExprAST, baseCType>()) // If T::CType != ExprAST*
					defineOpaqueInheritanceCast(scope, T::TYPE, PawsExprAST::TYPE); // Let type derive from PawsExprAST
			}
		}
	}
}

struct ReturnException
{
	const Variable result;
	ReturnException(const Variable& result) : result(result) {}
};

void definePawsReturnStmt(BlockExprAST* scope, const BaseType* returnType, const char* funcName = nullptr);

void getBlockParameterTypes(BlockExprAST* scope, const std::vector<ExprAST*> params, std::vector<Variable>& blockParams);

struct PawsCodegenContext : public CodegenContext
{
protected:
	BlockExprAST* const expr;
	BaseType* const type;
	std::vector<Variable> blockParams;
public:
	PawsCodegenContext(BlockExprAST* expr, BaseType* type, const std::vector<Variable>& blockParams);
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params);
	BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const;
};

// Templated version of defineStmt2():
// defineStmt() codegen's all inputs
void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)());
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
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 9)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 10)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 11)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 12)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsType<P11>* p11 = (PawsType<P11>*)codegenExpr(params[11], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 13)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsType<P11>* p11 = (PawsType<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsType<P12>* p12 = (PawsType<P12>*)codegenExpr(params[12], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 14)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsType<P11>* p11 = (PawsType<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsType<P12>* p12 = (PawsType<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsType<P13>* p13 = (PawsType<P13>*)codegenExpr(params[13], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 15)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsType<P11>* p11 = (PawsType<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsType<P12>* p12 = (PawsType<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsType<P13>* p13 = (PawsType<P13>*)codegenExpr(params[13], parentBlock).value;
		PawsType<P14>* p14 = (PawsType<P14>*)codegenExpr(params[14], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val, p14->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val, p14->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 16)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsType<P1>* p1 = (PawsType<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsType<P2>* p2 = (PawsType<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsType<P3>* p3 = (PawsType<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsType<P4>* p4 = (PawsType<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsType<P5>* p5 = (PawsType<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsType<P6>* p6 = (PawsType<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsType<P7>* p7 = (PawsType<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsType<P8>* p8 = (PawsType<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsType<P9>* p9 = (PawsType<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsType<P10>* p10 = (PawsType<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsType<P11>* p11 = (PawsType<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsType<P12>* p12 = (PawsType<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsType<P13>* p13 = (PawsType<P13>*)codegenExpr(params[13], parentBlock).value;
		PawsType<P14>* p14 = (PawsType<P14>*)codegenExpr(params[14], parentBlock).value;
		PawsType<P15>* p15 = (PawsType<P15>*)codegenExpr(params[15], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val, p14->val, p15->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(ExprFunc*)exprArgs)(p0->val, p1->val, p2->val, p3->val, p4->val, p5->val, p6->val, p7->val, p8->val, p9->val, p10->val, p11->val, p12->val, p13->val, p14->val, p15->val)));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsType<R>::TYPE, new ExprFunc(exprFunc));
}

// Templated version of defineExpr3():
// defineExpr() codegen's all inputs and wraps the output in a Variable
void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(), BaseType* (*exprTypeFunc)());
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

// Templated version of defineTypeCast2():
// defineTypeCast() codegen's all inputs and wraps the output in a Variable
template<class R, class P0> void defineTypeCast(BlockExprAST* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(CastFunc*)castArgs)(p0->val)));
	};
	defineTypeCast2(scope, PawsType<P0>::TYPE, PawsType<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

// Templated version of defineInheritanceCast2():
// defineInheritanceCast() codegen's all inputs and wraps the output in a Variable
template<class R, class P0> void defineInheritanceCast(BlockExprAST* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsType<P0>* p0 = (PawsType<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->val);
			return Variable(PawsType<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsType<R>::TYPE, new PawsType<R>((*(CastFunc*)castArgs)(p0->val)));
	};
	defineInheritanceCast2(scope, PawsType<P0>::TYPE, PawsType<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

#endif