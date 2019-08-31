#ifndef __MODULE_H
#define __MODULE_H

#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>

class XXXModule : public IModule
{
protected:
	Module* const prevModule;
	DIBuilder* const prevDbuilder;
	DIFile* const prevDfile;
	Function* const prevFunc;
	BasicBlock* const prevBB;
	const Location loc;

	std::unique_ptr<Module> module;
	legacy::FunctionPassManager* jitFunctionPassManager;
	legacy::PassManager* jitModulePassManager;

public:
	XXXModule(const std::string& moduleName, const Location& loc, bool outputDebugSymbols, bool optimizeCode);
	virtual void finalize();
	void print(const std::string& outputPath);
	void print();
	bool compile(const std::string& outputPath, std::string& errstr);
	void run();
};

class FileModule : public XXXModule
{
private:
	const std::string prevSourcePath;
	Function* mainFunc;

public:
	FileModule(const char* sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, bool optimizeCode);
	void finalize();
	void run();
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