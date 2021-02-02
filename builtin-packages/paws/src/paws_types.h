#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <mutex>
#include <new>

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
	virtual void copyTo(MincObject* src, MincObject* dest) = 0;
	virtual void copyToNew(MincObject* src, MincObject* dest) = 0;
	virtual MincObject* alloc() = 0;
	virtual MincObject* allocTo(MincObject* memory) = 0;
	virtual std::string toString(MincObject* value) const;
};

struct PawsMetaType : public PawsType
{
	PawsMetaType(int size) : PawsType(size) {}
	MincObject* copy(MincObject* value) { return value; }
	void copyTo(MincObject* src, MincObject* dest) {}
	void copyToNew(MincObject* src, MincObject* dest) {}
	MincObject* alloc() { return nullptr; }
	MincObject* allocTo(MincObject* memory) { return nullptr; }
	std::string toString(MincObject* value) const { return ((PawsType*)value)->name; }
};

template<typename T> struct PawsValue : PawsBase
{
	typedef T CType;
	struct Type : public PawsType
	{
		Type() : PawsType(sizeof(T)) {}
		MincObject* copy(MincObject* value) { return new PawsValue<T>(((PawsValue<T>*)value)->get()); }
		void copyTo(MincObject* src, MincObject* dest)
		{
			if constexpr (std::is_assignable<T&, T>())
			{
				auto foo = ((PawsValue<T>*)src)->get();
				((PawsValue<T>*)dest)->set(foo);
			}
		}
		void copyToNew(MincObject* src, MincObject* dest)
		{
			new(dest) PawsValue<T>(((PawsValue<T>*)src)->get());
		}
		MincObject* alloc()
		{
			if constexpr (std::is_default_constructible<T>())
				return new PawsValue<T>(T());
			else
				return (PawsValue<T>*)new unsigned char[size];
		}
		MincObject* allocTo(MincObject* memory)
		{
			if constexpr (std::is_default_constructible<T>())
				return new(memory) PawsValue<T>(T());
			else
				return (PawsValue<T>*)new(memory) unsigned char[size];
		}
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
	void copyTo(MincObject* dest) { return TYPE->copyTo(this, dest); }
	void copyToNew(MincObject* src, MincObject* dest) { return TYPE->copyToNew(src, dest); }
	MincObject* alloc() { return TYPE->alloc(); }
	MincObject* allocTo(MincObject* memory) { return TYPE->allocTo(memory); }
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
		void copyTo(MincObject* src, MincObject* dest) {}
		void copyToNew(MincObject* src, MincObject* dest) {}
		MincObject* alloc() { return nullptr; }
	MincObject* allocTo(MincObject* memory) { return nullptr; }
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
	void copyTo(MincObject* src, MincObject* dest) { return baseType->copyTo(src, dest); }
	void copyToNew(MincObject* src, MincObject* dest) { return baseType->copyToNew(src, dest); }
	MincObject* alloc() { return baseType->alloc(); }
	MincObject* allocTo(MincObject* memory) { return baseType->allocTo(memory); }
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
	MincBlockExpr *callerScope;

private:
	bool hasBuildResult;
	MincObject* buildResult;

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
	class StmtKernel : public MincKernel
	{
		const StmtFunc* const stmtFunc;
	public:
		StmtKernel(const StmtFunc* stmtFunc) : stmtFunc(stmtFunc) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 1)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			(*stmtFunc)(p0->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	defineStmt4(scope, tpltStr, new StmtKernel(new StmtFunc(stmtFunc)));
}
template<class P0, class P1> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1))
{
	using StmtFunc = void (*)(P0, P1);
	class StmtKernel : public MincKernel
	{
		const StmtFunc* const stmtFunc;
	public:
		StmtKernel(const StmtFunc* stmtFunc) : stmtFunc(stmtFunc) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 2)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			(*stmtFunc)(p0->get(), p1->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	defineStmt4(scope, tpltStr, new StmtKernel(new StmtFunc(stmtFunc)));
}
template<class P0, class P1, class P2> void defineStmt(MincBlockExpr* scope, const char* tpltStr, void (*stmtFunc)(P0, P1, P2))
{
	using StmtFunc = void (*)(P0, P1, P2);
	class StmtKernel : public MincKernel
	{
		const StmtFunc* const stmtFunc;
	public:
		StmtKernel(const StmtFunc* stmtFunc) : stmtFunc(stmtFunc) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 3)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			if (runExpr(params[2], runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
			(*stmtFunc)(p0->get(), p1->get(), p2->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	defineStmt4(scope, tpltStr, new StmtKernel(new StmtFunc(stmtFunc)));
}

// Templated version of defineExpr9():
// defineExpr() calls run() on all inputs and wraps the output in a MincSymbol
template<class R> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)())
{
	using ExprFunc = R (*)();
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 0)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)();
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0))
{
	using ExprFunc = R (*)(P0);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 1)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1))
{
	using ExprFunc = R (*)(P0, P1);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 2)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2))
{
	using ExprFunc = R (*)(P0, P1, P2);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 3)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			if (runExpr(params[2], runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3))
{
	using ExprFunc = R (*)(P0, P1, P2, P3);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 4)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			buildExpr(params[3], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			if (runExpr(params[2], runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
			if (runExpr(params[3], runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 5)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			buildExpr(params[3], buildtime);
			buildExpr(params[4], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 6)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			buildExpr(params[3], buildtime);
			buildExpr(params[4], buildtime);
			buildExpr(params[5], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 7)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			buildExpr(params[3], buildtime);
			buildExpr(params[4], buildtime);
			buildExpr(params[5], buildtime);
			buildExpr(params[6], buildtime);
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}
template<class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9, class P10, class P11, class P12, class P13, class P14, class P15> void defineExpr(MincBlockExpr* scope, const char* tpltStr, R (*exprFunc)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15))
{
	using ExprFunc = R (*)(P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
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
			return new ExprKernel(exprFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
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
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc)));
}

// Templated version of defineExpr10():
// defineExpr() calls run() on all inputs and wraps the output in a MincSymbol
void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(), PawsType* (*exprTypeFunc)());
template<class P0> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0), PawsType* (*exprTypeFunc)(PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0);
	using ExprTypeFunc = PawsType* (*)(PawsType*);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const ExprFunc* const exprTypeFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const ExprFunc* exprTypeFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), exprTypeFunc(exprTypeFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 1)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, allocAnonymousStackSymbol(buildtime.parentBlock, type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			runtime.result = (*exprFunc)(p0->get());
			MincObject* resultValue = getStackSymbol(runtime.parentBlock, runtime, result);
			((PawsType*)runtime.result.type)->copyToNew(runtime.result.value, resultValue);
			runtime.result.value = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = getType(params[0], parentBlock);
			return (*exprTypeFunc)(p0);
		}
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
}
template<class P0, class P1> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1), PawsType* (*exprTypeFunc)(PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const ExprTypeFunc* const exprTypeFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const ExprTypeFunc* exprTypeFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), exprTypeFunc(exprTypeFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 2)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, allocAnonymousStackSymbol(buildtime.parentBlock, type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			runtime.result = (*exprFunc)(p0->get(), p1->get());
			MincObject* resultValue = getStackSymbol(runtime.parentBlock, runtime, result);
			((PawsType*)runtime.result.type)->copyToNew(runtime.result.value, resultValue);
			runtime.result.value = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = getType(params[0], parentBlock);
			PawsType* p1 = getType(params[1], parentBlock);
			return (*exprTypeFunc)(p0, p1);
		}
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
}
template<class R, class P0, class P1, class P2> void defineExpr(MincBlockExpr* scope, const char* tpltStr, MincSymbol (*exprFunc)(P0, P1, P2), PawsType* (*exprTypeFunc)(PawsType*, PawsType*, PawsType*))
{
	using ExprFunc = MincSymbol (*)(P0, P1, P2);
	using ExprTypeFunc = PawsType* (*)(PawsType*, PawsType*, PawsType*);
	class ExprKernel : public MincKernel
	{
		const ExprFunc* const exprFunc;
		const ExprTypeFunc* const exprTypeFunc;
		const MincStackSymbol* const result;
	public:
		ExprKernel(const ExprFunc* exprFunc, const ExprTypeFunc* exprTypeFunc, const MincStackSymbol* result=nullptr) : exprFunc(exprFunc), exprTypeFunc(exprTypeFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 3)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
			buildExpr(params[2], buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, allocAnonymousStackSymbol(buildtime.parentBlock, type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			if (runExpr(params[1], runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result.value;
			if (runExpr(params[2], runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result.value;
			runtime.result = (*exprFunc)(p0->get(), p1->get(), p2->get());
			MincObject* resultValue = getStackSymbol(runtime.parentBlock, runtime, result);
			((PawsType*)runtime.result.type)->copyToNew(runtime.result.value, resultValue);
			runtime.result.value = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = getType(params[0], parentBlock);
			PawsType* p1 = getType(params[1], parentBlock);
			PawsType* p2 = getType(params[2], parentBlock);
			return (*exprTypeFunc)(p0, p1, p2);
		}
	};
	defineExpr6(scope, tpltStr, new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
}

// Templated version of defineTypeCast9():
// defineTypeCast() calls run() on all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineTypeCast(MincBlockExpr* scope, R (*castFunc)(P0))
{
	using CastFunc = R (*)(P0);
	class CastKernel : public MincKernel
	{
		const CastFunc* const castFunc;
		const MincStackSymbol* const result;
	public:
		CastKernel(const CastFunc* castFunc, const MincStackSymbol* result=nullptr) : castFunc(castFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 1)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			return new CastKernel(castFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*castFunc)(p0->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*castFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineTypeCast3(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, new CastKernel(new CastFunc(castFunc)));
}

// Templated version of defineInheritanceCast9():
// defineInheritanceCast() calls run() on all inputs and wraps the output in a MincSymbol
template<class R, class P0> void defineInheritanceCast(MincBlockExpr* scope, R (*castFunc)(P0))
{
	using CastFunc = R (*)(P0);
	class CastKernel : public MincKernel
	{
		const CastFunc* const castFunc;
		const MincStackSymbol* const result;
	public:
		CastKernel(const CastFunc* castFunc, const MincStackSymbol* result=nullptr) : castFunc(castFunc), result(result) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (params.size() != 1)
				raiseCompileError("parameter index out of bounds", (MincExpr*)buildtime.parentBlock);
			buildExpr(params[0], buildtime);
			return new CastKernel(castFunc, allocAnonymousStackSymbol(buildtime.parentBlock, PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result.value;
			runtime.result.type = PawsValue<R>::TYPE;
			if constexpr (std::is_void<R>::value)
			{
				(*castFunc)(p0->get());
				runtime.result.value = nullptr;
			}
			else
				new(runtime.result.value = getStackSymbol(runtime.parentBlock, runtime, result))
					PawsValue<R>((*castFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	defineInheritanceCast3(scope, PawsValue<P0>::TYPE, PawsValue<R>::TYPE, new CastKernel(new CastFunc(castFunc)));
}

#endif