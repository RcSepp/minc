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
	virtual Variable codegen(BlockExprAST* parentBlock) { assert(0); return Variable(nullptr, nullptr); }
	virtual BaseType* getType(const BlockExprAST* parentBlock) const { assert(0); return nullptr; }
	virtual bool match(const BlockExprAST* block, const ExprAST* expr, MatchScore& score) const = 0;
	virtual void collectParams(const BlockExprAST* block, ExprAST* expr, std::vector<ExprAST*>& params, size_t& paramIdx) const = 0;
	virtual void resolveTypes(const BlockExprAST* block) { assert(0); }
	virtual std::string str() const = 0;
	virtual std::string shortStr() const { assert(0); return ""; }
	virtual int comp(const ExprAST* other) const = 0;
};

class ListExprAST : public ExprAST
{
public:
	std::vector<ExprAST*>::iterator begin();
	std::vector<ExprAST*>::const_iterator cbegin();
	std::vector<ExprAST*>::iterator end();
	std::vector<ExprAST*>::const_iterator cend();
	size_t size() const;
	ExprAST* at(size_t index);
	const ExprAST* at(size_t index) const;
	ExprAST* operator[](size_t index);
	const ExprAST* operator[](size_t index) const;
	void push_back(ExprAST* expr);
	ListExprAST* clone() const;
	const char getSeparator() const;
	const void setSeparator(char separator);
};

class StmtAST : public ExprAST
{
public:
	StmtAST* clone() const;
};

class BlockExprAST : public ExprAST
{
public:
	void defineStmt(const std::vector<ExprAST*>& tplt, CodegenContext* stmt);
	bool lookupStmt(std::vector<ExprAST*>::const_iterator beginExpr, StmtAST& stmt) const;
	void lookupStmtCandidates(const ListExprAST* stmt, std::multimap<MatchScore, const std::pair<const ListExprAST*, CodegenContext*>>& candidates) const;
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
	BlockExprAST* clone() const;
	void reset();
	void clearCache(size_t targetSize);
	const StmtAST* getCurrentStmt() const;
	const std::string& getName() const;
	void setName(const std::string& name);

	static BlockExprAST* parseCFile(const char* filename);
	static const std::vector<ExprAST*> parseCTplt(const char* tpltStr);

	static BlockExprAST* parsePythonFile(const char* filename);
	static const std::vector<ExprAST*> parsePythonTplt(const char* tpltStr);
};

class LiteralExprAST : public ExprAST
{
public:
	LiteralExprAST* clone() const;
	const std::string& getValue() const;
};

class IdExprAST : public ExprAST
{
public:
	IdExprAST* clone() const;
	const std::string& getName() const;
};

class CastExprAST : public ExprAST
{
	const Cast* const cast;

public:
	ExprAST* getDerivedExpr();
	CastExprAST* clone() const;
	const Cast* getCast() const;
};

class PlchldExprAST : public ExprAST
{
public:
	PlchldExprAST* clone() const;
	char getP1() const;
	const char* getP2() const;
	bool getAllowCast() const;
};

class ParamExprAST : public ExprAST
{
public:
	ParamExprAST* clone() const;
	int getIndex() const;
};

class EllipsisExprAST : public ExprAST
{
public:
	EllipsisExprAST* clone() const;
	const ExprAST* getExpr() const;
};

class ArgOpExprAST : public ExprAST
{
public:
	ArgOpExprAST* clone();
	int getOp() const;
	const ExprAST* getVar() const;
	const ListExprAST* getArgs() const;
};

class EncOpExprAST : public ExprAST
{
public:
	EncOpExprAST* clone() const;
	int getOp() const;
	const ExprAST* getVal() const;
	const std::string& getOOpStr() const;
	const std::string& getCOpStr() const;
};

class TerOpExprAST : public ExprAST
{
public:
	TerOpExprAST* clone() const;
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
	PrefixExprAST* clone() const;
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

class PostfixExprAST : public ExprAST
{
public:
	PostfixExprAST* clone() const;
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

class BinOpExprAST : public ExprAST
{
public:
	BinOpExprAST* clone() const;
	int getOp() const;
	const ExprAST* getA() const;
	const ExprAST* getB() const;
	const std::string& getOpStr() const;
};

class VarBinOpExprAST : public ExprAST
{
public:
	VarBinOpExprAST* clone() const;
	int getOp() const;
	const ExprAST* getA() const;
	const std::string& getOpStr() const;
};

#endif