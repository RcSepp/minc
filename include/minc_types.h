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

struct MincStackSymbol
{
	MincObject* type;
	MincBlockExpr* scope;
	size_t location;
	MincStackSymbol() = default;
	MincStackSymbol(const MincStackSymbol& v) = default;
	MincStackSymbol(MincObject* type, MincBlockExpr* scope, size_t location) : type(type), scope(scope), location(location) {}
};

struct MincBuildtime
{
	MincBlockExpr* parentBlock;
	MincSymbol result;
};

struct MincStackFrame
{
	union {
		size_t stackPointer;
		unsigned char* heapPointer;
	};
	size_t stmtIndex;
	MincStackFrame* next; //TODO: Only used for heap frame. Consider separating this struct into MincStackFrame and MincHeapFrame

	MincStackFrame() : heapPointer(nullptr), next(nullptr) {}
};

struct MincRuntime
{
	const MincBlockExpr* parentBlock;
	const MincExpr* currentExpr;
	MincObject* result;
	MincObject* exceptionType;
	bool resume;
	MincStackFrame* const stackFrames;
	MincStackFrame* heapFrame;
	size_t currentStackPointerIndex;
	const size_t stackSize;
	unsigned char* const stack;
	size_t currentStackSize;

	MincRuntime();
	MincRuntime(MincBlockExpr* parentBlock, bool resume);
	~MincRuntime();
};

struct MincEnteredBlockExpr
{
	MincRuntime& runtime;
	const MincBlockExpr* block;
	MincStackFrame* const prevStackFrame;
	MincEnteredBlockExpr(MincRuntime& runtime, const MincBlockExpr* block);
	~MincEnteredBlockExpr();
	bool run();
	void exit();
};

struct MincKernel
{
	virtual ~MincKernel() {}
	virtual MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params) { return this; }
	virtual void dispose(MincKernel* kernel) {}
	virtual bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params) = 0;
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
	CompileError(MincBlockExpr* scope, MincLocation loc, const char* fmt, ...);
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

typedef void (*BuildBlock)(MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs);
typedef bool (*RunBlock)(MincRuntime& runtime, const std::vector<MincExpr*>& params, void* exprArgs);
typedef MincObject* (*ExprTypeBlock)(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs);
typedef void (*ImptBlock)(MincSymbol& symbol, MincScopeType* fromScope, MincScopeType* toScope);
typedef void (*StepEvent)(const MincExpr* loc, StepEventType type, void* eventArgs);

#endif