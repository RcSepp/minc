#include <string>
#include <vector>

struct BaseType;
struct XXXValue;
class JitFunction;

class ExprAST;
class IdExprAST;
class LiteralExprAST;
class PlchldExprAST;
class ExprListAST;
class StmtAST;
class BlockExprAST;

struct Variable
{
	BaseType* type;
	XXXValue* value;
	Variable() = default;
	Variable(const Variable& v) = default;
	Variable(BaseType* type, XXXValue* value) : type(type), value(value) {}
};

typedef void (*StmtBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params);
typedef Variable (*ExprBlock)(BlockExprAST* parentBlock, std::vector<ExprAST*>& params);
typedef BaseType* (*ExprTypeBlock)(const BlockExprAST*, const std::vector<ExprAST*>&);

extern "C"
{
	Variable codegenExpr(ExprAST* expr, BlockExprAST* scope);
	void codegenStmt(StmtAST* stmt, BlockExprAST* scope);
	BaseType* getType(ExprAST* expr, const BlockExprAST* scope);
	void collectParams(const BlockExprAST* scope, const ExprAST* tplt, ExprAST* expr, std::vector<ExprAST*>& params);
	std::string ExprASTToString(const ExprAST* expr);
	std::string StmtASTToString(const StmtAST* stmt);
	bool ExprASTIsId(const ExprAST* expr);
	bool ExprASTIsParam(const ExprAST* expr);
	bool ExprASTIsBlock(const ExprAST* expr);
	const char* getIdExprASTName(const IdExprAST* expr);
	const char* getLiteralExprASTValue(const LiteralExprAST* expr);
	BlockExprAST* getBlockExprASTParent(const BlockExprAST* expr);
	void setBlockExprASTParent(BlockExprAST* expr, BlockExprAST* parent);
	unsigned getExprLine(const ExprAST* expr);
	unsigned getExprColumn(const ExprAST* expr);

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, XXXValue* value);
	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, JitFunction* func, void* closure = nullptr);
	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock);
	void defineExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type);
	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type);
	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock);
	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func);
	void defineCast2(BlockExprAST* scope, BaseType* fromType, BaseType* toType, ExprBlock codeBlock);

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured);
	StmtAST* lookupStmt(const BlockExprAST* scope, const std::vector<ExprAST*>& exprs);
	ExprAST* lookupCast(const BlockExprAST* scope, ExprAST* expr, BaseType* toType);
	std::string reportExprCandidates(const BlockExprAST* scope, const ExprAST* expr);

	BaseType* getBaseType();

	void raiseCompileError(const char* msg, const ExprAST* loc);

	void importModule(BlockExprAST* scope, const char* path, const ExprAST* loc);

	BaseType* createFuncType(const char* name, bool isVarArg, BaseType* resultType, BaseType** argTypes, int numArgTypes);

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params, std::string& name);
	void removeJitFunctionModule(JitFunction* jitFunc);
	void removeJitFunction(JitFunction* jitFunc);
}