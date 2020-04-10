#ifndef __MINC_API_H
#define __MINC_API_H

#include <string>
#include <vector>
#include <functional>

struct BaseType {};
struct BaseScopeType {};
struct BaseValue
{
	virtual uint64_t getConstantValue() = 0;
};
class JitFunction;

class ExprAST;
class IdExprAST;
class CastExprAST;
class LiteralExprAST;
class PlchldExprAST;
class ExprListAST;
class StmtAST;
class BlockExprAST;

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

typedef void (*StmtBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs);
typedef Variable (*ExprBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs);
typedef BaseType* (*ExprTypeBlock)(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs);
typedef void (*ImptBlock)(Variable& symbol, BaseScopeType* fromScope, BaseScopeType* toScope);
typedef void (*StepEvent)(const ExprAST* loc, StepEventType type, void* eventArgs);

extern "C"
{
	// >>> Parser

	BlockExprAST* parseCFile(const char* filename);
	BlockExprAST* parsePythonFile(const char* filename);

	// >>> Code Generator

	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope);
	void codegenStmt(StmtAST* stmt, BlockExprAST* scope);
	BaseType* getType(ExprAST* expr, const BlockExprAST* scope);
	void importBlock(BlockExprAST* scope, BlockExprAST* block);
	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params);
	std::string ExprASTToString(const ExprAST* expr);
	std::string ExprASTToShortString(const ExprAST* expr);
	bool ExprASTIsId(const ExprAST* expr);
	bool ExprASTIsCast(const ExprAST* expr);
	bool ExprASTIsParam(const ExprAST* expr);
	bool ExprASTIsBlock(const ExprAST* expr);
	bool ExprASTIsStmt(const ExprAST* expr);
	bool ExprASTIsList(const ExprAST* expr);
	bool ExprASTIsPlchld(const ExprAST* expr);
	bool ExprASTIsEllipsis(const ExprAST* expr);
	void resolveExprAST(BlockExprAST* scope, ExprAST* expr);
	BlockExprAST* wrapExprAST(ExprAST* expr);
	BlockExprAST* createEmptyBlockExprAST();
	BlockExprAST* cloneBlockExprAST(BlockExprAST* block);
	void resetBlockExprAST(BlockExprAST* block);
	size_t getBlockExprASTCacheState(BlockExprAST* block);
	void resetBlockExprASTCache(BlockExprAST* block, size_t targetState);
	void removeBlockExprAST(BlockExprAST* expr);
	std::vector<ExprAST*>& getExprListASTExpressions(ExprListAST* expr);
	const char* getIdExprASTName(const IdExprAST* expr);
	const char* getLiteralExprASTValue(const LiteralExprAST* expr);
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr);
	const std::vector<BlockExprAST*>& getBlockExprASTReferences(const BlockExprAST* expr);
	size_t countBlockExprASTStmts(const BlockExprAST* expr);
	void iterateBlockExprASTStmts(const BlockExprAST* expr, std::function<void(const ExprListAST* tplt, const CodegenContext* stmt)> cbk);
	size_t countBlockExprASTExprs(const BlockExprAST* expr);
	void iterateBlockExprASTExprs(const BlockExprAST* expr, std::function<void(const ExprAST* tplt, const CodegenContext* expr)> cbk);
	size_t countBlockExprASTCasts(const BlockExprAST* expr);
	void iterateBlockExprASTCasts(const BlockExprAST* expr, std::function<void(const Cast* cast)> cbk);
	size_t countBlockExprASTSymbols(const BlockExprAST* expr);
	void iterateBlockExprASTSymbols(const BlockExprAST* expr, std::function<void(const std::string& name, const Variable& symbol)> cbk);
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent);
	void setBlockExprASTParams(BlockExprAST* expr, std::vector<Variable>& blockParams);
	const std::string& getBlockExprASTName(const BlockExprAST* expr);
	void setBlockExprASTName(BlockExprAST* expr, std::string name);
	ExprAST* getCastExprASTSource(const CastExprAST* expr);
	char getPlchldExprASTLabel(const PlchldExprAST* expr);
	const char* getPlchldExprASTSublabel(const PlchldExprAST* expr);
	const char* getExprFilename(const ExprAST* expr);
	unsigned getExprLine(const ExprAST* expr);
	unsigned getExprColumn(const ExprAST* expr);
	unsigned getExprEndLine(const ExprAST* expr);
	unsigned getExprEndColumn(const ExprAST* expr);
	const ExprAST* createLoc(const char* filename=nullptr, unsigned line=1, unsigned column=1, unsigned endLine=1, unsigned endColumn=1);
	ExprAST* getDerivedExprAST(ExprAST* expr);
	BlockExprAST* getRootScope();
	BlockExprAST* getFileScope();
	BaseScopeType* getScopeType(const BlockExprAST* scope);
	void setScopeType(BlockExprAST* scope, BaseScopeType* scopeType);
	void defineImportRule(BaseScopeType* fromScope, BaseScopeType* toScope, BaseType* symbolType, ImptBlock imptBlock);
	const std::string& getTypeName(const BaseType* type);

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, BaseValue* value);
	void defineType(const char* name, const BaseType* type);
	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineStmt3(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, CodegenContext* stmt);
	void defineAntiStmt2(BlockExprAST* scope, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineAntiStmt3(BlockExprAST* scope, CodegenContext* stmt);
	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type, void* exprArgs = nullptr);
	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs = nullptr);
	void defineExpr5(BlockExprAST* scope, ExprAST* tplt, CodegenContext* expr);
	void defineAntiExpr2(BlockExprAST* scope, ExprBlock codeBlock, BaseType* type, void* exprArgs = nullptr);
	void defineAntiExpr3(BlockExprAST* scope, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs = nullptr);
	void defineAntiExpr5(BlockExprAST* scope, CodegenContext* expr);
	void defineTypeCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock, void* castArgs = nullptr);
	void defineTypeCast3(BlockExprAST* scope, BaseType* fromType, BaseType* toType, CodegenContext* cast);
	void defineOpaqueTypeCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType);
	void defineInheritanceCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock, void* castArgs = nullptr);
	void defineInheritanceCast3(BlockExprAST* scope, BaseType* fromType, BaseType* toType, CodegenContext* cast);
	void defineOpaqueInheritanceCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType);

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name);
	Variable* importSymbol(BlockExprAST* scope, const char* name);
	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType);
	bool isInstance(const BlockExprAST* scope, BaseType* fromType, BaseType* toType);
	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr);
	std::string reportCasts(const BlockExprAST* scope);

	const Variable& getVoid();

	void raiseCompileError(const char* msg, const ExprAST* loc=nullptr);

	void registerStepEventListener(StepEvent listener, void* eventArgs=nullptr);
	void deregisterStepEventListener(StepEvent listener);
}

#endif