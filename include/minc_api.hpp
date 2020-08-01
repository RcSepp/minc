#ifndef __MINC_API_HPP
#define __MINC_API_HPP

#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "minc_types.h"

typedef std::vector<MincExpr*>::const_iterator MincExprIter;

extern "C"
{
	MincBlockExpr* getRootScope();
	MincBlockExpr* getFileScope();
	const MincSymbol& getVoid();
	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock);
	void raiseCompileError(const char* msg, const MincExpr* loc);
	void registerStepEventListener(StepEvent listener, void* eventArgs);
	void deregisterStepEventListener(StepEvent listener);
}

class MincExpr
{
public:
	MincLocation loc;
	enum ExprType {
		STMT, LIST, STOP, LITERAL, ID, CAST, PLCHLD, PARAM, ELLIPSIS, ARGOP, ENCOP, TEROP, BINOP, VARBINOP, PREOP, POSTOP, BLOCK,
		NUM_EXPR_TYPES
	};
	const ExprType exprtype;

	// Resolved state
	MincKernel* resolvedKernel;
	std::vector<MincExpr*> resolvedParams;

	MincExpr(const MincLocation& loc, ExprType exprtype);
	virtual ~MincExpr();
	virtual MincSymbol codegen(MincBlockExpr* parentBlock);
	virtual MincObject* getType(const MincBlockExpr* parentBlock) const;
	virtual bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const = 0;
	virtual void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const = 0;
	inline bool isResolved() { return this->resolvedKernel != nullptr; }
	virtual void resolve(const MincBlockExpr* block);
	virtual void forget();
	virtual std::string str() const = 0;
	virtual std::string shortStr() const;
	virtual int comp(const MincExpr* other) const;
	virtual MincExpr* clone() const = 0;
};
bool operator<(const MincExpr& left, const MincExpr& right);

namespace std
{
	template<> struct less<const MincExpr*>
	{
		bool operator()(const MincExpr* lhs, const MincExpr* rhs) const { return lhs->comp(rhs) < 0; }
	};
}

class ResolvingMincExprIter
{
	const MincBlockExpr* resolveScope;
	const std::vector<MincExpr*>* exprs;
	MincExprIter current;

public:
	ResolvingMincExprIter() : resolveScope(nullptr), exprs(nullptr) {}
	ResolvingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs)
		: resolveScope(resolveScope), exprs(exprs)
	{
		assert(resolveScope != nullptr && exprs != nullptr);
		current = exprs->begin();
	}
	ResolvingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>* exprs, MincExprIter current)
		: resolveScope(resolveScope), exprs(exprs), current(current)
	{
		assert(resolveScope != nullptr && exprs != nullptr);
	}
	ResolvingMincExprIter(const ResolvingMincExprIter& other) = default;
	ResolvingMincExprIter& operator=(const ResolvingMincExprIter& other) = default;
	inline bool done() { return current == exprs->end(); }
	inline MincExpr* operator*()
	{
		MincExpr* const expr = *current;
		expr->resolve(resolveScope);
		return expr;
	}
	inline MincExpr* operator[](int i)
	{
		MincExpr* const expr = *(current + i);
		expr->resolve(resolveScope);
		return expr;
	}
	inline size_t operator-(const ResolvingMincExprIter& other) const { return exprs == nullptr || other.exprs == nullptr ? 0 : current - other.current; }
	inline ResolvingMincExprIter operator+(int n) const { return ResolvingMincExprIter(resolveScope, exprs, current + n); }
	inline ResolvingMincExprIter operator++(int) { return ResolvingMincExprIter(resolveScope, exprs, current++); }
	inline ResolvingMincExprIter& operator++() { ++current; return *this; }
	inline MincExprIter iter() const { return current; }
};

class MincListExpr : public MincExpr
{
public:
	char separator;
	std::vector<MincExpr*> exprs;
	MincListExpr(char separator);
	MincListExpr(char separator, std::vector<MincExpr*> exprs);
	MincSymbol codegen(MincBlockExpr* parentBlock);
	bool match(const MincBlockExpr* block, const MincExpr* exprs, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* exprs, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	std::vector<MincExpr*>::iterator begin();
	std::vector<MincExpr*>::const_iterator cbegin() const;
	std::vector<MincExpr*>::iterator end();
	std::vector<MincExpr*>::const_iterator cend() const;
	size_t size() const;
	MincExpr* at(size_t index);
	const MincExpr* at(size_t index) const;
	MincExpr* operator[](size_t index);
	const MincExpr* operator[](size_t index) const;
	void push_back(MincExpr* expr);
	MincExpr* clone() const;
};
std::vector<MincExpr*>::iterator begin(MincListExpr& exprs);
std::vector<MincExpr*>::iterator begin(MincListExpr* exprs);
std::vector<MincExpr*>::iterator end(MincListExpr& exprs);
std::vector<MincExpr*>::iterator end(MincListExpr* exprs);

namespace std
{
	template<> struct less<const MincListExpr*>
	{
		bool operator()(const MincListExpr* lhs, const MincListExpr* rhs) const { return lhs->comp(rhs) < 0; }
	};
}

class MincStatementRegister
{
private:
	std::map<const MincListExpr*, MincKernel*> stmtreg;
	std::array<std::map<const MincExpr*, MincKernel*>, MincExpr::NUM_EXPR_TYPES> exprreg;
	MincKernel *antiStmt, *antiExpr;
public:
	MincStatementRegister() : antiStmt(nullptr), antiExpr(nullptr) {}
	void defineStmt(const MincListExpr* tplt, MincKernel* stmt);
	std::pair<const MincListExpr*, MincKernel*> lookupStmt(const MincBlockExpr* block, ResolvingMincExprIter stmt, ResolvingMincExprIter& stmtEnd, MatchScore& score) const;
	void lookupStmtCandidates(const MincBlockExpr* block, const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk) const;
	void defineDefaultStmt(MincKernel* stmt);

	void defineExpr(const MincExpr* tplt, MincKernel* expr);
	std::pair<const MincExpr*, MincKernel*> lookupExpr(const MincBlockExpr* block, MincExpr* expr, MatchScore& bestScore) const;
	void lookupExprCandidates(const MincBlockExpr* block, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk) const;
	void defineDefaultExpr(MincKernel* expr) { antiExpr = expr; }
};

struct InheritanceCast : public MincCast
{
	InheritanceCast(MincObject* fromType, MincObject* toType, MincKernel* kernel);
	int getCost() const;
	MincCast* derive() const;
};

struct TypeCast : public MincCast
{
	TypeCast(MincObject* fromType, MincObject* toType, MincKernel* kernel);
	int getCost() const;
	MincCast* derive() const;
};

class CastRegister
{
private:
	MincBlockExpr* const block;
	std::map<std::pair<MincObject*, MincObject*>, MincCast*> casts;
	std::multimap<MincObject*, MincCast*> fwdCasts, bwdCasts;
public:
	CastRegister(MincBlockExpr* block);
	void defineDirectCast(MincCast* cast);
	void defineIndirectCast(const CastRegister& castreg, MincCast* cast);
	const MincCast* lookupCast(MincObject* fromType, MincObject* toType) const;
	bool isInstance(MincObject* derivedType, MincObject* baseType) const;
	void listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const MincCast* cast)> cbk) const;
};

class MincStmt : public MincExpr
{
public:
	MincExprIter begin, end;
	std::vector<MincExpr*> resolvedExprs;
	MincExprIter sourceExprPtr;

	MincStmt(MincExprIter exprBegin, MincExprIter exprEnd, MincKernel* kernel);
	MincStmt();
	~MincStmt();
	MincSymbol codegen(MincBlockExpr* parentBlock);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincBlockExpr : public MincExpr
{
private:
	MincStatementRegister stmtreg;
	std::map<std::string, MincSymbol> symbolMap;
	std::map<const MincObject*, std::string> symbolNameMap;
	CastRegister castreg;
	std::vector<MincStmt>* resolvedStmts;
	bool ownesResolvedStmts;

	MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs, std::vector<MincStmt>* resolvedStmts);

public:
	MincBlockExpr* parent;
	std::vector<MincBlockExpr*> references;
	std::vector<MincExpr*>* exprs;
	size_t stmtIdx;
	MincScopeType* scopeType;
	std::vector<MincSymbol> blockParams;
	std::vector<MincSymbol*> resultCache;
	size_t resultCacheIdx;
	bool isBlockSuspended, isStmtSuspended, isExprSuspended;
	bool isBusy;

	// Meta data
	std::string name;
	void *user, *userType;

	MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs);
	~MincBlockExpr();
	void defineStmt(const std::vector<MincExpr*>& tplt, MincKernel* stmt);
	void defineStmt(const std::vector<MincExpr*>& tplt, std::function<void(MincBlockExpr*, std::vector<MincExpr*>&)> code);
	bool lookupStmt(MincExprIter beginExpr, MincStmt& stmt) const;
	void lookupStmtCandidates(const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const;
	std::pair<const MincListExpr*, MincKernel*> lookupStmt(ResolvingMincExprIter stmt, ResolvingMincExprIter& bestStmtEnd, MatchScore& bestScore) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const MincListExpr* tplt, const MincKernel* stmt)> cbk) const;
	void defineDefaultStmt(MincKernel* stmt);
	void defineExpr(MincExpr* tplt, MincKernel* expr);
	void defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> code, MincObject* type);
	void defineExpr(MincExpr* tplt, std::function<MincSymbol(MincBlockExpr*, std::vector<MincExpr*>&)> code, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> type);
	bool lookupExpr(MincExpr* expr) const;
	void lookupExprCandidates(const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const MincExpr* tplt, const MincKernel* expr)> cbk) const;
	void defineDefaultExpr(MincKernel* expr);
	void defineCast(MincCast* cast);
	const MincCast* lookupCast(MincObject* fromType, MincObject* toType) const;
	bool isInstance(MincObject* derivedType, MincObject* baseType) const;
	void listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const MincCast* cast)> cbk) const;
	void import(MincBlockExpr* importBlock);
	void defineSymbol(std::string name, MincObject* type, MincObject* value);
	const MincSymbol* lookupSymbol(const std::string& name) const;
	const std::string* lookupSymbolName(const MincObject* value) const;
	const std::string& lookupSymbolName(const MincObject* value, const std::string& defaultName) const;
	size_t countSymbols() const;
	void iterateSymbols(std::function<void(const std::string& name, const MincSymbol& symbol)> cbk) const;
	MincSymbol* importSymbol(const std::string& name);
	const std::vector<MincSymbol>* getBlockParams() const;
	MincSymbol codegen(MincBlockExpr* parentBlock);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
	void reset();
	void clearCache(size_t targetSize);
	const MincStmt* getCurrentStmt() const;

	static MincBlockExpr* parseCFile(const char* filename);
	static const std::vector<MincExpr*> parseCTplt(const char* tpltStr);

	static MincBlockExpr* parsePythonFile(const char* filename);
	static const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr);
};

class MincStopExpr : public MincExpr
{
public:
	MincStopExpr(const MincLocation& loc);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	MincExpr* clone() const;
};

class MincLiteralExpr : public MincExpr
{
public:
	const std::string value;
	MincLiteralExpr(const MincLocation& loc, const char* value);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincIdExpr : public MincExpr
{
public:
	const std::string name;
	MincIdExpr(const MincLocation& loc, const char* name);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincCastExpr : public MincExpr
{
	const MincCast* const cast;

public:
	MincCastExpr(const MincCast* cast, MincExpr* source);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	MincExpr* getSourceExpr() const;
	MincExpr* getDerivedExpr() const;
	MincExpr* clone() const;
};

class MincPlchldExpr : public MincExpr
{
public:
	char p1;
	char* p2;
	bool allowCast;
	MincPlchldExpr(const MincLocation& loc, char p1);
	MincPlchldExpr(const MincLocation& loc, char p1, const char* p2, bool allowCast);
	MincPlchldExpr(const MincLocation& loc, const char* p2);
	MincObject* getType(const MincBlockExpr* parentBlock) const;
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincParamExpr : public MincExpr
{
public:
	size_t idx;
	MincParamExpr(const MincLocation& loc, size_t idx);
	MincSymbol codegen(MincBlockExpr* parentBlock);
	MincObject* getType(const MincBlockExpr* parentBlock) const;
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincEllipsisExpr : public MincExpr
{
public:
	MincExpr* expr;
	MincEllipsisExpr(const MincLocation& loc, MincExpr* expr);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincArgOpExpr : public MincExpr
{
public:
	int op;
	MincExpr *var;
	MincListExpr* args;
	const std::string oopstr, copstr;
	MincArgOpExpr(const MincLocation& loc, int op, const char* oopstr, const char* copstr, MincExpr* var, MincListExpr* args);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void forget();
	void resolve(const MincBlockExpr* block);
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincEncOpExpr : public MincExpr
{
public:
	int op;
	MincExpr *val;
	const std::string oopstr, copstr;
	MincEncOpExpr(const MincLocation& loc, int op, const char* oopstr, const char* copstr, MincExpr* val);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincTerOpExpr : public MincExpr
{
public:
	int op1, op2;
	MincExpr *a, *b, *c;
	const std::string opstr1, opstr2;
	MincTerOpExpr(const MincLocation& loc, int op1, int op2, const char* opstr1, const char* opstr2, MincExpr* a, MincExpr* b, MincExpr* c);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincPrefixExpr : public MincExpr
{
public:
	int op;
	MincExpr *a;
	const std::string opstr;
	MincPrefixExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincPostfixExpr : public MincExpr
{
public:
	int op;
	MincExpr *a;
	const std::string opstr;
	MincPostfixExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincBinOpExpr : public MincExpr
{
public:
	int op;
	MincExpr *a, *b;
	const std::string opstr;
	MincPostfixExpr a_post;
	MincPrefixExpr b_pre;
	MincBinOpExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a, MincExpr* b);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

class MincVarBinOpExpr : public MincExpr
{
public:
	int op;
	MincExpr *a;
	const std::string opstr;
	MincVarBinOpExpr(const MincLocation& loc, int op, const char* opstr, MincExpr* a);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
};

#endif