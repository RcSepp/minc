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
	unsigned getExprLine(const ExprAST* expr);
	unsigned getExprColumn(const ExprAST* expr);

	void defineSymbol(BlockExprAST* scope, const char* name, BaseType* type, XXXValue* value);
	void defineStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, uint64_t funcAddr, void* closure = nullptr);
	void defineStmt2(BlockExprAST* scope, const char* tpltStr, StmtBlock codeBlock);
	void defineExpr(BlockExprAST* scope, ExprAST* tplt, uint64_t funcAddr, BaseType* type);
	void defineExpr2(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, BaseType* type);
	void defineExpr3(BlockExprAST* scope, const char* tpltStr, ExprBlock codeBlock, ExprTypeBlock typeBlock);
	void defineCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, uint64_t funcAddr);

	const Variable* lookupSymbol(const BlockExprAST* scope, const char* name, bool& isCaptured);
	StmtAST* lookupStmt(const BlockExprAST* scope, const std::vector<ExprAST*>& exprs);

	BaseType* getBaseType();

	void raiseCompileError(const char* msg, const ExprAST* loc);

	void importModule(BlockExprAST* scope, const char* path, const ExprAST* loc);

	BaseType* createFuncType(const char* name, bool isVarArg, BaseType* resultType, BaseType** argTypes, int numArgTypes);

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params);
	uint64_t compileJitFunction(JitFunction* jitFunc, const char* outputPath);
	void removeJitFunctionModule(JitFunction* jitFunc);
	void removeJitFunction(JitFunction* jitFunc);
}