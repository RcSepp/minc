#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <mutex>

struct MincScopeType;

extern MincScopeType* FILE_SCOPE_TYPE;

template<typename T> struct PawsValue;

class _Type;

struct PawsBase : MincObject
{
	typedef void CType;
	static PawsValue<_Type*>* const TYPE;
	virtual PawsBase* copy() { return new PawsBase(); }
	virtual const std::string toString() const;
};

template<typename T> struct PawsValue : PawsBase
{
private:
	T val;

public:
	typedef T CType;
	static PawsValue<_Type*>* const TYPE;
	PawsValue() {}
	PawsValue(const T& val) : val(val) {}
	PawsBase* copy() { return new PawsValue<T>(val); }
	const std::string toString() const { return PawsBase::toString(); }
	T& get() { return val; }
	const T& get() const { return val; }
	void set(const T& val) { this->val = val; }
};
template<> struct PawsValue<_Type*> : PawsBase
{
	typedef _Type* CType;
	static PawsValue<_Type*>* const TYPE;
	PawsValue<_Type*> *valType, *ptrType;
	std::string name;
	PawsValue() : valType(nullptr), ptrType(nullptr), name("UNKNOWN_PAWS_TYPE") {}
	PawsBase* copy() { return this; /* Pass all types by reference to allow matching of aliased types */ }
	const std::string toString() const;
};
template<> struct PawsValue<void> : PawsBase
{
	typedef void CType;
	static PawsValue<_Type*>* const TYPE;
	PawsValue() {}
	PawsBase* copy() { return new PawsValue<void>(); }
};
namespace std
{
	template<typename T> struct less<PawsValue<T>*>
	{
		bool operator()(const PawsValue<T>* lhs, const PawsValue<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

struct PawsTpltType : PawsValue<_Type*>
{
private:
	static std::mutex mutex;
	static std::set<PawsTpltType*> tpltTypes;
	PawsTpltType(PawsValue<_Type*>* baseType, PawsValue<_Type*>* tpltType) : baseType(baseType), tpltType(tpltType) {}

public:
	PawsValue<_Type*> *const baseType, *const tpltType;
	static PawsTpltType* get(MincBlockExpr* scope, PawsValue<_Type*>* baseType, PawsValue<_Type*>* tpltType);
};
namespace std
{
	template<> struct less<PawsTpltType*>
	{
		bool operator()(PawsTpltType* lhs, PawsTpltType* rhs) const
		{
			return lhs->baseType < rhs->baseType
				|| (lhs->baseType == rhs->baseType && lhs->tpltType < rhs->tpltType);
		}
	};
}

typedef PawsValue<_Type*> PawsType;
typedef PawsValue<void> PawsVoid;
typedef PawsValue<int> PawsInt;
typedef PawsValue<double> PawsDouble;
typedef PawsValue<std::string> PawsString;
typedef PawsValue<MincExpr*> PawsExpr;
typedef PawsValue<MincBlockExpr*> PawsBlockExpr;
typedef PawsValue<const std::vector<MincBlockExpr*>&> PawsConstBlockExprList;
typedef PawsValue<MincListExpr*> PawsListExpr;
typedef PawsValue<MincLiteralExpr*> PawsLiteralExpr;
typedef PawsValue<MincIdExpr*> PawsIdExpr;
typedef PawsValue<MincSymbol> PawsSym;
typedef PawsValue<MincScopeType*> PawsScopeType;
typedef PawsValue<MincException> PawsException;
typedef PawsValue<std::map<std::string, std::string>> PawsStringMap;

inline PawsType* const PawsBase::TYPE = new PawsType();
template <typename T> inline PawsType* const PawsValue<T>::TYPE = new PawsType();
inline PawsType* const PawsType::TYPE = new PawsType();
inline PawsType* const PawsVoid::TYPE = new PawsType();

template<typename T> void registerType(MincBlockExpr* scope, const char* name)
{
	const size_t nameLen = strlen(name);

	// Define type and add type symbol to scope
	T::TYPE->name = name;
	defineSymbol(scope, name, PawsType::TYPE, T::TYPE);

	if (T::TYPE != PawsBase::TYPE)
	{
		// Let type derive from PawsBase
		defineOpaqueInheritanceCast(scope, T::TYPE, PawsBase::TYPE);

		// Register pointer-relationship
		typedef PawsValue<typename std::add_pointer<typename T::CType>::type> ptrT;
		if (!ptrT::TYPE->name.empty())
		{
			T::TYPE->ptrType = ptrT::TYPE;
			ptrT::TYPE->valType = T::TYPE;
		}
		if constexpr (
			!std::is_same<typename T::CType, typename std::remove_pointer<typename T::CType>::type>() // If T::CType is pointer type
			&& std::is_constructible<typename std::remove_pointer<typename T::CType>::type>::value) // If *T::CType is not an incomplete type
		{
			typedef PawsValue<typename std::remove_pointer<typename T::CType>::type> valT;
			if (!valT::TYPE->name.empty())
			{
				valT::TYPE->ptrType = T::TYPE;
				T::TYPE->valType = valT::TYPE;
			}
		}

		// Define MincExpr type hierarchy
		typedef typename std::remove_pointer<typename T::CType>::type baseCType; // Pointer-less T::CType
		typedef typename std::remove_const<baseCType>::type rawCType; // Pointer-less, const-less T::CType
		if (
			std::is_same<MincExpr, rawCType>()
			|| std::is_same<MincIdExpr, rawCType>()
			|| std::is_same<MincCastExpr, rawCType>()
			|| std::is_same<MincLiteralExpr, rawCType>()
			|| std::is_same<MincPlchldExpr, rawCType>()
			|| std::is_same<MincListExpr, rawCType>()
			|| std::is_same<MincStmt, rawCType>()
			|| std::is_same<MincBlockExpr, rawCType>()
		) // If rawCType derives from MincExpr
		{
			if (std::is_const<baseCType>()) // If T::CType is a const type
			{
				if (!std::is_same<MincExpr, rawCType>()) // If T::CType != const MincExpr*
					defineOpaqueInheritanceCast(scope, T::TYPE, PawsValue<const MincExpr*>::TYPE); // Let type derive from PawsConstExpr
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

				if (!std::is_same<MincExpr, baseCType>()) // If T::CType != MincExpr*
					defineOpaqueInheritanceCast(scope, T::TYPE, PawsExpr::TYPE); // Let type derive from PawsExpr
			}
		}
	}
}

struct ReturnException
{
	const MincSymbol result;
	ReturnException(const MincSymbol& result) : result(result) {}
};

void definePawsReturnStmt(MincBlockExpr* scope, const MincObject* returnType, const char* funcName="function");

void getBlockParameterTypes(MincBlockExpr* scope, const std::vector<MincExpr*> params, std::vector<MincSymbol>& blockParams);

struct PawsKernel : public MincKernel
{
protected:
	MincBlockExpr* const expr;
	MincObject* const type;
	std::vector<MincSymbol> blockParams;
public:
	PawsKernel(MincBlockExpr* expr, MincObject* type, const std::vector<MincSymbol>& blockParams);
	MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params);
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
};

// Templated version of defineStmt2():
// defineStmt() codegen's all inputs
void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)());
template<class P0> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0))
{
	using StmtFunc = void (*)(P0);
	StmtBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs){
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get());
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	StmtBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs){
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get());
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1, class P2> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1, P2))
{
	using StmtFunc = void (*)(P0, P1, P2);
	StmtBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs){
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get(), p2->get());
	};
	defineStmt2(scope, tpltStr, codeBlock, new StmtFunc(stmtFunc));
}

// Templated version of defineExpr2():
// defineExpr() codegen's all inputs and wraps the output in a MincSymbol
template<class R> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)())
{
	using ExprFunc = R (*)();
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)();
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)()));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0))
{
	using ExprFunc = R (*)(P0);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1))
{
	using ExprFunc = R (*)(P0, P1);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2))
{
	using ExprFunc = R (*)(P0, P1, P2);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3))
{
	using ExprFunc = R (*)(P0, P1, P2, P3);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 4)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 5)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 6)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		PawsValue<P3>* p3 = (PawsValue<P3>*)codegenExpr(params[3], parentBlock).value;
		PawsValue<P4>* p4 = (PawsValue<P4>*)codegenExpr(params[4], parentBlock).value;
		PawsValue<P5>* p5 = (PawsValue<P5>*)codegenExpr(params[5], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 7)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 8)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 9)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 10)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 11)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 12)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 13)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 14)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 15)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 16)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
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
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get())));
	};
	defineExpr2(scope, tpltStr, codeBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}

// Templated version of defineExpr3():
// defineExpr() codegen's all inputs and wraps the output in a MincSymbol
void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(), PawsType* (*exprTypeFunc)());
template<class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0), PawsType* (*exprTypeFunc)(PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0);
	using ExprTypeFunc = PawsType* (*)(PawsType*);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get());
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1), PawsType* (*exprTypeFunc)(PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get());
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1, P2), PawsType* (*exprTypeFunc)(PawsType*, PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1, P2);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*, PawsType*);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
 		PawsValue<P1>* p1 = (PawsValue<P1>*)codegenExpr(params[1], parentBlock).value;
 		PawsValue<P2>* p2 = (PawsValue<P2>*)codegenExpr(params[2], parentBlock).value;
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get(), p2->get());
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		PawsType* p2 = getType(params[2], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1, p2);
	};
	defineExpr3(scope, tpltStr, codeBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

// Templated version of defineTypeCast2():
// defineTypeCast() codegen's all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineTypeCast(MincBlockExpr* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* castArgs) -> MincSymbol {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
	};
	defineTypeCast2(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

// Templated version of defineInheritanceCast2():
// defineInheritanceCast() codegen's all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineInheritanceCast(MincBlockExpr* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	ExprBlock codeBlock = [](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* castArgs) -> MincSymbol {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)parentBlock);
		PawsValue<P0>* p0 = (PawsValue<P0>*)codegenExpr(params[0], parentBlock).value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			return MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			return MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
	};
	defineInheritanceCast2(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, codeBlock, new CastFunc(exprFunc));
}

#endif