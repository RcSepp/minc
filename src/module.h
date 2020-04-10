#ifndef __MODULE_H
#define __MODULE_H

#include <set>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>

class XXXModule : public IModule
{
protected:
	Module* const prevModule;
	XXXModule* const prevXXXModule;
	DIBuilder* const prevDbuilder;
	DIFile* const prevDfile;
	Function* const prevFunc;
	BasicBlock* const prevBB;
	const ExprAST* const loc;

	legacy::FunctionPassManager* jitFunctionPassManager;
	legacy::PassManager* jitModulePassManager;

public:
	std::unique_ptr<Module> module;
	std::set<XXXModule*> dependencies;

	XXXModule(const std::string& moduleName, const ExprAST* loc, bool outputDebugSymbols, bool optimizeCode);
	virtual void finalize();
	void print(const std::string& outputPath);
	void print();
	bool compile(const std::string& outputPath, std::string& errstr);
	int run();
	void buildRun();
};

class FileModule : public XXXModule
{
private:
	const std::string prevSourcePath;
	Function* mainFunc;

public:
	FileModule(const char* sourcePath, const std::string& moduleFuncName, bool outputDebugSymbols, bool optimizeCode);
	void finalize();
	int run();
	void buildRun();
};

class JitFunction : public XXXModule
{
private:
	llvm::orc::VModuleKey jitModuleKey;

public:
	const std::string name;
	StructType* closureType;
	FunctionType* funcType;

	static void init();
	JitFunction(BlockExprAST* parentBlock, BlockExprAST* blockAST, Type *returnType, std::vector<ExprAST*>& params, std::string& name);
	void finalize();
	uint64_t compile();
	void removeCompiledModule();
};

#endif