#ifndef __MINC_API_HPP
#define __MINC_API_HPP

#include <cassert>
#include <functional>
#include <istream>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "minc_types.h"

#define CACHE_RESULTS

typedef std::vector<MincExpr*>::const_iterator MincExprIter;

extern "C"
{
	void raiseCompileError(const char* msg, const MincExpr* loc);
	MincObject* getErrorType();
	const MincSymbol& getVoid();
	MincBlockExpr* getRootScope();
	MincBlockExpr* getFileScope();
	void defineImportRule(MincScopeType* fromScope, MincScopeType* toScope, MincObject* symbolType, ImptBlock imptBlock);
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
	mutable MincObject* resolvedType;
	std::vector<MincExpr*> resolvedParams;

	// Built state
	MincKernel* builtKernel;

	MincExpr(const MincLocation& loc, ExprType exprtype);
	virtual ~MincExpr();
	virtual bool run(MincRuntime& runtime) const;
	MincObject* getType(const MincBlockExpr* parentBlock) const;
	MincObject* getType(MincBlockExpr* parentBlock);
	virtual bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const = 0;
	virtual void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const = 0;
	inline bool isResolved() const { return this->resolvedKernel != nullptr; }
	virtual void resolve(const MincBlockExpr* block);
	virtual void forget();
	inline bool isBuilt() { return this->builtKernel != nullptr; }
	virtual MincSymbol& build(MincBuildtime& buildtime);
	virtual std::string str() const = 0;
	virtual std::string shortStr() const;
	virtual int comp(const MincExpr* other) const;
	virtual MincExpr* clone() const = 0;
	MincExpr* getSourceExpr();
	MincExpr* getDerivedExpr();
	int exec(MincBuildtime& buildtime);

	static MincSymbol evalCCode(const char* code, MincBlockExpr* scope);

	static MincSymbol evalPythonCode(const char* code, MincBlockExpr* scope);
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
	MincExprIter current, end;

public:
	ResolvingMincExprIter() : resolveScope(nullptr) {}
	ResolvingMincExprIter(const MincBlockExpr* resolveScope, const std::vector<MincExpr*>& exprs)
		: resolveScope(resolveScope), current(exprs.begin()), end(exprs.end())
	{
		assert(resolveScope != nullptr);
	}
	ResolvingMincExprIter(const MincBlockExpr* resolveScope, MincExprIter current, const MincExprIter end)
		: resolveScope(resolveScope), current(current), end(end)
	{
		assert(resolveScope != nullptr);
	}
	ResolvingMincExprIter(const ResolvingMincExprIter& other) = default;
	ResolvingMincExprIter& operator=(const ResolvingMincExprIter& other) = default;
	inline bool done() { return current == end; }
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
	inline size_t operator-(const ResolvingMincExprIter& other) const { return resolveScope == nullptr || other.resolveScope == nullptr ? 0 : current - other.current; }
	inline ResolvingMincExprIter operator+(int n) const { return ResolvingMincExprIter(resolveScope, current + n, end); }
	inline ResolvingMincExprIter operator++(int) { return ResolvingMincExprIter(resolveScope, current++, end); }
	inline ResolvingMincExprIter& operator++() { ++current; return *this; }
	inline MincExprIter iter() const { return current; }
	inline MincExprIter iterEnd() const { return end; }
};

class MincListExpr : public MincExpr
{
public:
	char separator;
	std::vector<MincExpr*> exprs;
	MincListExpr(char separator);
	MincListExpr(char separator, std::vector<MincExpr*> exprs);
	bool run(MincRuntime& runtime) const;
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
public:
	struct RegisteredKernel
	{
		MincKernel* kernel;
		unsigned int refDepth;
		RegisteredKernel() {}
		RegisteredKernel(MincKernel* kernel, unsigned int refDepth) : kernel(kernel), refDepth(refDepth) {}
	};
private:
	std::map<const MincListExpr*, RegisteredKernel> stmtreg;
	std::array<std::map<const MincExpr*, RegisteredKernel>, MincExpr::NUM_EXPR_TYPES> exprreg;
public:
	void defineStmt(const MincListExpr* tplt, MincKernel* stmt, MincBlockExpr* scope, MincBlockExpr* refScope);
	std::pair<const MincListExpr*, RegisteredKernel> lookupStmt(const MincBlockExpr* block, ResolvingMincExprIter stmt, unsigned int refDepth, ResolvingMincExprIter& stmtEnd, MatchScore& score) const;
	void lookupStmtCandidates(const MincBlockExpr* block, const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const MincListExpr* tplt, MincKernel* stmt)> cbk) const;

	void defineExpr(const MincExpr* tplt, MincKernel* expr);
	std::pair<const MincExpr*, RegisteredKernel> lookupExpr(const MincBlockExpr* block, MincExpr* expr, MatchScore& bestScore) const;
	void lookupExprCandidates(const MincBlockExpr* block, const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const MincExpr* tplt, MincKernel* expr)> cbk) const;
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

class MincCastRegister
{
private:
	MincBlockExpr* const block;
	std::map<std::pair<MincObject*, MincObject*>, MincCast*> casts;
	std::multimap<MincObject*, MincCast*> fwdCasts, bwdCasts;
public:
	MincCastRegister(MincBlockExpr* block);
	void defineDirectCast(MincCast* cast);
	void defineIndirectCast(const MincCastRegister& castreg, const MincCast* cast);
	const MincCast* lookupCast(MincObject* fromType, MincObject* toType) const;
	bool isInstance(MincObject* derivedType, MincObject* baseType) const;
	void listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const MincCast* cast)> cbk) const;
	void iterateBases(MincObject* derivedType, std::function<void(MincObject* baseType)> cbk) const;
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
	bool run(MincRuntime& runtime) const;
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void resolve(const MincBlockExpr* block);
	void forget();
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;

	static void evalCCode(const char* code, MincBlockExpr* scope);

	static void evalPythonCode(const char* code, MincBlockExpr* scope);
};

class MincBlockExpr : public MincExpr
{
	friend MincEnteredBlockExpr;
public:
	enum SymbolType {
		BUILDTIME, STACK
	};
private:
	struct SymbolMapEntry
	{
		size_t index;
		SymbolType type;
		SymbolMapEntry() = default;
		SymbolMapEntry(size_t index, SymbolType type) : index(index), type(type) {}
	};
	MincStatementRegister stmtreg;
	MincKernel *defaultStmtKernel, *defaultExprKernel;
	std::vector<MincSymbol*> symbols;
	std::vector<MincStackSymbol*> stackSymbols;
	std::map<std::string, SymbolMapEntry> symbolMap;
	std::map<const MincObject*, std::string> symbolNameMap;
	MincCastRegister castreg;
	size_t stackSize;

public:
	MincBlockExpr* parent;
	std::vector<MincStmt*> builtStmts;
	std::vector<const MincStmt*> runStmts;
	std::vector<MincBlockExpr*> references;
	std::vector<MincExpr*>* exprs;
	MincScopeType* scopeType;
	std::vector<MincSymbol> blockParams;
	mutable MincStackFrame* stackFrame;
#ifdef CACHE_RESULTS
	mutable std::vector<std::pair<MincObject*, bool>> resultCache;
#endif
	mutable size_t resultCacheIdx;
	mutable bool isBlockSuspended, isStmtSuspended, isExprSuspended, isResuming;
	bool isResumable;
	mutable bool isBusy;

	// Meta data
	std::string name;
	void *user, *userType;

	MincBlockExpr(const MincLocation& loc, std::vector<MincExpr*>* exprs);
	void defineStmt(const std::vector<MincExpr*>& tplt, MincKernel* stmt, MincBlockExpr* scope=nullptr);
	void defineStmt(const std::vector<MincExpr*>& tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincBlockExpr* scope=nullptr);
	void defineStmt(const std::vector<MincExpr*>& tplt, std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincBlockExpr* scope=nullptr);
	bool lookupStmt(MincExprIter beginExpr, MincExprIter endExpr, MincStmt& stmt) const;
	void lookupStmtCandidates(const MincListExpr* stmt, std::multimap<MatchScore, const std::pair<const MincListExpr*, MincKernel*>>& candidates) const;
	std::pair<const MincListExpr*, MincStatementRegister::RegisteredKernel> lookupStmt(ResolvingMincExprIter stmt, ResolvingMincExprIter& bestStmtEnd, MatchScore& bestScore, MincKernel** defaultStmtKernel) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const MincListExpr* tplt, MincKernel* stmt)> cbk) const;
	void defineDefaultStmt(MincKernel* stmt);
	void defineExpr(MincExpr* tplt, MincKernel* expr);
	void defineExpr(MincExpr* tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincObject* type);
	void defineExpr(MincExpr* tplt, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> type);
	void defineExpr(MincExpr* tplt, std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, MincObject* type);
	void defineExpr(MincExpr* tplt, std::function<void(MincBuildtime&, std::vector<MincExpr*>&)> build, std::function<bool(MincRuntime&, const std::vector<MincExpr*>&)> run, std::function<MincObject*(const MincBlockExpr*, const std::vector<MincExpr*>&)> type);
	bool lookupExpr(MincExpr* expr) const;
	void lookupExprCandidates(const MincExpr* expr, std::multimap<MatchScore, const std::pair<const MincExpr*, MincKernel*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const MincExpr* tplt, MincKernel* expr)> cbk) const;
	void defineDefaultExpr(MincKernel* expr);
	void defineCast(MincCast* cast);
	const MincCast* lookupCast(MincObject* fromType, MincObject* toType) const;
	bool isInstance(MincObject* derivedType, MincObject* baseType) const;
	void listAllCasts(std::list<std::pair<MincObject*, MincObject*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const MincCast* cast)> cbk) const;
	void iterateBases(MincObject* derivedType, std::function<void(MincObject* baseType)> cbk) const;
	void import(MincBlockExpr* importBlock);
	void defineSymbol(std::string name, MincObject* type, MincObject* value);
	const MincSymbol* lookupSymbol(const std::string& name) const;
	const std::string* lookupSymbolName(const MincObject* value) const;
	const std::string& lookupSymbolName(const MincObject* value, const std::string& defaultName) const;
	size_t countSymbols() const;
	size_t countSymbols(SymbolType symbolType) const;
	void iterateBuildtimeSymbols(std::function<void(const std::string& name, const MincSymbol& symbol)> cbk) const;
	void iterateStackSymbols(std::function<void(const std::string& name, const MincStackSymbol& symbol)> cbk) const;
	MincSymbol* importSymbol(const std::string& name);
	const MincStackSymbol* allocStackSymbol(const std::string& name, MincObject* type, size_t size);
	const MincStackSymbol* allocStackSymbol(MincObject* type, size_t size);
	const MincStackSymbol* lookupStackSymbol(const std::string& name) const;
	const std::vector<MincSymbol>* getBlockParams() const;
	void enter(MincEnteredBlockExpr& entered);
	bool run(MincRuntime& runtime) const;
	MincSymbol& build(MincBuildtime& buildtime);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	std::string str() const;
	std::string shortStr() const;
	int comp(const MincExpr* other) const;
	MincExpr* clone() const;
	void reset() const;
	void clearCache(size_t targetSize) const;

	static MincFlavor flavorFromFile(const std::string& filename);
	static MincFlavor flavorFromFile(const char* filename);
	static MincBlockExpr* parseStream(std::istream& stream, MincFlavor flavor);
	static MincBlockExpr* parseFile(const char* filename, MincFlavor flavor=MincFlavor::C_FLAVOR);
	static MincBlockExpr* parseCode(const char* code, MincFlavor flavor);
	static const std::vector<MincExpr*> parseTplt(const char* tpltStr, MincFlavor flavor);

	static MincBlockExpr* parseCStream(std::istream& stream);
	static MincBlockExpr* parseCFile(const char* filename);
	static MincBlockExpr* parseCCode(const char* code);
	static const std::vector<MincExpr*> parseCTplt(const char* tpltStr);
	static void evalCCode(const char* code, MincBlockExpr* scope);

	static MincBlockExpr* parsePythonStream(std::istream& stream);
	static MincBlockExpr* parsePythonFile(const char* filename);
	static MincBlockExpr* parsePythonCode(const char* code);
	static const std::vector<MincExpr*> parsePythonTplt(const char* tpltStr);
	static void evalPythonCode(const char* code, MincBlockExpr* scope);

	static MincBlockExpr* parseGoStream(std::istream& stream);
	static MincBlockExpr* parseGoFile(const char* filename);
	static MincBlockExpr* parseGoCode(const char* code);
	static const std::vector<MincExpr*> parseGoTplt(const char* tpltStr);
	static void evalGoCode(const char* code, MincBlockExpr* scope);
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
	enum Flags {
		NONE, NO_TYPECAST, NO_CAST
	};
	char p1;
	char* p2;
	Flags flags;
	MincPlchldExpr(const MincLocation& loc, char p1);
	MincPlchldExpr(const MincLocation& loc, char p1, const char* p2, Flags flags);
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
private:
	struct Kernel : public MincKernel
	{
		MincParamExpr* expr;

		Kernel(MincParamExpr* expr);
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
	} kernel;

public:
	size_t idx;
	MincParamExpr(const MincLocation& loc, size_t idx);
	bool match(const MincBlockExpr* block, const MincExpr* expr, MatchScore& score) const;
	void collectParams(const MincBlockExpr* block, MincExpr* expr, std::vector<MincExpr*>& params, size_t& paramIdx) const;
	void forget();
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

struct MincOpaqueCastKernel : public MincKernel
{
private:
	MincObject* const type;
public:
	MincOpaqueCastKernel(MincObject* type);
	MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params);
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
};

#endif