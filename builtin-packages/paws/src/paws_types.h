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
	scope->defineSymbol(name, PawsType::TYPE, T::TYPE);

	if (T::TYPE == PawsStatic::TYPE || T::TYPE == PawsDynamic::TYPE)
	{
		// Let type derive from PawsBase
		scope->defineCast(new InheritanceCast(T::TYPE, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE)));
	}
	else if (T::TYPE != PawsBase::TYPE)
	{
		if (!scope->isInstance(T::TYPE, PawsBase::TYPE)) // Do not overwrite static-ness when registering derived types
		{
			// Let type derive from PawsStatic or PawsDynamic
			PawsType* const base = isStatic ? PawsStatic::TYPE : PawsDynamic::TYPE;
			scope->defineCast(new InheritanceCast(T::TYPE, base, new MincOpaqueCastKernel(base)));
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
				{
					// Let type derive from PawsConstExpr
					PawsType* const base = PawsValue<const MincExpr*>::TYPE;
					scope->defineCast(new InheritanceCast(T::TYPE, base, new MincOpaqueCastKernel(base)));
				}
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
				scope->defineCast(new InheritanceCast(T::TYPE, constT::TYPE, new MincOpaqueCastKernel(constT::TYPE)));

				if (!std::is_same<MincExpr, baseCType>()) // If T::CType != MincExpr*
				{
					// Let type derive from PawsExpr
					scope->defineCast(new InheritanceCast(T::TYPE, PawsExpr::TYPE, new MincOpaqueCastKernel(PawsExpr::TYPE)));
				}
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
	enum Phase { INIT, BUILD, RUN } phase;
	Phase activePhase;
	const MincBlockExpr *callerScope;

private:
	bool hasBuildResult;
	MincObject* buildResult;

public:
	PawsKernel(MincBlockExpr* body, MincObject* type, MincBuildtime& buildtime, const std::vector<MincSymbol>& blockParams);
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params);
	void dispose(MincKernel* kernel);
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			(*stmtFunc)(p0->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	scope->defineStmt(MincBlockExpr::parseCTplt(tpltStr), new StmtKernel(new StmtFunc(stmtFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			(*stmtFunc)(p0->get(), p1->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	scope->defineStmt(MincBlockExpr::parseCTplt(tpltStr), new StmtKernel(new StmtFunc(stmtFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			return this;
		}
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			(*stmtFunc)(p0->get(), p1->get(), p2->get());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	scope->defineStmt(MincBlockExpr::parseCTplt(tpltStr), new StmtKernel(new StmtFunc(stmtFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)();
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)());
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			params[11]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if (params[11]->run(runtime)) return true;
			PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			params[11]->build(buildtime);
			params[12]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if (params[11]->run(runtime)) return true;
			PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result;
			if (params[12]->run(runtime)) return true;
			PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			params[11]->build(buildtime);
			params[12]->build(buildtime);
			params[13]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if (params[11]->run(runtime)) return true;
			PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result;
			if (params[12]->run(runtime)) return true;
			PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result;
			if (params[13]->run(runtime)) return true;
			PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			params[11]->build(buildtime);
			params[12]->build(buildtime);
			params[13]->build(buildtime);
			params[14]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if (params[11]->run(runtime)) return true;
			PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result;
			if (params[12]->run(runtime)) return true;
			PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result;
			if (params[13]->run(runtime)) return true;
			PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result;
			if (params[14]->run(runtime)) return true;
			PawsValue<P14>* p14 = (PawsValue<P14>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			params[3]->build(buildtime);
			params[4]->build(buildtime);
			params[5]->build(buildtime);
			params[6]->build(buildtime);
			params[7]->build(buildtime);
			params[8]->build(buildtime);
			params[9]->build(buildtime);
			params[10]->build(buildtime);
			params[11]->build(buildtime);
			params[12]->build(buildtime);
			params[13]->build(buildtime);
			params[14]->build(buildtime);
			params[15]->build(buildtime);
			return new ExprKernel(exprFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			if (params[3]->run(runtime)) return true;
			PawsValue<P3>* p3 = (PawsValue<P3>*)runtime.result;
			if (params[4]->run(runtime)) return true;
			PawsValue<P4>* p4 = (PawsValue<P4>*)runtime.result;
			if (params[5]->run(runtime)) return true;
			PawsValue<P5>* p5 = (PawsValue<P5>*)runtime.result;
			if (params[6]->run(runtime)) return true;
			PawsValue<P6>* p6 = (PawsValue<P6>*)runtime.result;
			if (params[7]->run(runtime)) return true;
			PawsValue<P7>* p7 = (PawsValue<P7>*)runtime.result;
			if (params[8]->run(runtime)) return true;
			PawsValue<P8>* p8 = (PawsValue<P8>*)runtime.result;
			if (params[9]->run(runtime)) return true;
			PawsValue<P9>* p9 = (PawsValue<P9>*)runtime.result;
			if (params[10]->run(runtime)) return true;
			PawsValue<P10>* p10 = (PawsValue<P10>*)runtime.result;
			if (params[11]->run(runtime)) return true;
			PawsValue<P11>* p11 = (PawsValue<P11>*)runtime.result;
			if (params[12]->run(runtime)) return true;
			PawsValue<P12>* p12 = (PawsValue<P12>*)runtime.result;
			if (params[13]->run(runtime)) return true;
			PawsValue<P13>* p13 = (PawsValue<P13>*)runtime.result;
			if (params[14]->run(runtime)) return true;
			PawsValue<P14>* p14 = (PawsValue<P14>*)runtime.result;
			if (params[15]->run(runtime)) return true;
			PawsValue<P15>* p15 = (PawsValue<P15>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*exprFunc)(p0->get(), p1->get(), p2->get(), p3->get(), p4->get(), p5->get(), p6->get(), p7->get(), p8->get(), p9->get(), p10->get(), p11->get(), p12->get(), p13->get(), p14->get(), p15->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, buildtime.parentBlock->allocStackSymbol(type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			runtime.result = (*exprFunc)(p0->get());
			MincObject* resultValue = runtime.parentBlock->getStackSymbol(runtime, result);
			((PawsType*)result->type)->copyToNew(runtime.result, resultValue);
			runtime.result = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = params[0]->getType(parentBlock);
			return (*exprTypeFunc)(p0);
		}
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, buildtime.parentBlock->allocStackSymbol(type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			runtime.result = (*exprFunc)(p0->get(), p1->get());
			MincObject* resultValue = runtime.parentBlock->getStackSymbol(runtime, result);
			((PawsType*)result->type)->copyToNew(runtime.result, resultValue);
			runtime.result = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = params[0]->getType(parentBlock);
			PawsType* p1 = params[1]->getType(parentBlock);
			return (*exprTypeFunc)(p0, p1);
		}
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			PawsType* type = getType(buildtime.parentBlock, params);
			return new ExprKernel(exprFunc, exprTypeFunc, buildtime.parentBlock->allocStackSymbol(type, type->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if (params[1]->run(runtime)) return true;
			PawsValue<P1>* p1 = (PawsValue<P1>*)runtime.result;
			if (params[2]->run(runtime)) return true;
			PawsValue<P2>* p2 = (PawsValue<P2>*)runtime.result;
			runtime.result = (*exprFunc)(p0->get(), p1->get(), p2->get());
			MincObject* resultValue = runtime.parentBlock->getStackSymbol(runtime, result);
			((PawsType*)result->type)->copyToNew(runtime.result, resultValue);
			runtime.result = resultValue;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* p0 = params[0]->getType(parentBlock);
			PawsType* p1 = params[1]->getType(parentBlock);
			PawsType* p2 = params[2]->getType(parentBlock);
			return (*exprTypeFunc)(p0, p1, p2);
		}
	};
	scope->defineExpr(MincBlockExpr::parseCTplt(tpltStr)[0], new ExprKernel(new ExprFunc(exprFunc), new ExprTypeFunc(exprTypeFunc)));
}

// Templated version of defineCast(TypeCast()):
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			return new CastKernel(castFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*castFunc)(p0->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*castFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineCast(new TypeCast(PawsValue<P0>::TYPE, PawsValue<R>::TYPE, new CastKernel(new CastFunc(castFunc))));
}

// Templated version of defineCast(InheritanceCast()):
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
				throw CompileError(buildtime.parentBlock, buildtime.parentBlock->loc, "parameter index out of bounds");
			params[0]->build(buildtime);
			return new CastKernel(castFunc, buildtime.parentBlock->allocStackSymbol(PawsValue<R>::TYPE, PawsValue<R>::TYPE->size));
		}
		void dispose(MincKernel* kernel) { delete kernel; }
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime)) return true;
			PawsValue<P0>* p0 = (PawsValue<P0>*)runtime.result;
			if constexpr (std::is_void<R>::value)
			{
				(*castFunc)(p0->get());
				runtime.result = nullptr;
			}
			else
				new(runtime.result = runtime.parentBlock->getStackSymbol(runtime, result))
					PawsValue<R>((*castFunc)(p0->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return PawsValue<R>::TYPE; }
	};
	scope->defineCast(new InheritanceCast(PawsValue<P0>::TYPE, PawsValue<R>::TYPE, new CastKernel(new CastFunc(castFunc))));
}

#endif