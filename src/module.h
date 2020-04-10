#ifndef __MODULE_H
#define __MODULE_H

#include <vector>
#include <string>

struct BaseType;
class ExprAST;
class BlockExprAST;
class JitFunction;

class IModule
{
public:
	virtual void print(const std::string& outputPath) = 0;
	virtual void print() = 0;
	virtual bool compile(const std::string& outputPath, std::string& errstr) = 0;
	virtual int run() = 0;
	virtual void buildRun() = 0;
	virtual void finalize() = 0;
};

extern "C"
{
	void initCompiler();
	void loadLibrary(const char* filename);
	IModule* createModule(const std::string& sourcePath, const std::string& moduleFuncName, bool outputDebugSymbols);

	void defineJitFunctionInheritanceCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func);
	void defineJitFunctionStmt(BlockExprAST* scope, const std::vector<ExprAST*>& tplt, JitFunction* func, void* stmtArgs = nullptr);
	void defineJitFunctionAntiStmt(BlockExprAST* scope, JitFunction* func, void* stmtArgs = nullptr);
	void defineJitFunctionExpr(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, BaseType* type);
	void defineJitFunctionExpr2(BlockExprAST* scope, ExprAST* tplt, JitFunction* func, JitFunction* typeFunc);
	void defineJitFunctionAntiExpr(BlockExprAST* scope, JitFunction* func, BaseType* type);
	void defineJitFunctionAntiExpr2(BlockExprAST* scope, JitFunction* func, JitFunction* typeFunc);
	void defineJitFunctionTypeCast(BlockExprAST* scope, BaseType* fromType, BaseType* toType, JitFunction* func);

	JitFunction* createJitFunction(BlockExprAST* scope, BlockExprAST* blockAST, BaseType *returnType, std::vector<ExprAST*>& params, std::string& name);
	uint64_t compileJitFunction(JitFunction* jitFunc);
	void removeJitFunctionModule(JitFunction* jitFunc);
	void removeJitFunction(JitFunction* jitFunc);
}

#endif