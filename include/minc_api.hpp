#ifndef __MINC_API_HPP
#define __MINC_API_HPP

#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "minc_types.h"

class ExprAST
{
public:
	virtual ~ExprAST() {}
	virtual Variable codegen(::BlockExprAST* parentBlock) { assert(0); return Variable(nullptr, nullptr); }
	virtual BaseType* getType(const ::BlockExprAST* parentBlock) const { assert(0); return nullptr; }
	virtual bool match(const ::BlockExprAST* block, const ::ExprAST* expr, MatchScore& score) const = 0;
	virtual void collectParams(const ::BlockExprAST* block, ::ExprAST* expr, std::vector<::ExprAST*>& params, size_t& paramIdx) const = 0;
	virtual void resolveTypes(const ::BlockExprAST* block) { assert(0); }
	virtual std::string str() const = 0;
	virtual std::string shortStr() const { assert(0); return ""; }
	virtual int comp(const ::ExprAST* other) const = 0;
};

class BlockExprAST : public ExprAST
{
public:
	std::string name;

	virtual ~BlockExprAST() {}
	void defineStmt(const std::vector<::ExprAST*>& tplt, CodegenContext* stmt);
	void lookupStmtCandidates(const ExprListAST* stmt, std::multimap<MatchScore, const std::pair<const ExprListAST*, CodegenContext*>>& candidates) const;
	size_t countStmts() const;
	void iterateStmts(std::function<void(const ExprListAST* tplt, const CodegenContext* stmt)> cbk) const;
	void defineAntiStmt(CodegenContext* stmt);
	void defineExpr(::ExprAST* tplt, CodegenContext* expr);
	void lookupExprCandidates(const ::ExprAST* expr, std::multimap<MatchScore, const std::pair<const ::ExprAST*, CodegenContext*>>& candidates) const;
	size_t countExprs() const;
	void iterateExprs(std::function<void(const ::ExprAST* tplt, const CodegenContext* expr)> cbk) const;
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
	BlockExprAST* clone() const;

	static BlockExprAST* parseCFile(const char* filename);
	static const std::vector<ExprAST*> parseCTplt(const char* tpltStr);

	static BlockExprAST* parsePythonFile(const char* filename);
	static const std::vector<ExprAST*> parsePythonTplt(const char* tpltStr);
};

class LiteralExprAST : public ExprAST
{
public:
	const std::string& getValue() const;
};

#endif