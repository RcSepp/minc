#ifndef __PAWS_TYPES_H
#define __PAWS_TYPES_H

#include<set>

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
			defineOpaqueCast(getRootScope(), t, PawsOpaqueType<0>::TYPE); // Let baseType<tpltType> derive from PawsBase
			defineOpaqueCast(getRootScope(), t, baseType); // Let baseType<tpltType> derive from baseType
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

struct StmtMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<StmtMap> PawsStmtMap;

struct ExprMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<ExprMap> PawsExprMap;

struct SymbolMap { BlockExprAST* block; operator BlockExprAST*() const { return block; } };
typedef PawsType<SymbolMap> PawsSymbolMap;

#endif