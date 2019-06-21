// lc -relocation-model=pic -o stmtdef.o -filetype=obj stmtdef.ll && g++ -o stmtdef stmtdef.o `llvm-config --cxxflags --ldflags --system-libs --libs all` -fexceptions

#include "../src/ast.h"
#include "llvm-c/Core.h"

extern "C" {
    void jitFunction1(LLVMBuilderRef builder, ExprAST** params);
}

int main()
{
    LLVMBuilderRef builder = LLVMCreateBuilder();
    ExprAST** params = new ExprAST*[16];
    for (int i = 0; i < 16; ++i)
        params[i] = new LiteralExprAST({0}, "TEST");
    jitFunction1(builder, params);
}