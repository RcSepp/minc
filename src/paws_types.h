#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include <vector>
#include <set>
#include <map>
#include <cstring>

struct BaseScopeType;

extern BaseScopeType* FILE_SCOPE_TYPE;

struct PawsType : public BaseType
{
};

template<typename T> struct PawsValue : BaseValue
{
private:
	T val;

public:
	typedef T CType;
	static inline PawsType* TYPE = new PawsType();
	PawsValue() {}
	PawsValue(const T& val) : val(val) {}
	uint64_t getConstantValue() { return 0; }
	T& get() { return val; }
	void set(const T& val) { this->val = val; }
};
template<> struct PawsValue<void> : BaseValue
{
	typedef void CType;
	static inline PawsType* TYPE = new PawsType();
	PawsValue() {}
	uint64_t getConstantValue() { return 0; }
};
namespace std
{
	template<typename T> struct less<PawsValue<T>*>
	{
		bool operator()(const PawsValue<T>* lhs, const PawsValue<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

template<int T> struct PawsOpaqueValue
{
	typedef void CType;
	static inline PawsType* TYPE = new PawsType();
};

struct PawsTpltType : PawsType
{
private:
	static std::set<PawsTpltType> tpltTypes;
	PawsTpltType(PawsType* baseType, PawsType* tpltType) : baseType(baseType), tpltType(tpltType) {}

public:
	PawsType *const baseType, *const tpltType;
	static PawsTpltType* get(PawsType* baseType, PawsType* tpltType)
	{
		std::set<PawsTpltType>::iterator iter = tpltTypes.find(PawsTpltType(baseType, tpltType));
		if (iter == tpltTypes.end())
		{
			iter = tpltTypes.insert(PawsTpltType(baseType, tpltType)).first;
			PawsTpltType* t = const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
			defineType((getTypeName(baseType) + '<' + getTypeName(tpltType) + '>').c_str(), t);
			defineOpaqueInheritanceCast(getRootScope(), t, PawsOpaqueValue<0>::TYPE); // Let baseType<tpltType> derive from PawsBase
			defineOpaqueInheritanceCast(getRootScope(), t, baseType); // Let baseType<tpltType> derive from baseType
		}
		return const_cast<PawsTpltType*>(&*iter); //TODO: Find a way to avoid const_cast
	}
};
bool operator<(const PawsTpltType& lhs, const PawsTpltType& rhs);

typedef PawsOpaqueValue<0> PawsBase;
typedef PawsValue<void> PawsVoid;
typedef PawsValue<PawsType*> PawsMetaType;
typedef PawsValue<int> PawsInt;
typedef PawsValue<double> PawsDouble;
typedef PawsValue<std::string> PawsString;
typedef PawsValue<ExprAST*> PawsExprAST;
typedef PawsValue<BlockExprAST*> PawsBlockExprAST;
typedef PawsValue<const std::vector<BlockExprAST*>&> PawsConstBlockExprASTList;
typedef PawsValue<ExprListAST*> PawsExprListAST;
typedef PawsValue<LiteralExprAST*> PawsLiteralExprAST;
typedef PawsValue<IdExprAST*> PawsIdExprAST;
typedef PawsValue<IModule*> PawsModule;
typedef PawsValue<Variable> PawsVariable;
typedef PawsValue<BaseScopeType*> PawsScopeType;
typedef PawsValue<std::map<std::string, std::string>> PawsStringMap;

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
					defineOpaqueInheritanceCast(scope, T::TYPE, PawsValue<const ExprAST*>::TYPE); // Let type derive from PawsConstExprAST
			}
			else // If T::CType is not a const type
			{
				// Register const type
				typedef PawsValue<typename std::add_pointer<typename std::add_const<baseCType>::type>::type> constT; // const T
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
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get());
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get());
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1, class P2> void defineStmt(BlockExprAST* scope, const char* tpltStr, void (*stmtFunc)(P0, P1, P2))
{
	using StmtFunc = void (*)(P0, P1, P2);
	StmtBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs){
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get(), p2->get());
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
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)()));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0))
{
	using ExprFunc = R (*)(P0);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1))
{
	using ExprFunc = R (*)(P0, P1);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2))
{
	using ExprFunc = R (*)(P0, P1, P2);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3))
{
	using ExprFunc = R (*)(P0, P1, P2, P3);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 4)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 5)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 6)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 7)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 8)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 9)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 10)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 11)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 12)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsValue<P11>* p11 = (PawsValue<P11>*)codegenExpr(params[11], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 13)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsValue<P11>* p11 = (PawsValue<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsValue<P12>* p12 = (PawsValue<P12>*)codegenExpr(params[12], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 14)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsValue<P11>* p11 = (PawsValue<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsValue<P12>* p12 = (PawsValue<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsValue<P13>* p13 = (PawsValue<P13>*)codegenExpr(params[13], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 15)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsValue<P11>* p11 = (PawsValue<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsValue<P12>* p12 = (PawsValue<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsValue<P13>* p13 = (PawsValue<P13>*)codegenExpr(params[13], parentBlock).value;
		PawsValue<P14>* p14 = (PawsValue<P14>*)codegenExpr(params[14], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15> void defineExpr(BlockExprAST* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 16)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		PawsValue<P6>* p6 = (PawsValue<P6>*)codegenExpr(params[6], parentBlock).value;
		PawsValue<P7>* p7 = (PawsValue<P7>*)codegenExpr(params[7], parentBlock).value;
		PawsValue<P8>* p8 = (PawsValue<P8>*)codegenExpr(params[8], parentBlock).value;
		PawsValue<P9>* p9 = (PawsValue<P9>*)codegenExpr(params[9], parentBlock).value;
		PawsValue<P10>* p10 = (PawsValue<P10>*)codegenExpr(params[10], parentBlock).value;
		PawsValue<P11>* p11 = (PawsValue<P11>*)codegenExpr(params[11], parentBlock).value;
		PawsValue<P12>* p12 = (PawsValue<P12>*)codegenExpr(params[12], parentBlock).value;
		PawsValue<P13>* p13 = (PawsValue<P13>*)codegenExpr(params[13], parentBlock).value;
		PawsValue<P14>* p14 = (PawsValue<P14>*)codegenExpr(params[14], parentBlock).value;
		PawsValue<P15>* p15 = (PawsValue<P15>*)codegenExpr(params[15], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}

// Templated version of defineExpr3():
// defineExpr() codegen's all inputs and wraps the output in a Variable
void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(), PawsType* (*exprTypeFunc)());
template<class P0> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0), PawsType* (*exprTypeFunc)(PawsType*))
{
	using ExprFunc = Variable (*)(P0);
	using ExprTypeFunc = PawsType* (*)(PawsType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get());
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		PawsType* p0 = getType(params[0], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class P0, class P1> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0, P1), PawsType* (*exprTypeFunc)(PawsType*, PawsType*))
{
	using ExprFunc = Variable (*)(P0, P1);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get());
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(BlockExprAST* scope, const char* tpltStr, Variable (*exprFunc)(P0, P1, P2), PawsType* (*exprTypeFunc)(PawsType*, PawsType*, PawsType*))
{
	using ExprFunc = Variable (*)(P0, P1, P2);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*, PawsType*);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
 		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get(), p2->get());
	};
	ExprTypeBlock typeCodeBlock = [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		PawsType* p2 = getType(params[2], parentBlock);
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
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
	};
	defineTypeCast2(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

// Templated version of defineInheritanceCast2():
// defineInheritanceCast() codegen's all inputs and wraps the output in a Variable
template<class R, class P0> void defineInheritanceCast(BlockExprAST* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	ExprBlock codeBlock = [](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (ExprAST*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			return Variable(PawsValue<R>::TYPE, nullptr);
		}
		else
			return Variable(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
	};
	defineInheritanceCast2(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

#endif