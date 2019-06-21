#ifndef __CODEGEN_H
#define __CODEGEN_H

class BlockExprAST;

class IModule
{
public:
	virtual void print(const std::string& outputPath) = 0;
	virtual void print() = 0;
	virtual bool compile(const std::string& outputPath, std::string& errstr) = 0;
	virtual void run() = 0;
};

void init();
IModule* createModule(const std::string& sourcePath, BlockExprAST* moduleBlock, bool outputDebugSymbols, BlockExprAST* parentBlock = nullptr);

#endif