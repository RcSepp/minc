#ifndef __MINC_TYPES_H
#define __MINC_TYPES_H

#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <threads.h>
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

struct MincRunner;
struct MincKernel;

typedef int MatchScore;

enum MincFlavor { C_FLAVOR, PYTHON_FLAVOR, GO_FLAVOR, UNKNOWN_FLAVOR };

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
struct UndefinedStmtException : public CompileError { UndefinedStmtException(const MincStmt* stmt); };
struct UndefinedExprException : public CompileError { UndefinedExprException(const MincExpr* expr); };
struct UndefinedIdentifierException : public CompileError { UndefinedIdentifierException(const MincIdExpr* id); };
struct InvalidTypeException : public CompileError { InvalidTypeException(const MincPlchldExpr* plchld); };
struct TooManyErrorsException : public CompileError { TooManyErrorsException(const MincLocation& loc); };

struct MincBuildtime
{
	MincBlockExpr* parentBlock;
	MincSymbol result;
	struct Settings
	{
		bool debug;
		unsigned maxErrors;
	} settings;
	struct Hooks
	{
	} hooks;
	struct Outputs
	{
		std::vector<CompileError> errors;
	} outputs;
	std::set<MincRunner*> runners;
	std::map<const char*, std::set<MincRunner*>> fileRunners;
	MincRunner* currentRunner;
	std::set<MincRunner*>* currentFileRunners;

	MincBuildtime(MincBlockExpr* parentBlock=nullptr);
};

struct MincInteropData
{
	MincExpr* followupExpr;
	MincObject* exprResult;
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
	MincInteropData& interopData;

	MincRuntime(MincBlockExpr* parentBlock, bool resume, MincInteropData& interopData);
	~MincRuntime();
	MincObject* getStackSymbol(const MincStackSymbol* stackSymbol);
	MincObject* getStackSymbolOfNextStackFrame(const MincStackSymbol* stackSymbol);
};

struct MincRunner
{
	const char* const name;
	mtx_t mutex;
	MincRunner(const char* name) : name(name) {}
	virtual void buildBegin(MincBuildtime& buildtime) {}
	virtual void buildEnd(MincBuildtime& buildtime) {}
	virtual void buildBeginFile(MincBuildtime& buildtime, const char* path) {}
	virtual void buildEndFile(MincBuildtime& buildtime, const char* path) {}
	virtual void buildStmt(MincBuildtime& buildtime, MincStmt* stmt);
	virtual void buildSuspendStmt(MincBuildtime& buildtime, MincStmt* stmt) {}
	virtual void buildResumeStmt(MincBuildtime& buildtime, MincStmt* stmt) {}
	virtual void buildExpr(MincBuildtime& buildtime, MincExpr* expr);
	virtual void buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next);
	virtual void buildSuspendExpr(MincBuildtime& buildtime, MincExpr* expr) {}
	virtual void buildResumeExpr(MincBuildtime& buildtime, MincExpr* expr) {}
	virtual int run(MincExpr* expr, MincInteropData& interopData) = 0;
};

extern struct MincInterpreter : public MincRunner
{
	MincInterpreter();
	void buildStmt(MincBuildtime& buildtime, MincStmt* stmt);
	void buildSuspendStmt(MincBuildtime& buildtime, MincStmt* stmt);
	void buildExpr(MincBuildtime& buildtime, MincExpr* expr);
	void buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next);
	void buildSuspendExpr(MincBuildtime& buildtime, MincExpr* expr);
	int run(MincExpr* expr, MincInteropData& interopData);
} MINC_INTERPRETER;

struct MincKernel
{
	MincRunner& runner;

	MincKernel(MincRunner* runner=&MINC_INTERPRETER) : runner(*runner) {}
	virtual ~MincKernel();
	virtual MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params);
	virtual void dispose(MincKernel* kernel);
	virtual bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
	virtual MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
};
extern struct UnresolvableExprKernel : public MincKernel
{
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
} UNRESOLVABLE_EXPR_KERNEL, UNUSED_KERNEL;
extern struct UnresolvableStmtKernel : public MincKernel
{
	bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params);
	MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const;
} UNRESOLVABLE_STMT_KERNEL;

struct MincEnteredBlockExpr
{
private:
	MincRuntime& runtime;
	const MincBlockExpr* block;
	MincStackFrame* const prevStackFrame;

public:
	MincEnteredBlockExpr(MincRuntime& runtime, const MincBlockExpr* block);
	~MincEnteredBlockExpr();
	bool run();
	void exit();
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

typedef void (*BuildBlock)(MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs);
typedef bool (*RunBlock)(MincRuntime& runtime, const std::vector<MincExpr*>& params, void* exprArgs);
typedef MincObject* (*ExprTypeBlock)(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs);
typedef void (*ImptBlock)(MincSymbol& symbol, MincScopeType* fromScope, MincScopeType* toScope);
typedef void (*StepEvent)(const MincExpr* loc, StepEventType type, void* eventArgs);

#endif