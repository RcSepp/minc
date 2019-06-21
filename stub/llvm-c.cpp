#include "llvm-c/Core.h"

void foo(LLVMBuilderRef builder)
{
	LLVMValueRef result = LLVMConstInt(LLVMInt32Type(), 0, false);
	LLVMBuildRet(builder, result);
}

int main()
{
	//LLVMModuleRef M = LLVMModuleCreateWithName("foo.c");
	LLVMBuilderRef builder = LLVMCreateBuilder();
	foo(builder);
	return 0;
}