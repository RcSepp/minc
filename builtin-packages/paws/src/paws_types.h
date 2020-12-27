#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <mutex>

struct MincScopeType;

extern MincScopeType* FILE_SCOPE_TYPE;

struct PawsType;
struct PawsMetaType;

struct PawsBase : MincObject
{
	typedef void CType;
	static PawsType* const TYPE;
};

struct PawsStatic : public PawsBase
{
public:
	static PawsType* const TYPE;
};

struct PawsDynamic : public PawsBase
{
public:
	static PawsType* const TYPE;
};

struct PawsNull : public PawsBase
{
public:
	static PawsMetaType* const TYPE;
};

struct PawsType : public PawsBase
{
	static PawsMetaType* const TYPE;
	PawsType *valType, *ptrType;
	std::string name;
	int size;
protected:
	PawsType(int size=0) : valType(nullptr), ptrType(nullptr), name("UNKNOWN_PAWS_TYPE"), size(size) {}
public:
	virtual MincObject* copy(MincObject* value) = 0;
	virtual std::string toString(MincObject* value) const;
};

struct PawsMetaType : public PawsType
{
	PawsMetaType(int size) : PawsType(size) {}
	MincObject* copy(MincObject* value) { return value; }
	std::string toString(MincObject* value) const { return ((PawsType*)value)->name; }
};

template<typename T> struct PawsValue : PawsBase
{
	typedef T CType;
	struct Type : public PawsType
	{
		Type() : PawsType(sizeof(T)) {}
		MincObject* copy(MincObject* value) { return new PawsValue<T>(((PawsValue<T>*)value)->get()); }
		std::string toString(MincObject* value) const { return PawsType::toString(value); }
	};

private:
	T val;

public:
	static PawsType* const TYPE;
	PawsValue() {}
	PawsValue(const T& val) : val(val) {}
	T& get() { return val; }
	const T& get() const { return val; }
	void set(const T& val) { this->val = val; }
	MincObject* copy() { return TYPE->copy(this); }
	std::string toString() const { return TYPE->toString(this); }
};
static_assert(sizeof(PawsValue<int>) == sizeof(int), "PawsValue class should not be dynamic");
namespace std
{
	template<typename T> struct less<PawsValue<T>*>
	{
		bool operator()(const PawsValue<T>* lhs, const PawsValue<T>* rhs) const { return lhs->val < rhs->val; }
	};
}

template<> struct PawsValue<void> : public PawsBase
{
	typedef void CType;
	struct Type : public PawsType
	{
		Type() : PawsType(0) {}
		MincObject* copy(MincObject* value) { return value; }
	};
	static Type* const TYPE;
};

struct PawsTpltType : PawsType
{
private:
	static std::mutex mutex;
	static std::set<PawsTpltType*> tpltTypes;
	PawsTpltType(PawsType* baseType, PawsType* tpltType) : PawsType(baseType->size), baseType(baseType), tpltType(tpltType) {}

public:
	PawsType *const baseType, *const tpltType;
	static PawsTpltType* get(MincBlockExpr* scope, PawsType* baseType, PawsType* tpltType);
	MincObject* copy(MincObject* value) { return baseType->copy(value); }
	std::string toString(MincObject* value) const { return baseType->toString(value); }
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

struct PawsStaticBlockExpr : public PawsBlockExpr
{
public:
	static PawsType* const TYPE;
	PawsStaticBlockExpr() : PawsBlockExpr() {}
	PawsStaticBlockExpr(MincBlockExpr* val) : PawsBlockExpr(val) {}
};

inline PawsType* const PawsBase::TYPE = new PawsValue<PawsBase>::Type();
inline PawsType* const PawsStatic::TYPE = new PawsValue<PawsStatic>::Type();
inline PawsType* const PawsDynamic::TYPE = new PawsValue<PawsDynamic>::Type();
inline PawsMetaType* const PawsNull::TYPE = new PawsMetaType(0);
inline PawsMetaType* const PawsType::TYPE = new PawsMetaType(sizeof(PawsMetaType));
template <typename T> inline PawsType* const PawsValue<T>::TYPE = new PawsValue<T>::Type();
inline typename PawsVoid::Type* const PawsVoid::TYPE = new PawsVoid::Type();
inline PawsType* const PawsStaticBlockExpr::TYPE = new PawsBlockExpr::Type();

template<typename T> void registerType(MincBlockExpr* scope, const char* name, bool isStatic=false)
{
	const size_t nameLen = strlen(name);

	// Define type and add type symbol to scope
	T::TYPE->name = name;
	defineSymbol(scope, name, PawsType::TYPE, T::TYPE);

	if (T::TYPE == PawsStatic::TYPE || T::TYPE == PawsDynamic::TYPE)
	{
		// Let type derive from PawsBase
		defineOpaqueInheritanceCast(scope, T::TYPE, PawsBase::TYPE);
	}
	else if (T::TYPE != PawsBase::TYPE)
	{
		if (!isInstance(scope, T::TYPE, PawsBase::TYPE)) // Do not overwrite static-ness when registering derived types
		{
			// Let type derive from PawsStatic or PawsDynamic
			defineOpaqueInheritanceCast(scope, T::TYPE, isStatic ? PawsStatic::TYPE : PawsDynamic::TYPE);
		}

		if (isStatic)
			return; // Pointer and MincExpr type hierarchy not supported for static types

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
extern MincObject PAWS_RETURN_TYPE, PAWS_AWAIT_TYPE;

void definePawsReturnStmt(MincBlockExpr* scope, const MincObject* returnType, const char* funcName="function");

void getBlockParameterTypes(MincBlockExpr* scope, const std::vector<MincExpr*> params, std::vector<MincSymbol>& blockParams);

struct PawsKernel : public MincKernel
{
private:
	PawsKernel(MincBlockExpr* body, MincObject* type);
protected:
	MincBlockExpr* const body;
	MincObject* const type;
	std::vector<MincSymbol> blockParams;
public:
	enum Phase { INIT, BUILD, RUN } phase, activePhase;
	MincBlockExpr *instance, *callerScope;

private:
	bool hasBuildResult;
	MincSymbol buildResult;

public:
	PawsKernel(MincBlockExpr* body, MincObject* type, MincBuildtime& buildtime, const std::vector<MincSymbol>& blockParams);
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params);
	void dispose(MincKernel* kernel);
	bool run(MincRuntime& runtime, std::vector<MincExpr*>& params);
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
};

// Templated version of defineStmt6():
// defineStmt() calls run() on all inputs
void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)());
template<class P0> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0))
{
	using StmtFunc = void (*)(P0);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		(*(StmtFunc*)stmtArgs)(p0->get());
		return false;
	};
	defineStmt6(scope, tpltStr, buildBlock, runBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get());
		return false;
	};
	defineStmt6(scope, tpltStr, buildBlock, runBlock, new StmtFunc(stmtFunc));
}
template<class P0, class P1, class P2> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1, P2))
{
	using StmtFunc = void (*)(P0, P1, P2);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		(*(StmtFunc*)stmtArgs)(p0->get(), p1->get(), p2->get());
		return false;
	};
	defineStmt6(scope, tpltStr, buildBlock, runBlock, new StmtFunc(stmtFunc));
}

// Templated version of defineExpr9():
// defineExpr() calls run() on all inputs and wraps the output in a MincSymbol
template<class R> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)())
{
	using ExprFunc = R (*)();
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
		if (params.size() != 0)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)();
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)()));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0))
{
	using ExprFunc = R (*)(P0);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1))
{
	using ExprFunc = R (*)(P0, P1);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2))
{
	using ExprFunc = R (*)(P0, P1, P2);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3))
{
	using ExprFunc = R (*)(P0, P1, P2, P3);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 4)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 5)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 6)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 7)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 8)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 9)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 10)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 11)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 12)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
		buildExpr(params[11], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if (runExpr(params[11], runtime)) return true;
		PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 13)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
		buildExpr(params[11], buildtime);
		buildExpr(params[12], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if (runExpr(params[11], runtime)) return true;
		PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result.value;
		if (runExpr(params[12], runtime)) return true;
		PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 14)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
		buildExpr(params[11], buildtime);
		buildExpr(params[12], buildtime);
		buildExpr(params[13], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if (runExpr(params[11], runtime)) return true;
		PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result.value;
		if (runExpr(params[12], runtime)) return true;
		PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result.value;
		if (runExpr(params[13], runtime)) return true;
		PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 15)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
		buildExpr(params[11], buildtime);
		buildExpr(params[12], buildtime);
		buildExpr(params[13], buildtime);
		buildExpr(params[14], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if (runExpr(params[11], runtime)) return true;
		PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result.value;
		if (runExpr(params[12], runtime)) return true;
		PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result.value;
		if (runExpr(params[13], runtime)) return true;
		PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result.value;
		if (runExpr(params[14], runtime)) return true;
		PawsValue<P14>* p14 = (PawsValue<P14>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 16)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
		buildExpr(params[3], buildtime);
		buildExpr(params[4], buildtime);
		buildExpr(params[5], buildtime);
		buildExpr(params[6], buildtime);
		buildExpr(params[7], buildtime);
		buildExpr(params[8], buildtime);
		buildExpr(params[9], buildtime);
		buildExpr(params[10], buildtime);
		buildExpr(params[11], buildtime);
		buildExpr(params[12], buildtime);
		buildExpr(params[13], buildtime);
		buildExpr(params[14], buildtime);
		buildExpr(params[15], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		if (runExpr(params[3], runtime)) return true;
		PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
		if (runExpr(params[4], runtime)) return true;
		PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result.value;
		if (runExpr(params[5], runtime)) return true;
		PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result.value;
		if (runExpr(params[6], runtime)) return true;
		PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result.value;
		if (runExpr(params[7], runtime)) return true;
		PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result.value;
		if (runExpr(params[8], runtime)) return true;
		PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result.value;
		if (runExpr(params[9], runtime)) return true;
		PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result.value;
		if (runExpr(params[10], runtime)) return true;
		PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result.value;
		if (runExpr(params[11], runtime)) return true;
		PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result.value;
		if (runExpr(params[12], runtime)) return true;
		PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result.value;
		if (runExpr(params[13], runtime)) return true;
		PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result.value;
		if (runExpr(params[14], runtime)) return true;
		PawsValue<P14>* p14 = (PawsValue<P14>*)runtime.result.value;
		if (runExpr(params[15], runtime)) return true;
		PawsValue<P15>* p15 = (PawsValue<P15>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(ExprFunc*)exprArgs)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get())));
		return false;
	};
	defineExpr9(scope, tpltStr, buildBlock, runBlock, PawsValue<R>::TYPE, new ExprFunc(exprFunc));
}

// Templated version of defineExpr10():
// defineExpr() calls run() on all inputs and wraps the output in a MincSymbol
void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(), PawsType* (*exprTypeFunc)());
template<class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0), PawsType* (*exprTypeFunc)(PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0);
	using ExprTypeFunc = PawsType* (*)(PawsType*);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		runtime.result = ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get());
		return false;
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0);
	};
	defineExpr10(scope, tpltStr, buildBlock, runBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1), PawsType* (*exprTypeFunc)(PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 2)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
 		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
		runtime.result = ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get());
		return false;
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1);
	};
	defineExpr10(scope, tpltStr, buildBlock, runBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1, P2), PawsType* (*exprTypeFunc)(PawsType*, PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1, P2);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*, PawsType*);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
		if (params.size() != 3)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
		buildExpr(params[1], buildtime);
		buildExpr(params[2], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
 		if (runExpr(params[1], runtime)) return true;
		PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
 		if (runExpr(params[2], runtime)) return true;
		PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
		runtime.result = ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->first(p0->get(), p1->get(), p2->get());
		return false;
	};
	ExprTypeBlock typeCodeBlock = [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
		PawsType* p0 = getType(params[0], parentBlock);
		PawsType* p1 = getType(params[1], parentBlock);
		PawsType* p2 = getType(params[2], parentBlock);
		return ((std::pair<ExprFunc, ExprTypeFunc>*)exprArgs)->second(p0, p1, p2);
	};
	defineExpr10(scope, tpltStr, buildBlock, runBlock, typeCodeBlock, new std::pair<ExprFunc, ExprTypeFunc>(exprFunc, exprTypeFunc));
}

// Templated version of defineTypeCast9():
// defineTypeCast() calls run() on all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineTypeCast(MincBlockExpr* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* castArgs) {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* castArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
		return false;
	};
	defineTypeCast9(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, buildBlock, runBlock, new CastFunc(exprFunc));
}

// Templated version of defineInheritanceCast9():
// defineInheritanceCast() calls run() on all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineInheritanceCast(MincBlockExpr* scope, R (*exprFunc)(P0))
{
	using CastFunc = R (*)(P0);
	BuildBlock buildBlock = [](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* castArgs) {
		if (params.size() != 1)
			raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
		buildExpr(params[0], buildtime);
	};
	RunBlock runBlock = [](MincRuntime& runtime, std::vector<MincExpr*>& params, void* castArgs) -> bool {
		if (runExpr(params[0], runtime)) return true;
		PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
		if constexpr (std::is_void<R>::value)
		{
			(*(CastFunc*)castArgs)(p0->get());
			runtime.result = MincSymbol(PawsValue<R>::TYPE, nullptr);
		}
		else
			runtime.result = MincSymbol(PawsValue<R>::TYPE, new PawsValue<R>((*(CastFunc*)castArgs)(p0->get())));
		return false;
	};
	defineInheritanceCast9(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, buildBlock, runBlock, new CastFunc(exprFunc));
}

#endif