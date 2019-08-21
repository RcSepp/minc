#include <string>
#include <vector>

struct BaseType;
struct BaseValue;
class JitFunction;

class ExprAST;
class IdExprAST;
class CastExprAST;
class LiteralExprAST;
class PlchldExprAST;
class ExprListAST;
class StmtAST;
class BlockExprAST;

struct Variable
{
	BaseType* type;
	BaseValue* value;
	Variable() = default;
	Variable(const Variable& v) = default;
	Variable(BaseType* type, BaseValue* value) : type(type), value(value) {}
};

typedef void (*StmtBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs);
typedef Variable (*ExprBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs);
typedef BaseType* (*ExprTypeBlock)(const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs);

extern "C"
{
	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope);
	void codegenStmt(StmtAST* stmt, BlockExprAST* scope);
	BaseType* getType(ExprAST* expr, const BlockExprAST* scope);
	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params);
	std::string ExprASTToString(const ExprAST* expr);
	std::string StmtASTToString(const StmtAST* stmt);
	bool ExprASTIsId(const ExprAST* expr);
	bool ExprASTIsCast(const ExprAST* expr);
	bool ExprASTIsParam(const ExprAST* expr);
	bool ExprASTIsBlock(const ExprAST* expr);
	void resolveExprAST(BlockExprAST* scope, ExprAST* expr);
	BlockExprAST* wrapExprAST(ExprAST* expr);
	std::vector<ExprAST*>& getExprListASTExpressions(ExprListAST* expr);
	const char* getIdExprASTName(const IdExprAST* expr);
	const char* getLiteralExprASTValue(const LiteralExprAST* expr);
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr);
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent);
	ExprAST* getCastExprASTSource(const CastExprAST* expr);
	const char* getExprFilename(const ExprAST* expr);
	unsigned getExprLine(const ExprAST* expr);
	unsigned getExprColumn(const ExprAST* expr);
	unsigned getExprEndLine(const ExprAST* expr);
	unsigned getExprEndColumn(const ExprAST* expr);
	BlockExprAST* getRootScope();
	const std::string& getTypeName(const BaseType* type);

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, BaseValue* value);
	void defineType(const char* name, BaseType* type);
	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, JitFunction* func, void* stmtArgs = nullptr);
	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock, void* stmtArgs = nullptr);
	void defineExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type);
	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type, void* exprArgs = nullptr);
	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock, void* exprArgs = nullptr);
	void defineExpr4(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, JitFunction* typeFunc);
	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func);
	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock, void* castArgs = nullptr);
	void defineOpaqueCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType);

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured);
	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType);
	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr);
	std::string reportCasts(const BlockExprAST* scope);

	BaseType* getBaseType();
	BaseType* getVoidType();

	void raiseCompileError(const char* msg, const ExprAST* loc);

	void importModule(BlockExprAST* scope, const char* path, const ExprAST* loc);

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params, std::string& name);
	uint64_t compileJitFunction(JitFunction* jitFunc);
	void removeJitFunctionModule(JitFunction* jitFunc);
	void removeJitFunction(JitFunction* jitFunc);
}