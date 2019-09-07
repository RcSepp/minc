#ifndef __AST_H
#define __AST_H

#include <string>
#include <cstring>
#include <map>
#include <unordered_set>
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <cassert>
#include <regex>

#include "api.h"

typedef std::vector<ExprAST*>::const_iterator ExprASTIter;
typedef int MatchScore;

struct Location
{
	const char* filename;
	unsigned begin_line, begin_col;
	unsigned end_line, end_col;
};

struct CompileError
{
	const Location loc;
	const std::string msg;
	std::vector<std::string> hints;
	CompileError(std::string msg, Location loc={0}) : msg(msg), loc(loc) {}
	void addHint(const std::string& hint) { hints.push_back(hint); }
};
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
	const Location loc;
	enum ExprType {
		STMT, LIST, STOP, LITERAL, ID, CAST, PLCHLD, PARAM, ELLIPSIS, CALL, PREC, SUBSCR, TPLT, TEROP, BINOP, PREOP, POSTOP, BLOCK,
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
	virtual void resolveTypes(BlockExprAST* block);
	virtual std::string str() const = 0;
	virtual int comp(const ExprAST* other) const { return this->exprtype - other->exprtype; }
};
bool operator<(const ExprAST& left, const ExprAST& right);

namespace std
{
	template<> struct less<ExprAST*>
	{
		bool operator()(const ExprAST* lhs, const ExprAST* rhs) const { return lhs->comp(rhs) < 0; }
	};
}

class ExprListAST : public ExprAST
{
public:
	std::vector<ExprAST*> exprs;
	char seperator;
	ExprListAST(char seperator) : ExprAST({0}, ExprAST::ExprType::LIST), seperator(seperator) {}
	ExprListAST(char seperator, std::vector<ExprAST*> exprs) : ExprAST({0}, ExprAST::ExprType::LIST), seperator(seperator), exprs(exprs) {}
	Variable codegen(BlockExprAST* parentBlock) { assert(0); }
	bool match(const BlockExprAST* block, const ExprAST* exprs, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	void resolveTypes(BlockExprAST* block) { for (auto expr: exprs) expr->resolveTypes(block); }
	std::string str() const
	{
		if (exprs.empty())
			return "";

		std::string s;
		const std::string _(1, ' ');
		switch(seperator)
		{
		case '\0': s = _; break;
		case ',': case ';': s = seperator + _; break;
		default: s = _ + seperator + _; break;
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
		const ExprListAST* _other = (const ExprListAST*)other;
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
};
std::vector<ExprAST*>::iterator begin(ExprListAST& exprs);
std::vector<ExprAST*>::iterator begin(ExprListAST* exprs);
std::vector<ExprAST*>::iterator end(ExprListAST& exprs);
std::vector<ExprAST*>::iterator end(ExprListAST* exprs);

class StatementRegister
{
private:
	std::map<const ExprListAST, CodegenContext*> stmtreg;
	std::array<std::map<const ExprAST*, CodegenContext*>, ExprAST::NUM_EXPR_TYPES> exprreg;
public:
	void defineStatement(const std::vector<ExprAST*>& tplt, CodegenContext* stmt) { stmtreg[ExprListAST('\0', tplt)] = stmt; }
	const std::pair<const ExprListAST, CodegenContext*>* lookupStatement(const BlockExprAST* block, const ExprASTIter stmt, ExprASTIter& stmtEnd, MatchScore& score) const;

	void defineExpr(const ExprAST* tplt, CodegenContext* expr)
	{
		exprreg[tplt->exprtype][tplt] = expr;
	}
	const std::pair<const ExprAST*const, CodegenContext*>* lookupExpr(const BlockExprAST* block, const ExprAST* expr, MatchScore& bestScore) const;
	void lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*const, CodegenContext*>&>& candidates) const;
};

class StmtAST : public ExprAST
{
public:
	ExprASTIter begin, end;

	StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context);
	void collectParams(const BlockExprAST* block, const ExprListAST& tplt);
	Variable codegen(BlockExprAST* parentBlock);
bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const { assert(0); }
void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const { assert(0); }
void resolveTypes(BlockExprAST* block) { assert(0); }
	std::string str() const
	{
		if (begin == end)
			return "";
		std::string result = (*begin)->str();
		for (ExprASTIter expr = begin; ++expr != end;)
			result += ' ' + (*expr)->str();
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
};

class BlockExprAST : public ExprAST
{
private:
	StatementRegister stmtreg;
	std::map<std::string, Variable> scope;
	std::map<std::pair<BaseType*, BaseType*>, CodegenContext*> casts;
	BaseScopeType* scopeType;

	const std::pair<const ExprListAST, CodegenContext*>* lookupStatementInternal(const BlockExprAST* block, ExprASTIter& exprs, ExprASTIter& bestStmtEnd, MatchScore& bestScore) const;
	const std::pair<const ExprAST*const, CodegenContext*>* lookupExprInternal(const BlockExprAST* block, const ExprAST* expr, MatchScore& bestScore) const;

public:
	BlockExprAST* parent;
	std::unordered_set<BlockExprAST*> references;
	std::vector<ExprAST*>* exprs;
	std::vector<Variable> blockParams;
	BlockExprAST(const Location& loc, std::vector<ExprAST*>* exprs)
		: ExprAST(loc, ExprAST::ExprType::BLOCK), scopeType(nullptr), parent(nullptr), exprs(exprs) {}

	void defineStatement(const std::vector<ExprAST*>& tplt, CodegenContext* stmt)
	{
		for (ExprAST* tpltExpr: tplt)
			tpltExpr->resolveTypes(this);
		stmtreg.defineStatement(tplt, stmt);
	}
	const std::pair<const ExprListAST, CodegenContext*>* lookupStatement(ExprASTIter& exprs, const ExprASTIter exprEnd) const;

	void defineExpr(ExprAST* tplt, CodegenContext* expr)
	{
		tplt->resolveTypes(this);
		stmtreg.defineExpr(tplt, expr);
	}
	bool lookupExpr(ExprAST* expr) const;
	void lookupExprCandidates(const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*const, CodegenContext*>&>& candidates) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			block->stmtreg.lookupExprCandidates(this, expr, candidates);
			for (const BlockExprAST* ref: block->references)
				ref->stmtreg.lookupExprCandidates(this, expr, candidates);
		}
	}

	void defineCast(BaseType* fromType, BaseType* toType, CodegenContext* context)
	{
		casts[std::make_pair(fromType, toType)] = context;
	}
	CodegenContext* lookupCast(BaseType* fromType, BaseType* toType) const
	{
		const std::pair<BaseType*, BaseType*>& key = std::make_pair(fromType, toType);
		std::map<std::pair<BaseType*, BaseType*>, CodegenContext*>::const_iterator cast;
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			if ((cast = block->casts.find(key)) != block->casts.end())
				return cast->second;
			for (const BlockExprAST* ref: block->references)
				if ((cast = ref->casts.find(key)) != ref->casts.end())
					return cast->second;
		}
		return nullptr;
	}
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
		{
			for (const std::pair<std::pair<BaseType*, BaseType*>, CodegenContext*> cast: block->casts)
				casts.push_back(cast.first);
			for (const BlockExprAST* ref: block->references)
				for (const std::pair<std::pair<BaseType*, BaseType*>, CodegenContext*> cast: ref->casts)
					casts.push_back(cast.first);
		}
	}

	void import(BlockExprAST* importBlock)
	{
		const BlockExprAST* block;

		// Import importBlock
		for (block = this; block; block = block->parent)
			if (importBlock == block || block->references.find(importBlock) != block->references.end())
				break;
		if (block == nullptr)
			references.insert(importBlock);

		// Import all references of importBlock
		for (BlockExprAST* importRef: importBlock->references)
		{
			for (block = this; block; block = block->parent)
				if (importRef == block || block->references.find(importRef) != block->references.end())
					break;
			if (block == nullptr)
				references.insert(importRef);
		}
	}

	void defineSymbol(std::string name, BaseType* type, BaseValue* var) { scope[name] = Variable(type, var); }
	const Variable* lookupSymbol(const std::string& name) const;
	Variable* importSymbol(const std::string& name);

	void setScopeType(BaseScopeType* scopeType) { this->scopeType = scopeType; }

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
};

class LiteralExprAST : public ExprAST
{
public:
	const char* value;
	LiteralExprAST(const Location& loc, const char* value) : ExprAST(loc, ExprAST::ExprType::LITERAL), value(value) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && strcmp(((LiteralExprAST*)expr)->value,  this->value) == 0;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return std::regex_replace(std::regex_replace(value, std::regex("\n"), "\\n"), std::regex("\r"), "\\r"); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const LiteralExprAST* _other = (const LiteralExprAST*)other;
		return strcmp(this->value, _other->value);
	}
};

class IdExprAST : public ExprAST
{
public:
	const char* name;
	IdExprAST(const Location& loc, const char* name) : ExprAST(loc, ExprAST::ExprType::ID), name(name) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const {}
	std::string str() const { return name; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const IdExprAST* _other = (const IdExprAST*)other;
		return strcmp(this->name, _other->name);
	}
};

class CastExprAST : public ExprAST
{
public:
	CastExprAST(const Location& loc) : ExprAST(loc, ExprAST::ExprType::CAST) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		assert(0);
		return false;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const { assert(0); }
	std::string str() const { assert(0); return ""; }
};

class PlchldExprAST : public ExprAST
{
public:
	char p1;
	const char* p2;
	PlchldExprAST(const Location& loc, char p1) : ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p1), p2(nullptr) {}
	PlchldExprAST(const Location& loc, const char* p2) : ExprAST(loc, ExprAST::ExprType::PLCHLD), p1(p2[0]), p2(p2 + 1) {}
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const;
	std::string str() const { return '$' + std::string(1, p1) + (p2 == nullptr ? "" : '<' + std::string(p2) + '>'); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PlchldExprAST* _other = (const PlchldExprAST*)other;
		c = this->p1 - _other->p1;
		if (c) return c;
		if (this->p2 == nullptr || _other->p2 == nullptr) return this->p2 - _other->p2;
		return strcmp(this->p2, _other->p2);
	}
};

class ParamExprAST : public ExprAST
{
public:
	int staticIdx;
	ExprAST* dynamicIdx;
	ParamExprAST(const Location& loc, int idx) : ExprAST(loc, ExprAST::ExprType::PARAM), staticIdx(idx), dynamicIdx(nullptr) {}
	ParamExprAST(const Location& loc, ExprAST* idx) : ExprAST(loc, ExprAST::ExprType::PARAM), staticIdx(-1), dynamicIdx(idx) {}
	Variable codegen(BlockExprAST* parentBlock);
	BaseType* getType(const BlockExprAST* parentBlock) const;
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return staticIdx == this->staticIdx && (dynamicIdx == nullptr || this->dynamicIdx->match(block, dynamicIdx, score));
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		if (dynamicIdx && this->dynamicIdx)
			this->dynamicIdx->collectParams(block, dynamicIdx, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		if (dynamicIdx)
			dynamicIdx->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return '$' + (dynamicIdx ? '[' + dynamicIdx->str() + ']' : std::to_string(staticIdx)); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const ParamExprAST* _other = (const ParamExprAST*)other;
		return this->staticIdx - _other->staticIdx;
	}
};

class EllipsisExprAST : public ExprAST
{
public:
	ExprAST* expr;
	EllipsisExprAST(const Location& loc, ExprAST* expr) : ExprAST(loc, ExprAST::ExprType::ELLIPSIS), expr(expr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return this->expr->match(block, expr->exprtype == ExprAST::ExprType::ELLIPSIS ? ((EllipsisExprAST*)expr)->expr : expr, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		this->expr->collectParams(block, expr->exprtype == ExprAST::ExprType::ELLIPSIS ? ((EllipsisExprAST*)expr)->expr : expr, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
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
};

class CallExprAST : public ExprAST
{
public:
	ExprAST *var;
	ExprListAST* args;
	CallExprAST(const Location& loc, ExprAST* var, ExprListAST* args) : ExprAST(loc, ExprAST::ExprType::CALL), var(var), args(args) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && var->match(block, ((CallExprAST*)expr)->var, score) && args->match(block, ((CallExprAST*)expr)->args, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		var->collectParams(block, ((CallExprAST*)expr)->var, params, paramIdx);
		args->collectParams(block, ((CallExprAST*)expr)->args, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		args->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "(" + args->str() + ")"; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const CallExprAST* _other = (const CallExprAST*)other;
		c = this->var->comp(_other->var);
		if (c) return c;
		return this->args->comp(_other->args);
	}
};

class PrecExprAST : public ExprAST
{
public:
	ExprAST *val;
	PrecExprAST(const Location& loc, ExprAST* val) : ExprAST(loc, ExprAST::ExprType::PREC), val(val) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && val->match(block, ((PrecExprAST*)expr)->val, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		val->collectParams(block, ((PrecExprAST*)expr)->val, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		val->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return "(" + val->str() + ")"; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PrecExprAST* _other = (const PrecExprAST*)other;
		return this->val->comp(_other->val);
	}
};

class SubscrExprAST : public ExprAST
{
public:
	ExprAST *var;
	ExprListAST* idx;
	SubscrExprAST(const Location& loc, ExprAST* var, ExprListAST* idx) : ExprAST(loc, ExprAST::ExprType::SUBSCR), var(var), idx(idx) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && var->match(block, ((SubscrExprAST*)expr)->var, score) && idx->match(block, ((SubscrExprAST*)expr)->idx, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		var->collectParams(block, ((SubscrExprAST*)expr)->var, params, paramIdx);
		idx->collectParams(block, ((SubscrExprAST*)expr)->idx, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		idx->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "[" + idx->str() + "]"; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const SubscrExprAST* _other = (const SubscrExprAST*)other;
		c = this->var->comp(_other->var);
		if (c) return c;
		return this->idx->comp(_other->idx);
	}
};

class TpltExprAST : public ExprAST
{
public:
	ExprAST *var;
	ExprListAST* args;
	TpltExprAST(const Location& loc, ExprAST* var, ExprListAST* args) : ExprAST(loc, ExprAST::ExprType::TPLT), var(var), args(args) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && var->match(block, ((TpltExprAST*)expr)->var, score) && args->match(block, ((TpltExprAST*)expr)->args, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		var->collectParams(block, ((TpltExprAST*)expr)->var, params, paramIdx);
		args->collectParams(block, ((TpltExprAST*)expr)->args, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		args->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "<" + args->str() + ">"; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const TpltExprAST* _other = (const TpltExprAST*)other;
		c = this->var->comp(_other->var);
		if (c) return c;
		return this->args->comp(_other->args);
	}
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
	void resolveTypes(BlockExprAST* block)
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
};

class BinOpExprAST : public ExprAST
{
public:
	int op;
	ExprAST *a, *b;
	const std::string opstr;
	BinOpExprAST(const Location& loc, int op, const char* opstr, ExprAST* a, ExprAST* b) : ExprAST(loc, ExprAST::ExprType::BINOP), op(op), a(a), b(b), opstr(opstr) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && ((BinOpExprAST*)expr)->op == this->op && a->match(block, ((BinOpExprAST*)expr)->a, score) && b->match(block, ((BinOpExprAST*)expr)->b, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx);
		b->collectParams(block, ((BinOpExprAST*)expr)->b, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		a->resolveTypes(block);
		b->resolveTypes(block);
		ExprAST::resolveTypes(block);
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
		return expr->exprtype == this->exprtype && ((BinOpExprAST*)expr)->op == this->op && a->match(block, ((BinOpExprAST*)expr)->a, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return opstr + a->str(); }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PrefixExprAST* _other = (const PrefixExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->a->comp(_other->a);
	}
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
		return expr->exprtype == this->exprtype && ((BinOpExprAST*)expr)->op == this->op && a->match(block, ((BinOpExprAST*)expr)->a, score);
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params, paramIdx);
	}
	void resolveTypes(BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return a->str() + opstr; }
	int comp(const ExprAST* other) const
	{
		int c = ExprAST::comp(other);
		if (c) return c;
		const PostfixExprAST* _other = (const PostfixExprAST*)other;
		c = this->op - _other->op;
		if (c) return c;
		return this->a->comp(_other->a);
	}
};

#endif
