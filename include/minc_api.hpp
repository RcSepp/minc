#ifndef __MINC_API_HPP
#define __MINC_API_HPP

#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "minc_types.h"

typedef std::vector<ExprAST*>::const_iterator ExprASTIter;

extern "C"
{
	BlockExprAST* getRootScope();
	BlockExprAST* getFileScope();
	const Variable& getVoid();
	void defineImportRule(BaseScopeType* fromScope, BaseScopeType* toScope, BaseType* symbolType, ImptBlock imptBlock);
}

class ExprAST
{
public:
	Location loc;
	enum ExprType {
		STMT, LIST, STOP, LITERAL, ID, CAST, PLCHLD, PARAM, ELLIPSIS, ARGOP, ENCOP, TEROP, BINOP, VARBINOP, PREOP, POSTOP, BLOCK,
		NUM_EXPR_TYPES
	};
	const ExprType exprtype;

	// Resolved state
	CodegenContext* resolvedContext;
	std::vector<ExprAST*> resolvedParams;

	ExprAST(const Location& loc, ExprType exprtype);
	virtual ~ExprAST();
	virtual Variable codegen(BlockExprAST* parentBlock);
	virtual BaseType* getType(const BlockExprAST* parentBlock) const;
	virtual bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const = 0;
	virtual void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const = 0;
	virtual void resolveTypes(const BlockExprAST* block);
	virtual std::string str() const = 0;
	virtual std::string shortStr() const;
	virtual int comp(const ExprAST* other) const;
	virtual ExprAST* clone() const = 0;
};
bool operator<(const ExprAST& left, const ExprAST& right);

namespace std
{
	template<> struct less<const ExprAST*>
	{
		bool operator()(const ExprAST* lhs, const ExprAST* rhs) const { return lhs->comp(rhs) < 0; }
	};
}

class StreamingExprASTIter
{
	const std::vector<ExprAST*>* buffer;
	size_t idx;
	std::function<bool()> next;

public:
	static bool defaultNext() { return false; }
	StreamingExprASTIter(const std::vector<ExprAST*>* buffer=nullptr, size_t idx=0, std::function<bool()> next=defaultNext);
	StreamingExprASTIter(const StreamingExprASTIter& other) = default;
	bool done();
	ExprAST* operator*();
	ExprAST* operator[](int i);
	size_t operator-(const StreamingExprASTIter& other) const;
	StreamingExprASTIter& operator=(const StreamingExprASTIter& other) = default;
	StreamingExprASTIter operator+(int n) const;
	StreamingExprASTIter operator++(int);
	StreamingExprASTIter& operator++();
	ExprASTIter iter() const;
};

class ListExprAST : public ExprAST
{
public:
	char separator;
	std::vector<ExprAST*> exprs;
	ListExprAST(char separator);
	ListExprAST(char separator, std::vector<ExprAST*> exprs);
	Variable codegen(BlockExprAST* parentBlock);
	bool match(const BlockExprAST* block, const ExprAST* exprs, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	std::vector<ExprAST*>::iterator begin();
	std::vector<ExprAST*>::const_iterator cbegin() const;
	std::vector<ExprAST*>::iterator end();
	std::vector<ExprAST*>::const_iterator cend() const;
	size_t size() const;
	ExprAST* at(size_t index);
	const ExprAST* at(size_t index) const;
	ExprAST* operator[](size_t index);
	const ExprAST* operator[](size_t index) const;
	void push_back(ExprAST* expr);
	ExprAST* clone() const;
};
std::vector<ExprAST*>::iterator begin(ListExprAST& exprs);
std::vector<ExprAST*>::iterator begin(ListExprAST* exprs);
std::vector<ExprAST*>::iterator end(ListExprAST& exprs);
std::vector<ExprAST*>::iterator end(ListExprAST* exprs);

namespace std
{
	template<> struct less<const ListExprAST*>
	{
		bool operator()(const ListExprAST* lhs, const ListExprAST* rhs) const { return lhs->comp(rhs) < 0; }
	};
}

class StatementRegister
{
private:
	std::map<const ListExprAST*, CodegenContext*> stmtreg;
	std::array<std::map<const ExprAST*, CodegenContext*>, ExprAST::NUM_EXPR_TYPES> exprreg;
	CodegenContext *antiStmt, *antiExpr;
public:
	StatementRegister() : antiStmt(nullptr), antiExpr(nullptr) {}
	void defineStmt(const ListExprAST* tplt, CodegenContext* stmt);
	std::pair<const ListExprAST*, CodegenContext*> lookupStmt(const BlockExprAST* block, StreamingExprASTIter stmt, StreamingExprASTIter& stmtEnd, MatchScore& score) const;
	void lookupStmtCandidates(const BlockExprAST* block, const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk) const;
	void defineAntiStmt(CodegenContext* stmt);

	void defineExpr(const ExprAST* tplt, CodegenContext* expr);
	std::pair<const ExprAST*, CodegenContext*> lookupExpr(const BlockExprAST* block, ExprAST* expr, MatchScore& bestScore) const;
	void lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk) const;
	void defineAntiExpr(CodegenContext* expr) { antiExpr = expr; }
};

struct InheritanceCast : public Cast
{
	InheritanceCast(BaseType* fromType, BaseType* toType, CodegenContext* context);
	int getCost() const;
	Cast* derive() const;
};

struct TypeCast : public Cast
{
	TypeCast(BaseType* fromType, BaseType* toType, CodegenContext* context);
	int getCost() const;
	Cast* derive() const;
};

class CastRegister
{
private:
	BlockExprAST* const block;
	std::map<std::pair<BaseType*, BaseType*>, Cast*> casts;
	std::multimap<BaseType*, Cast*> fwdCasts, bwdCasts;
public:
	CastRegister(BlockExprAST* block);
	void defineDirectCast(Cast* cast);
	void defineIndirectCast(const CastRegister& castreg, Cast* cast);
	const Cast* lookupCast(BaseType* fromType, BaseType* toType) const;
	bool isInstance(BaseType* derivedType, BaseType* baseType) const;
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const Cast* cast)> cbk) const;
};

class StmtAST : public ExprAST
{
public:
	ExprASTIter begin, end;
	std::vector<ExprAST*> resolvedExprs;
	ExprASTIter sourceExprPtr;

	StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context);
	StmtAST();
	~StmtAST();
	Variable codegen(BlockExprAST* parentBlock);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class BlockExprAST : public ExprAST
{
private:
	StatementRegister stmtreg;
	std::map<std::string, Variable> scope;
	CastRegister castreg;
	StmtAST currentStmt;

public:
	BlockExprAST* parent;
	std::vector<BlockExprAST*> references;
	std::vector<ExprAST*>* exprs;
	std::string name;
	size_t exprIdx;
	BaseScopeType* scopeType;
	std::vector<Variable> blockParams;
	std::vector<Variable*> resultCache;
	size_t resultCacheIdx;
	bool isBlockSuspended, isStmtSuspended, isExprSuspended;
	bool isBusy;
	BlockExprAST(const Location& loc, std::vector<ExprAST*>* exprs);
	void defineStmt(const std::vector<ExprAST*>& tplt, CodegenContext* stmt);
	bool lookupStmt(ExprASTIter beginExpr, StmtAST& stmt) const;
	void lookupStmtCandidates(const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const;
	std::pair<const ListExprAST*, CodegenContext*> lookupStmt(StreamingExprASTIter stmt, StreamingExprASTIter& bestStmtEnd, MatchScore& bestScore) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk) const;
	void defineAntiStmt(CodegenContext* stmt);
	void defineExpr(ExprAST* tplt, CodegenContext* expr);
	bool lookupExpr(ExprAST* expr) const;
	void lookupExprCandidates(const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk) const;
	void defineAntiExpr(CodegenContext* expr);
	void defineCast(Cast* cast);
	const Cast* lookupCast(BaseType* fromType, BaseType* toType) const;
	bool isInstance(BaseType* derivedType, BaseType* baseType) const;
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const;
	size_t countCasts() const;
	void iterateCasts(std::function<void(const Cast* cast)> cbk) const;
	void import(BlockExprAST* importBlock);
	void defineSymbol(std::string name, BaseType* type, BaseValue* var);
	const Variable* lookupSymbol(const std::string& name) const;
	size_t countSymbols() const;
	void iterateSymbols(std::function<void(const std::string& name, const Variable& symbol)> cbk) const;
	Variable* importSymbol(const std::string& name);
	const std::vector<Variable>* getBlockParams() const;
	Variable codegen(BlockExprAST* parentBlock);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	std::string shortStr() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
	void reset();
	void clearCache(size_t targetSize);
	const StmtAST* getCurrentStmt() const;

	static BlockExprAST* parseCFile(const char* filename);
	static const std::vector<ExprAST*> parseCTplt(const char* tpltStr);

	static BlockExprAST* parsePythonFile(const char* filename);
	static const std::vector<ExprAST*> parsePythonTplt(const char* tpltStr);
};

class StopExprAST : public ExprAST
{
public:
	StopExprAST(const Location& loc);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	ExprAST* clone() const;
};

class LiteralExprAST : public ExprAST
{
public:
	const std::string value;
	LiteralExprAST(const Location& loc, const char* value);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class IdExprAST : public ExprAST
{
public:
	const std::string name;
	IdExprAST(const Location& loc, const char* name);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class CastExprAST : public ExprAST
{
	const Cast* const cast;

public:
	CastExprAST(const Cast* cast, ExprAST* source);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	ExprAST* getDerivedExpr();
	ExprAST* clone() const;
};

class PlchldExprAST : public ExprAST
{
public:
	char p1;
	char* p2;
	bool allowCast;
	PlchldExprAST(const Location& loc, char p1);
	PlchldExprAST(const Location& loc, char p1, const char* p2, bool allowCast);
	PlchldExprAST(const Location& loc, const char* p2);
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class ParamExprAST : public ExprAST
{
public:
	int idx;
	ParamExprAST(const Location& loc, int idx);
	Variable codegen(BlockExprAST* parentBlock);
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class EllipsisExprAST : public ExprAST
{
public:
	ExprAST* expr;
	EllipsisExprAST(const Location& loc, ExprAST* expr);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class ArgOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *var;
	ListExprAST* args;
	const std::string oopstr, copstr;
	ArgOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* var, ListExprAST* args);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class EncOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *val;
	const std::string oopstr, copstr;
	EncOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* val);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class TerOpExprAST : public ExprAST
{
public:
	int op1, op2;
	ExprAST *a, *b, *c;
	const std::string opstr1, opstr2;
	TerOpExprAST(const Location& loc, int op1, int op2, const char* opstr1, const char* opstr2, ExprAST* a, ExprAST* b, ExprAST* c);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class PrefixExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	PrefixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class PostfixExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	PostfixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class BinOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a, *b;
	const std::string opstr;
	PostfixExprAST a_post;
	PrefixExprAST b_pre;
	BinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a, ExprAST* b);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

class VarBinOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	VarBinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block);
	std::string str() const;
	int comp(const ExprAST* other) const;
	ExprAST* clone() const;
};

#endif