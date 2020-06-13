#ifndef __MINC_TYPES_H
#define __MINC_TYPES_H

#include <iostream>
#include <string>
#include <vector>

struct BaseType {};
struct BaseScopeType {};
struct BaseValue
{
	virtual uint64_t getConstantValue() { return 0; }
};

class ExprAST;
class ListExprAST;
class IdExprAST;
class CastExprAST;
class LiteralExprAST;
class PlchldExprAST;
class ListExprAST;
class StmtAST;
class BlockExprAST;

typedef int MatchScore;

enum StepEventType { STEP_IN, STEP_OUT, STEP_SUSPEND, STEP_RESUME };

struct Variable
{
	BaseType* type;
	BaseValue* value;
	Variable() = default;
	Variable(const Variable& v) = default;
	Variable(BaseType* type, BaseValue* value) : type(type), value(value) {}
};

struct CodegenContext
{
	virtual ~CodegenContext() {}
	virtual Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params) = 0;
	virtual BaseType* getType(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) const = 0;
};

struct Cast
{
	BaseType* const fromType;
	BaseType* const toType;
	CodegenContext* const context;
	Cast() = default;
	Cast(const Cast&) = default;
	Cast(BaseType* fromType, BaseType* toType, CodegenContext* context)
		: fromType(fromType), toType(toType), context(context) {}
	virtual int getCost() const = 0;
	virtual Cast* derive() const = 0;
};

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
	CompileError(std::string msg, Location loc={0}) : loc(loc), msg(msg) {}
	void addHint(const std::string& hint) { hints.push_back(hint); }
	void print(std::ostream& out=std::cerr);
};

typedef void (*StmtBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs);
typedef Variable (*ExprBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs);
typedef BaseType* (*ExprTypeBlock)(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs);
typedef void (*ImptBlock)(Variable& symbol, BaseScopeType* fromScope, BaseScopeType* toScope);
typedef void (*StepEvent)(const ExprAST* loc, StepEventType type, void* eventArgs);

#endif