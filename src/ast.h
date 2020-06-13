#ifndef __AST_H
#define __AST_H

#include <string>
#include <cstring>
#include <map>
#include <algorithm>
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <cassert>
#include <regex>

#include "minc_types.h"

const std::string& getTypeNameInternal(const BaseType* type);
const char* getTypeName2Internal(const BaseType* type);

typedef std::vector<ExprAST*>::const_iterator ExprASTIter;

struct UndefinedStmtException : public CompileError
{
	UndefinedStmtException(const StmtAST* stmt);
};
struct UndefinedExprException : public CompileError
{
	UndefinedExprException(const ExprAST* expr);
};
struct UndefinedIdentifierException : public CompileError
{
	UndefinedIdentifierException(const IdExprAST* id);
};
struct InvalidTypeException : public CompileError
{
	InvalidTypeException(const PlchldExprAST* plchld);
};

struct TypeDescription
{
	std::string name;
};

class Expr
{
	virtual Variable codegen(BlockExprAST* parentBlock) = 0;
	virtual std::string str() const = 0;
};

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

	ExprAST(const Location& loc, ExprType exprtype) : loc(loc), exprtype(exprtype), resolvedContext(nullptr) {}
	virtual ~ExprAST() {}
	virtual Variable codegen(BlockExprAST* parentBlock);
	virtual BaseType* getType(const BlockExprAST* parentBlock) const;
	virtual bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const = 0;
	virtual void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const = 0;
	virtual void resolveTypes(const BlockExprAST* block);
	virtual std::string str() const = 0;
	virtual std::string shortStr() const { return str(); }
	virtual int comp(const ExprAST* other) const { return this->exprtype - other->exprtype; }
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
	StreamingExprASTIter(const std::vector<ExprAST*>* buffer=nullptr, size_t idx=0, std::function<bool()> next=defaultNext) : buffer(buffer), idx(idx), next(next)
	{
		if (buffer != nullptr)
			for(size_t i = buffer->size(); i <= idx; ++i)
				if (!next())
				{
					idx = buffer->size();
					break;
				}
	}
	StreamingExprASTIter(const StreamingExprASTIter& other) = default;
	bool done()
	{
		return idx == buffer->size() && !next();
	}

	ExprAST* operator*() { return idx != buffer->size() ? buffer->at(idx) : nullptr; }
	ExprAST* operator[](int i) { return idx + i < buffer->size() ? buffer->at(idx + i) : nullptr; }
	size_t operator-(const StreamingExprASTIter& other) const { return idx - other.idx; }
	StreamingExprASTIter& operator=(const StreamingExprASTIter& other) = default;

	StreamingExprASTIter operator+(int n) const
	{
		return StreamingExprASTIter(buffer, idx + n, next);
	}
	StreamingExprASTIter operator++(int)
	{
		if (done())
			return *this;
		else
			return StreamingExprASTIter(buffer, idx++, next);
	}
	StreamingExprASTIter& operator++()
	{
		if (!done())
			++idx;
		return *this;
	}

	ExprASTIter iter() const { return buffer->cbegin() + idx; }
};

class LocExprAST : public ExprAST
{
public:
	LocExprAST(const Location& loc) : ExprAST(loc, (ExprAST::ExprType)-1) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const { return true; }
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return ""; }
	std::string shortStr() const { return ""; }
	ExprAST* clone() const { return new LocExprAST(loc); }
};

class ListExprAST : public ExprAST
{
public:
	char separator;
	std::vector<ExprAST*> exprs;
	ListExprAST(char separator) : ExprAST({0}, ExprAST::ExprType::LIST), separator(separator) {}
	ListExprAST(char separator, std::vector<ExprAST*> exprs) : ExprAST({0}, ExprAST::ExprType::LIST), separator(separator), exprs(exprs) {}
	Variable codegen(BlockExprAST* parentBlock) { assert(0); return Variable(nullptr, nullptr); /* Unreachable */ }
	bool match(const BlockExprAST* block, const ExprAST* exprs, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block) { for (auto expr: exprs) expr->resolveTypes(block); }
	std::string str() const
	{
		if (exprs.empty())
			return "";

		std::string s;
		const std::string _(1, ' ');
		switch(separator)
		{
		case '\0': s = _; break;
		case ',': case ';': s = separator + _; break;
		default: s = _ + separator + _; break;
		}

		std::string result = exprs[0]->str();
		for (auto expriter = exprs.begin() + 1; expriter != exprs.end(); ++expriter)
			result += (*expriter)->exprtype == ExprAST::ExprType::STOP ? (*expriter)->str() : s + (*expriter)->str();
		return result;
	}
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const ListExprAST* _other = (const ListExprAST*)other;
		c = (int)this->exprs.size() - (int)_other->exprs.size();
		if (c) return c;
		for (std::vector<ExprAST*>::const_iterator t = this->exprs.cbegin(), o = _other->exprs.cbegin(); t != this->exprs.cend(); ++t, ++o)
		{
			c = (*t)->comp(*o);
			if (c) return c;
		}
		return 0;
	}

	std::vector<ExprAST*>::iterator begin() { return exprs.begin(); }
	std::vector<ExprAST*>::const_iterator cbegin() const { return exprs.cbegin(); }
	std::vector<ExprAST*>::iterator end() { return exprs.end(); }
	std::vector<ExprAST*>::const_iterator cend() const { return exprs.cend(); }
	size_t size() const { return exprs.size(); }
	ExprAST* at(size_t index) { return exprs.at(index); }
	const ExprAST* at(size_t index) const { return exprs.at(index); }
	ExprAST* operator[](size_t index) { return exprs[index]; }
	const ExprAST* operator[](size_t index) const { return exprs[index]; }
	void push_back(ExprAST* expr) { return exprs.push_back(expr); }
	ExprAST* clone() const
	{
		ListExprAST* clone = new ListExprAST(separator);
		for (ExprAST* expr: this->exprs)
			clone->exprs.push_back(expr->clone());
		return clone;
	}
	const char getSeparator() const { return separator; }
	const void setSeparator(char separator) { this->separator = separator; }
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
	InheritanceCast(BaseType* fromType, BaseType* toType, CodegenContext* context)
		: Cast(fromType, toType, context) {}
	int getCost() const { return 0; }
	Cast* derive() const { return nullptr; }
};

struct TypeCast : public Cast
{
	TypeCast(BaseType* fromType, BaseType* toType, CodegenContext* context)
		: Cast(fromType, toType, context) {}
	int getCost() const { return 1; }
	Cast* derive() const { return new TypeCast(fromType, toType, context); }
};

class CastRegister
{
private:
	BlockExprAST* const block;
	std::map<std::pair<BaseType*, BaseType*>, Cast*> casts;
	std::multimap<BaseType*, Cast*> fwdCasts, bwdCasts;
public:
	CastRegister(BlockExprAST* block) : block(block) {}
	void defineDirectCast(Cast* cast);
	void defineIndirectCast(const CastRegister& castreg, Cast* cast);
	const Cast* lookupCast(BaseType* fromType, BaseType* toType) const;
	bool isInstance(BaseType* derivedType, BaseType* baseType) const;
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const;
	size_t countCasts() const { return casts.size(); }
	void iterateCasts(std::function<void(const Cast* cast)> cbk) const
	{
		for (const std::pair<const std::pair<BaseType*, BaseType*>, Cast*>& iter: casts)
			cbk(iter.second);
	}
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
bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const { assert(0); return false; /* Unreachable */ }
void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const { assert(0); }
void resolveTypes(const BlockExprAST* block) { assert(0); }
	std::string str() const
	{
		if (begin == end)
			return "";
		std::string result = (*begin)->str();
		for (ExprASTIter expr = begin; ++expr != end;)
			result += (*expr)->exprtype == ExprAST::ExprType::STOP ? (*expr)->str() : ' ' + (*expr)->str();
		return result;
	}
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const StmtAST* _other = (const StmtAST*)other;
		c = (int)(this->end - this->begin) - (int)(_other->end - _other->begin);
		if (c) return c;
		for (std::vector<ExprAST*>::const_iterator t = this->begin, o = _other->begin; t != this->end; ++t, ++o)
		{
			c = (*t)->comp(*o);
			if (c) return c;
		}
		return 0;
	}
	ExprAST* clone() const { return new StmtAST(begin, end, resolvedContext); }
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
	BlockExprAST(const Location& loc, std::vector<ExprAST*>* exprs)
		: ExprAST(loc, ExprAST::ExprType::BLOCK), castreg(this), parent(nullptr), exprs(exprs), exprIdx(0), scopeType(nullptr), resultCacheIdx(0), isBlockSuspended(false), isStmtSuspended(false), isExprSuspended(false), isBusy(false) {}

	void defineStmt(const std::vector<ExprAST*>& tplt, CodegenContext* stmt)
	{
		for (ExprAST* tpltExpr: tplt)
			tpltExpr->resolveTypes(this);
		stmtreg.defineStmt(new ListExprAST('\0', tplt), stmt);
	}
	bool lookupStmt(ExprASTIter beginExpr, StmtAST& stmt) const;
	void lookupStmtCandidates(const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			block->stmtreg.lookupStmtCandidates(this, stmt, candidates);
			for (const BlockExprAST* ref: block->references)
				ref->stmtreg.lookupStmtCandidates(this, stmt, candidates);
		}
	}
	std::pair<const ListExprAST*, CodegenContext*> lookupStmt(StreamingExprASTIter stmt, StreamingExprASTIter& bestStmtEnd, MatchScore& bestScore) const;
	size_t countStmts() const { return stmtreg.countStmts(); }
	void iterateStmts(std::function<void(const ListExprAST* tplt, const CodegenContext* stmt)> cbk) const { stmtreg.iterateStmts(cbk); }
	void defineAntiStmt(CodegenContext* stmt) { stmtreg.defineAntiStmt(stmt); }

	void defineExpr(ExprAST* tplt, CodegenContext* expr)
	{
		tplt->resolveTypes(this);
		stmtreg.defineExpr(tplt, expr);
	}
	bool lookupExpr(ExprAST* expr) const;
	void lookupExprCandidates(const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>>& candidates) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			block->stmtreg.lookupExprCandidates(this, expr, candidates);
			for (const BlockExprAST* ref: block->references)
				ref->stmtreg.lookupExprCandidates(this, expr, candidates);
		}
	}
	size_t countExprs() const { return stmtreg.countExprs(); }
	void iterateExprs(std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk) const { stmtreg.iterateExprs(cbk); }
	void defineAntiExpr(CodegenContext* expr) { stmtreg.defineAntiExpr(expr); }

	void defineCast(Cast* cast)
	{
		// Skip if one of the following is true
		// 1. fromType == toType
		// 2. A cast exists from fromType to toType with a lower or equal cost
		// 3. Cast is an inheritance and another inheritance cast exists from toType to fromType (inheritance loop avoidance)
		const Cast* existingCast;
		if (cast->fromType == cast->toType
			|| ((existingCast = lookupCast(cast->fromType, cast->toType)) != nullptr && existingCast->getCost() <= cast->getCost())
			|| ((existingCast = lookupCast(cast->toType, cast->fromType)) != nullptr && existingCast->getCost() == 0 && cast->getCost() == 0))
			return;

		castreg.defineDirectCast(cast);

		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			castreg.defineIndirectCast(block->castreg, cast);
			for (const BlockExprAST* ref: block->references)
				castreg.defineIndirectCast(ref->castreg, cast);
		}
	}
	const Cast* lookupCast(BaseType* fromType, BaseType* toType) const
	{
		const Cast* cast;
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			if ((cast = block->castreg.lookupCast(fromType, toType)) != nullptr)
				return cast;
			for (const BlockExprAST* ref: block->references)
				if ((cast = ref->castreg.lookupCast(fromType, toType)) != nullptr)
					return cast;
		}
		return nullptr;
	}
	bool isInstance(BaseType* derivedType, BaseType* baseType) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			if (block->castreg.isInstance(derivedType, baseType))
				return true;
			for (const BlockExprAST* ref: block->references)
				if (ref->castreg.isInstance(derivedType, baseType))
					return true;
		}
		return false;
	}
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			block->castreg.listAllCasts(casts);
			for (const BlockExprAST* ref: block->references)
				ref->castreg.listAllCasts(casts);
		}
	}
	size_t countCasts() const { return castreg.countCasts(); }
	void iterateCasts(std::function<void(const Cast* cast)> cbk) const { castreg.iterateCasts(cbk); }

	void import(BlockExprAST* importBlock);

	void defineSymbol(std::string name, BaseType* type, BaseValue* var) { scope[name] = Variable(type, var); }
	const Variable* lookupSymbol(const std::string& name) const;
	size_t countSymbols() const { return scope.size(); }
	void iterateSymbols(std::function<void(const std::string& name, const Variable& symbol)> cbk) const
	{
		for (const std::pair<std::string, Variable>& iter: scope)
			cbk(iter.first, iter.second);
	}
	Variable* importSymbol(const std::string& name);

	const std::vector<Variable>* getBlockParams() const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			if (block->blockParams.size())
				return &block->blockParams;
			for (const BlockExprAST* ref: block->references)
				if (ref->blockParams.size())
					return &ref->blockParams;
		}
		return nullptr;
	}

	Variable codegen(BlockExprAST* parentBlock);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const
	{
		if (exprs->empty())
			return "{}";

		std::string result = "{\n";
		for (auto expr: *exprs)
		{
			if (expr->exprtype == ExprAST::ExprType::STOP)
			{
				if (expr->exprtype == ExprAST::ExprType::STOP)
					result.pop_back(); // Remove ' ' before STOP string
				result += expr->str() + '\n';
			}
			else
				result += expr->str() + ' ';
		}

		size_t start_pos = 0;
		while((start_pos = result.find("\n", start_pos)) != std::string::npos && start_pos + 1 != result.size()) {
			result.replace(start_pos, 1, "\n\t");
			start_pos += 2;
		}

		return result + '}';
	}
	std::string shortStr() const { return "{}"; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const BlockExprAST* _other = (const BlockExprAST*)other;
		c = (int)this->exprs->size() - (int)_other->exprs->size();
		if (c) return c;
		for (std::vector<ExprAST*>::const_iterator t = this->exprs->cbegin(), o = _other->exprs->cbegin(); t != this->exprs->cend(); ++t, ++o)
		{
			c = (*t)->comp(*o);
			if (c) return c;
		}
		return 0;
	}
	ExprAST* clone() const;
	void reset();
	void clearCache(size_t targetSize);
	const StmtAST* getCurrentStmt() const { return &currentStmt; }
	const std::string& getName() const;
	void setName(const std::string& name);

	static BlockExprAST* parseCFile(const char* filename);
	static const std::vector<ExprAST*> parseCTplt(const char* tpltStr);

	static BlockExprAST* parsePythonFile(const char* filename);
	static const std::vector<ExprAST*> parsePythonTplt(const char* tpltStr);
};

class StopExprAST : public ExprAST
{
public:
	StopExprAST(const Location& loc) : ExprAST(loc, ExprAST::ExprType::STOP) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return ";"; }
	ExprAST* clone() const { return new StopExprAST(loc); }
};

class LiteralExprAST : public ExprAST
{
public:
	const std::string value;
	LiteralExprAST(const Location& loc, const char* value) : ExprAST(loc, ExprAST::ExprType::LITERAL), value(value) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((LiteralExprAST*)expr)->value == this->value;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return std::regex_replace(std::regex_replace(value, std::regex("\n"), "\\n"), std::regex("\r"), "\\r"); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const LiteralExprAST* _other = (const LiteralExprAST*)other;
		return this->value.compare(_other->value);
	}
	ExprAST* clone() const { return new ::LiteralExprAST(loc, value.c_str()); }
	const std::string& getValue() const;
};

class IdExprAST : public ExprAST
{
public:
	const std::string name;
	IdExprAST(const Location& loc, const char* name) : ExprAST(loc, ExprAST::ExprType::ID), name(name) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return name; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const IdExprAST* _other = (const IdExprAST*)other;
		return this->name.compare(_other->name);
	}
	ExprAST* clone() const { return new IdExprAST(loc, name.c_str()); }
	const std::string& getName() const;
};

class CastExprAST : public ExprAST
{
	const Cast* const cast;

public:
	CastExprAST(const Cast* cast, ExprAST* source) : ExprAST(source->loc, ExprAST::ExprType::CAST), cast(cast)
	{
		resolvedContext = cast->context;
		resolvedParams.push_back(source);
	}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		assert(0);
		return false;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const { assert(0); }
	std::string str() const { return "cast expression from " + getTypeNameInternal(cast->fromType) + " to " + getTypeNameInternal(cast->toType); }
	ExprAST* getDerivedExpr();
	ExprAST* clone() const { return new CastExprAST(cast, resolvedParams[0]->clone()); }
	const Cast* getCast() const;
};

class PlchldExprAST : public ExprAST
{
public:
	char p1;
	char* p2;
	bool allowCast;
	PlchldExprAST(const Location& loc, char p1) : ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p1), p2(nullptr), allowCast(false) {}
	PlchldExprAST(const Location& loc, char p1, const char* p2, bool allowCast) : ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p1), allowCast(allowCast)
	{
		size_t p2len = strlen(p2);
		this->p2 = new char[p2len + 1];
		memcpy(this->p2, p2, p2len + 1);
	}
	PlchldExprAST(const Location& loc, const char* p2) : ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p2[0])
	{
		size_t p2len = strlen(++p2);
		if (p2len && p2[p2len - 1] == '!')
		{
			allowCast = false;
			this->p2 = new char[p2len];
			memcpy(this->p2, p2, p2len - 1);
			this->p2[p2len - 1] = '\0';
		}
		else
		{
			allowCast = true;
			this->p2 = new char[p2len + 1];
			memcpy(this->p2, p2, p2len + 1);
		}
	}
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const { return '$' + std::string(1, p1) + (p2 == nullptr ? "" : '<' + std::string(p2) + (allowCast ? ">" : "!>")); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PlchldExprAST* _other = (const PlchldExprAST*)other;
		c = this->p1 - _other->p1;
		if (c) return c;
		c = (int)this->allowCast - (int)_other->allowCast;
		if (c) return c;
		if (this->p2 == nullptr || _other->p2 == nullptr) return this->p2 - _other->p2;
		return strcmp(this->p2, _other->p2);
	}
	ExprAST* clone() const { return p2 == nullptr ? new PlchldExprAST(loc, p1) : new PlchldExprAST(loc, p1, p2, allowCast); }
	char getP1() const;
	const char* getP2() const;
	bool getAllowCast() const;
};

class ParamExprAST : public ExprAST
{
public:
	int idx;
	ParamExprAST(const Location& loc, int idx) : ExprAST(loc, ExprAST::ExprType::PARAM), idx(idx) {}
	Variable codegen(BlockExprAST* parentBlock);
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((ParamExprAST*)expr)->idx == this->idx;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return '$' + std::to_string(idx); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const ParamExprAST* _other = (const ParamExprAST*)other;
		return this->idx - _other->idx;
	}
	ExprAST* clone() const { return new ParamExprAST(loc, idx); }
	int getIndex() const;
};

class EllipsisExprAST : public ExprAST
{
public:
	ExprAST* expr;
	EllipsisExprAST(const Location& loc, ExprAST* expr) : ExprAST(loc, ExprAST::ExprType::ELLIPSIS), expr(expr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(const BlockExprAST* block)
	{
		expr->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return expr->str() + ", ..."; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const EllipsisExprAST* _other = (const EllipsisExprAST*)other;
		return this->expr->comp(_other->expr);
	}
	ExprAST* clone() const { return new EllipsisExprAST(loc, expr->clone()); }
	const ExprAST* getExpr() const;
};

class ArgOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *var;
	ListExprAST* args;
	const std::string oopstr, copstr;
	ArgOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* var, ListExprAST* args)
		: ExprAST(loc, ExprAST::ExprType::ARGOP), op(op), var(var), args(args), oopstr(oopstr), copstr(copstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype
			&& ((ArgOpExprAST*)expr)->op == this->op
			&& var->match(block, ((ArgOpExprAST*)expr)->var, score)
			&& args->match(block, ((ArgOpExprAST*)expr)->args, score)
		;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		var->collectParams(block, ((ArgOpExprAST*)expr)->var, params, paramIdx);
		args->collectParams(block, ((ArgOpExprAST*)expr)->args, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		var->resolveTypes(block);
		args->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + oopstr + args->str() + copstr; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const ArgOpExprAST* _other = (const ArgOpExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		c = this->var->comp(_other->var);
		if (c) return c;
		return this->args->comp(_other->args);
	}
	ExprAST* clone() const { return new ArgOpExprAST(loc, op, oopstr.c_str(), copstr.c_str(), var->clone(), (ListExprAST*)args->clone()); }
	int getOp() const;
	const ExprAST* getVar() const;
	const ListExprAST* getArgs() const;
};

class EncOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *val;
	const std::string oopstr, copstr;
	EncOpExprAST(const Location& loc, int op, const char* oopstr, const char* copstr, ExprAST* val)
		: ExprAST(loc, ExprAST::ExprType::ENCOP), op(op), val(val), oopstr(oopstr), copstr(copstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype
			&& ((EncOpExprAST*)expr)->op == this->op
			&& val->match(block, ((EncOpExprAST*)expr)->val, score)
		;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		val->collectParams(block, ((EncOpExprAST*)expr)->val, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		val->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return oopstr + val->str() + copstr; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const EncOpExprAST* _other = (const EncOpExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->val->comp(_other->val);
	}
	ExprAST* clone() const { return new EncOpExprAST(loc, op, oopstr.c_str(), copstr.c_str(), val->clone()); }
	int getOp() const;
	const ExprAST* getVal() const;
	const std::string& getOOpStr() const;
	const std::string& getCOpStr() const;
};

class TerOpExprAST : public ExprAST
{
public:
	int op1, op2;
	ExprAST *a, *b, *c;
	const std::string opstr1, opstr2;
	TerOpExprAST(const Location& loc, int op1, int op2, const char* opstr1, const char* opstr2, ExprAST* a, ExprAST* b, ExprAST* c)
		: ExprAST(loc, ExprAST::ExprType::TEROP), op1(op1), op2(op2), a(a), b(b), c(c), opstr1(opstr1), opstr2(opstr2) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype
			&& ((TerOpExprAST*)expr)->op1 == this->op1
			&& ((TerOpExprAST*)expr)->op2 == this->op2
			&& a->match(block, ((TerOpExprAST*)expr)->a, score)
			&& b->match(block, ((TerOpExprAST*)expr)->b, score)
			&& c->match(block, ((TerOpExprAST*)expr)->c, score)
		;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((TerOpExprAST*)expr)->a, params, paramIdx);
		b->collectParams(block, ((TerOpExprAST*)expr)->b, params, paramIdx);
		c->collectParams(block, ((TerOpExprAST*)expr)->c, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		a->resolveTypes(block);
		b->resolveTypes(block);
		c->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return a->str() + " " + opstr1 + " " + b->str() + " " + opstr2 + " " + c->str(); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const TerOpExprAST* _other = (const TerOpExprAST*)other;
		c = this->op1 - _other->op1;
		if (c) return c;
		c = this->op2 - _other->op2;
		if (c) return c;
		c = this->a->comp(_other->a);
		if (c) return c;
		c = this->b->comp(_other->b);
		if (c) return c;
		return this->c->comp(_other->c);
	}
	ExprAST* clone() const { return new TerOpExprAST(loc, op1, op2, opstr1.c_str(), opstr2.c_str(), a->clone(), b->clone(), c->clone()); }
	int getOp1() const;
	int getOp2() const;
	const ExprAST* getA() const;
	const ExprAST* getB() const;
	const ExprAST* getC() const;
	const std::string& getOpStr1() const;
	const std::string& getOpStr2() const;
};

class PrefixExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	PrefixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a) : ExprAST(loc, ExprAST::ExprType::PREOP), op(op), a(a), opstr(opstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((PrefixExprAST*)expr)->op == this->op && a->match(block, ((PrefixExprAST*)expr)->a, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((PrefixExprAST*)expr)->a, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return (std::isalpha(opstr.back()) ? opstr + ' ' : opstr) + a->str(); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PrefixExprAST* _other = (const PrefixExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->a->comp(_other->a);
	}
	ExprAST* clone() const { return new PrefixExprAST(loc, op, opstr.c_str(), a->clone()); }
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

class PostfixExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	PostfixExprAST(const Location& loc, int op, const char* opstr, ExprAST* a) : ExprAST(loc, ExprAST::ExprType::POSTOP), op(op), a(a), opstr(opstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((PostfixExprAST*)expr)->op == this->op && a->match(block, ((PostfixExprAST*)expr)->a, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((PostfixExprAST*)expr)->a, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return a->str() + (std::isalpha(opstr.front()) ? opstr + ' ' : opstr); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PostfixExprAST* _other = (const PostfixExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->a->comp(_other->a);
	}
	ExprAST* clone() const { return new PostfixExprAST(loc, op, opstr.c_str(), a->clone()); }
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

class BinOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a, *b;
	const std::string opstr;
	PostfixExprAST a_post;
	PrefixExprAST b_pre;
	BinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a, ExprAST* b)
		: ExprAST(loc, ExprAST::ExprType::BINOP), op(op), a(a), b(b), opstr(opstr), a_post(a->loc, op, opstr, a), b_pre(b->loc, op, opstr, b) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((BinOpExprAST*)expr)->op == this->op && a->match(block, ((BinOpExprAST*)expr)->a, score) && b->match(block, ((BinOpExprAST*)expr)->b, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx);
		b->collectParams(block, ((BinOpExprAST*)expr)->b, params, paramIdx);
	}
	void resolveTypes(const BlockExprAST* block)
	{
		a->resolveTypes(block);
		b->resolveTypes(block);
		ExprAST::resolveTypes(block);
		a_post.resolveTypes(block);
		b_pre.resolveTypes(block);
	}
	std::string str() const { return a->str() + " " + opstr + " " + b->str(); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const BinOpExprAST* _other = (const BinOpExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		c = this->a->comp(_other->a);
		if (c) return c;
		return this->b->comp(_other->b);
	}
	ExprAST* clone() const { return new BinOpExprAST(loc, op, opstr.c_str(), a->clone(), b->clone()); }
	int getOp() const;
	const ExprAST* getA() const;
	const ExprAST* getB() const;
	const std::string& getOpStr() const;
};

class VarBinOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a;
	const std::string opstr;
	VarBinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a)
		: ExprAST(loc, ExprAST::ExprType::VARBINOP), op(op), a(a), opstr(opstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		if (expr->exprtype == this->exprtype)
			return ((VarBinOpExprAST*)expr)->op == this->op && a->match(block, ((VarBinOpExprAST*)expr)->a, score);
		else if (expr->exprtype == ExprAST::ExprType::BINOP)
			return ((BinOpExprAST*)expr)->op == this->op && this->match(block, ((BinOpExprAST*)expr)->a, score) && this->match(block, ((BinOpExprAST*)expr)->b, score);
		else
			return a->match(block, expr, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		size_t paramBegin = paramIdx;
		if (expr->exprtype == this->exprtype)
			a->collectParams(block, ((VarBinOpExprAST*)expr)->a, params, paramIdx);
		else if (expr->exprtype == ExprAST::ExprType::BINOP)
		{
			size_t& paramIdx1 = paramIdx, paramIdx2 = paramIdx;
			this->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx1);
			this->collectParams(block, ((BinOpExprAST*)expr)->b, params, paramIdx2);
			paramIdx = paramIdx1 > paramIdx2 ? paramIdx1 : paramIdx2;
		}
		else
			a->collectParams(block, expr, params, paramIdx);

		// Replace all non-list parameters within this VarBinOpExprAST with single-element lists,
		// because ellipsis parameters are expected to always be lists
		for (size_t i = paramBegin; i < paramIdx; ++i)
			if (params[i]->exprtype != ExprAST::ExprType::LIST)
				params[i] = new ListExprAST('\0', { params[i] });
	}
	void resolveTypes(const BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return a->str() + " " + opstr + " ..."; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const VarBinOpExprAST* _other = (const VarBinOpExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->a->comp(_other->a);
	}
	ExprAST* clone() const { return new VarBinOpExprAST(loc, op, opstr.c_str(), a->clone()); }
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

#endif
