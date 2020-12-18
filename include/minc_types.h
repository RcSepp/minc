#ifndef __MINC_TYPES_H
#define __MINC_TYPES_H

#include <exception>
#include <iostream>
#include <string>
#include <vector>

struct MincObject {};
struct MincScopeType {};

class MincExpr;
class MincListExpr;
class MincStmt;
class MincBlockExpr;
class MincStopExpr;
class MincLiteralExpr;
class MincIdExpr;
class MincCastExpr;
class MincPlchldExpr;
class MincParamExpr;
class MincEllipsisExpr;
class MincArgOpExpr;
class MincEncOpExpr;
class MincTerOpExpr;
class MincPrefixExpr;
class MincPostfixExpr;
class MincBinOpExpr;
class MincVarBinOpExpr;

typedef int MatchScore;

enum StepEventType { STEP_IN, STEP_OUT, STEP_SUSPEND, STEP_RESUME };

struct MincSymbol
{
	MincObject* type;
	MincObject* value;
	MincSymbol() = default;
	MincSymbol(const MincSymbol& v) = default;
	MincSymbol(MincObject* type, MincObject* value) : type(type), value(value) {}
};

struct MincSymbolId
{
	uint32_t i;
	uint16_t p, r;
	static const MincSymbolId NONE;
};
bool operator==(const MincSymbolId& left, const MincSymbolId& right);
bool operator!=(const MincSymbolId& left, const MincSymbolId& right);

struct MincRuntime
{
	MincBlockExpr* parentBlock;
	bool resume;
	MincSymbol result;
};

struct MincKernel
{
	virtual ~MincKernel() {}
	virtual MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) { return this; }
	virtual void dispose(MincKernel* kernel) {}
	virtual bool run(MincRuntime& runtime, std::vector<MincExpr*>& params) = 0;
	virtual MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const = 0;
};

struct MincCast
{
	MincObject* const fromType;
	MincObject* const toType;
	MincKernel* const kernel;
	MincCast() = default;
	MincCast(const MincCast&) = default;
	MincCast(MincObject* fromType, MincObject* toType, MincKernel* kernel)
		: fromType(fromType), toType(toType), kernel(kernel) {}
	virtual int getCost() const = 0;
	virtual MincCast* derive() const = 0;
};

struct MincLocation
{
	const char* filename;
	unsigned begin_line, begin_column;
	unsigned end_line, end_column;
};

struct MincException : public std::exception
{
private:
	mutable int* refcount;

protected:
	char** const msg;

public:
	const MincLocation loc;
	MincException(const char* msg, MincLocation loc={ nullptr, 0, 0, 0, 0 });
	MincException(std::string msg, MincLocation loc={ nullptr, 0, 0, 0, 0 });
	MincException(MincLocation loc={ nullptr, 0, 0, 0, 0 });
	MincException(const MincException& other);
	~MincException();
	const char* what() const noexcept { return *msg; }
	void print(std::ostream& out=std::cerr) const noexcept;
};
struct CompileError : public MincException
{
	CompileError(const char* msg, MincLocation loc={ nullptr, 0, 0, 0, 0 });
	CompileError(std::string msg, MincLocation loc={ nullptr, 0, 0, 0, 0 });
	CompileError(const MincBlockExpr* scope, MincLocation loc, const char* fmt, ...);
};
struct UndefinedStmtException : public CompileError
{
	UndefinedStmtException(const MincStmt* stmt);
};
struct UndefinedExprException : public CompileError
{
	UndefinedExprException(const MincExpr* expr);
};
struct UndefinedIdentifierException : public CompileError
{
	UndefinedIdentifierException(const MincIdExpr* id);
};
struct InvalidTypeException : public CompileError
{
	InvalidTypeException(const MincPlchldExpr* plchld);
};

typedef void (*StmtBlock)(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs);
typedef MincSymbol (*ExprBlock)(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs);
typedef bool (*RunBlock)(MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs);
typedef MincObject* (*ExprTypeBlock)(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs);
typedef void (*ImptBlock)(MincSymbol& symbol, MincScopeType* fromScope, MincScopeType* toScope);
typedef void (*StepEvent)(const MincExpr* loc, StepEventType type, void* eventArgs);

#endif