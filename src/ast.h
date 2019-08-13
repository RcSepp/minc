#ifndef __AST_H
#define __AST_H

#include <string>
#include <cstring>
#include <map>
#include <list>
#include <vector>
#include <array>
#include <functional>
#include <cassert>

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
	CompileError(std::string msg, Location loc={0}) : msg(msg), loc(loc) {}
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

struct CodegenContext
{
	virtual Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params) = 0;
	virtual BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const = 0;
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
		STMT, LIST, STOP, LITERAL, ID, CAST, PLCHLD, PARAM, ELLIPSIS, CALL, SUBSCR, TPLT, BINOP, PREOP, BLOCK,
		NUM_EXPR_TYPES
	};
	const ExprType exprtype;

	// Resolved state
	CodegenContext* resolvedContext;
	std::vector<ExprAST*> resolvedParams;

	ExprAST(const Location& loc, ExprType exprtype) : loc(loc), exprtype(exprtype), resolvedContext(nullptr) {}
	virtual ~ExprAST() {}
	virtual Variable codegen(BlockExprAST* parentBlock);
	virtual BaseType* getType(const BlockExprAST* parentBlock) const
	{
		return resolvedContext ? resolvedContext->getType(parentBlock, resolvedParams) : nullptr;
	}
	virtual bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const = 0;
	virtual void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const = 0;
	virtual void resolveTypes(BlockExprAST* block);
	virtual std::string str() const = 0;
};

class ExprListAST : public ExprAST
{
public:
	std::vector<ExprAST*> exprs;
	char seperator;
	ExprListAST(char seperator) : ExprAST({0}, ExprAST::ExprType::LIST), seperator(seperator) {}
	ExprListAST(char seperator, std::vector<ExprAST*> exprs) : ExprAST({0}, ExprAST::ExprType::LIST), seperator(seperator), exprs(exprs) {}
	Variable codegen(BlockExprAST* parentBlock) { assert(0); }
	bool match(const BlockExprAST* block, const ExprAST* exprs, MatchScore& score) const;
	void collectParams(const BlockExprAST* block, ExprAST* exprs, std::vector<ExprAST*>& params) const;
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
			result += s + (*expriter)->str();
		return result;
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
	std::list<std::pair<const std::vector<ExprAST*>, CodegenContext*>> stmtreg;
	std::array<std::list<std::pair<const ExprAST*, CodegenContext*>>, ExprAST::NUM_EXPR_TYPES> exprreg;
public:
	void defineStatement(const std::vector<ExprAST*>& tplt, CodegenContext* stmt) { stmtreg.push_front({tplt, stmt}); }
	void importStatements(StatementRegister& stmtreg) { this->stmtreg.insert(this->stmtreg.begin(), stmtreg.stmtreg.begin(), stmtreg.stmtreg.end()); }
	const std::pair<const std::vector<ExprAST*>, CodegenContext*>* lookupStatement(const BlockExprAST* block, const ExprASTIter stmt, MatchScore& score, ExprASTIter& stmtEnd) const;

	void defineExpr(const ExprAST* tplt, CodegenContext* expr)
	{
		exprreg[tplt->exprtype].push_front({tplt, expr});
	}
	void importExprs(StatementRegister& stmtreg)
	{
		for (size_t i = 0; i < exprreg.size(); ++i)
			this->exprreg[i].insert(this->exprreg[i].begin(), stmtreg.exprreg[i].begin(), stmtreg.exprreg[i].end());
	}
	const std::pair<const ExprAST*, CodegenContext*>* lookupExpr(const BlockExprAST* block, const ExprAST* expr) const;
	void lookupExprCandidates(const BlockExprAST* block, const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>&>& candidates) const;
};

class StmtAST : public ExprAST
{
public:
	ExprASTIter begin, end;

	StmtAST(ExprASTIter exprBegin, ExprASTIter exprEnd, CodegenContext* context);
	void collectParams(const BlockExprAST* block, const std::vector<ExprAST*> tplt);
	Variable codegen(BlockExprAST* parentBlock);
bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const { assert(0); }
void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const { assert(0); }
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
};

class BlockExprAST : public ExprAST
{
private:
	StatementRegister stmtreg;
	std::map<std::string, Variable> scope;
	std::map<std::pair<BaseType*, BaseType*>, CodegenContext*> casts;
	std::vector<ExprAST*>* blockParams;
	BaseValue* blockParamsVal;
public:
	BlockExprAST* parent;
	std::vector<ExprAST*>* exprs;
	BlockExprAST(const Location& loc, std::vector<ExprAST*>* exprs)
		: ExprAST(loc, ExprAST::ExprType::BLOCK), parent(nullptr), exprs(exprs), blockParams(nullptr), blockParamsVal(nullptr) {}

	void defineStatement(const std::vector<ExprAST*>& tplt, CodegenContext* stmt)
	{
		for (ExprAST* tpltExpr: tplt)
			tpltExpr->resolveTypes(this);
		stmtreg.defineStatement(tplt, stmt);
	}
	const std::pair<const std::vector<ExprAST*>, CodegenContext*>* lookupStatement(ExprASTIter& exprs) const;

	void defineExpr(ExprAST* tplt, CodegenContext* expr)
	{
		tplt->resolveTypes(this);
		stmtreg.defineExpr(tplt, expr);
	}
	bool lookupExpr(ExprAST* expr) const;
	void lookupExprCandidates(const ExprAST* expr, std::multimap<MatchScore, const std::pair<const ExprAST*, CodegenContext*>&>& candidates) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
			block->stmtreg.lookupExprCandidates(this, expr, candidates);
	}

	void defineCast(BaseType* fromType, BaseType* toType, CodegenContext* context)
	{
		casts[{fromType, toType}] = context;
	}
	CodegenContext* lookupCast(BaseType* fromType, BaseType* toType) const
	{
		std::map<std::pair<BaseType*, BaseType*>, CodegenContext*>::const_iterator cast;
		for (const BlockExprAST* block = this; block; block = block->parent)
			if ((cast = block->casts.find({fromType, toType})) != block->casts.end())
				return cast->second;
		return nullptr;
	}
	void listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
	{
		for (const BlockExprAST* block = this; block; block = block->parent)
			for (const std::pair<std::pair<BaseType*, BaseType*>, CodegenContext*> cast: block->casts)
				casts.push_back(cast.first);
	}

	void import(BlockExprAST* block)
	{
		this->stmtreg.importStatements(block->stmtreg);
		this->stmtreg.importExprs(block->stmtreg);
		this->casts.insert(block->casts.begin(), block->casts.end());
	}

	void addToScope(std::string name, BaseType* type, BaseValue* var) { scope[name] = Variable(type, var); }
	const Variable* lookupScope(const std::string& name) const
	{
		std::map<std::string, Variable>::const_iterator var;
		for (const BlockExprAST* block = this; block; block = block->parent)
			if ((var = block->scope.find(name)) != block->scope.end())
				return &var->second;
		return nullptr;
	}
	const Variable* lookupScope(const std::string& name, bool& isCaptured) const
	{
		std::map<std::string, Variable>::const_iterator var;
		for (const BlockExprAST* block = this; block; block = block->parent)
			if ((var = block->scope.find(name)) != block->scope.end())
			{
				isCaptured = false; //block != this; //TODO: Fix and re-enable closure
				return &var->second;
			}
		isCaptured = false;
		return nullptr;
	}

	void setBlockParams(std::vector<ExprAST*>& params, BaseValue* paramsVal) { blockParams = new std::vector<ExprAST*>(params); blockParamsVal = paramsVal; }
	std::vector<ExprAST*>* getBlockParams() const
	{
		std::vector<ExprAST*>* params;
		for (const BlockExprAST* block = this; block; block = block->parent)
			if ((params = block->blockParams) != nullptr)
				return params;
		return nullptr;
	}
	BaseValue* getBlockParamsVal()
	{
		BaseValue* paramsVal;
		for (const BlockExprAST* block = this; block; block = block->parent)
			if ((paramsVal = block->blockParamsVal) != nullptr)
				return paramsVal;
		return nullptr;
	}

	Variable codegen(BlockExprAST* parentBlock);
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const {}
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
};

class StopExprAST : public ExprAST
{
public:
	StopExprAST(const Location& loc) : ExprAST(loc, ExprAST::ExprType::STOP) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const {}
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const {}
	std::string str() const { return std::string(value); }
};

class IdExprAST : public ExprAST
{
public:
	const char* name;
	IdExprAST(const Location& loc, const char* name) : ExprAST(loc, ExprAST::ExprType::ID), name(name) {}
	bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const
	{
		return expr->exprtype == this->exprtype && strcmp(((IdExprAST*)expr)->name, this->name) == 0;
	}
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const {}
	std::string str() const { return name; }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const { assert(0); }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const;
	std::string str() const { return '$' + std::string(1, p1) + (p2 == nullptr ? "" : '<' + std::string(p2) + '>'); }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		if (dynamicIdx && this->dynamicIdx)
			this->dynamicIdx->collectParams(block, dynamicIdx, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		if (dynamicIdx)
			dynamicIdx->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return '$' + (dynamicIdx ? '[' + dynamicIdx->str() + ']' : std::to_string(staticIdx)); }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		this->expr->collectParams(block, expr->exprtype == ExprAST::ExprType::ELLIPSIS ? ((EllipsisExprAST*)expr)->expr : expr, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		expr->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return "..."; }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		var->collectParams(block, ((CallExprAST*)expr)->var, params);
		args->collectParams(block, ((CallExprAST*)expr)->args, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		args->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "(" + args->str() + ")"; }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		var->collectParams(block, ((SubscrExprAST*)expr)->var, params);
		idx->collectParams(block, ((SubscrExprAST*)expr)->idx, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		idx->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "[" + idx->str() + "]"; }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		var->collectParams(block, ((TpltExprAST*)expr)->var, params);
		args->collectParams(block, ((TpltExprAST*)expr)->args, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		var->resolveTypes(block);
		args->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return var->str() + "<" + args->str() + ">"; }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params);
		b->collectParams(block, ((BinOpExprAST*)expr)->b, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		a->resolveTypes(block);
		b->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return a->str() + " " + opstr + " " + b->str(); }
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
	void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params) const
	{
		a->collectParams(block, ((BinOpExprAST*)expr)->a, params);
	}
	void resolveTypes(BlockExprAST* block)
	{
		a->resolveTypes(block);
		ExprAST::resolveTypes(block);
	}
	std::string str() const { return opstr + a->str(); }
};

#endif
