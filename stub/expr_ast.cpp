#include "../src/ast.h"
#include <stdio.h>
#include <llvm-c/Core.h>

void func(LLVMBuilderRef builder, ExprAST** literalAST)
{
	//int foo = literalAST->loc.begin_line;
	LiteralExprAST* bar = (LiteralExprAST*)literalAST[0];
	const char* foo = bar->value;
	puts(foo);
}

int main()
{
ExprAST** foo = new ExprAST*[1];
foo[0] = new LiteralExprAST({0}, "123");
	func(nullptr, foo);
	//func(new Location({4, 5, 6, 7}));
	return 0;
}

/*#include <llvm-c/Core.h>
int main()
{
	LLVMInt32Type();
	//LLVMValueRef loc = LLVMConstInt(LLVMInt32Type(), 0, false);
	//LLVMValueRef indices[] = { LLVMConstInt(LLVMInt32Type(), 0, false) };
	//LLVMConstInBoundsGEP(loc, indices, 1);
	return 0;
}*/


// #include <llvm/IR/Value.h>
// #include <llvm/IR/Function.h>
// #include <llvm/IR/DerivedTypes.h>
// using namespace llvm;
// struct XXXValue
// {
// 	llvm::Type* type;
// 	union {
// 		Value* val;
// 		Function* func;
// 	};
// 	XXXValue(Value* val) : type(val->getType()), val(val) {}
// 	XXXValue(Function* func) : type(func->getFunctionType()), func(func) {}
// };
// extern "C" {
// 	XXXValue* CreateVariableFromValue(LLVMValueRef val)
// 	{
// 		return new XXXValue(unwrap(val));
// 	}
// 	Value* CodeGenValue(ExprAST* expr, BlockExprAST* block)
// 	{
// 		return expr->codegen(block).value->val;
// 	}
// }
// int main()
// {
// 	/*LLVMContextRef context = LLVMContextCreate();
// 	LLVMBuilderRef builder = LLVMCreateBuilder();
// 	LLVMModuleRef module = LLVMModuleCreateWithName("foo");

// 	LLVMTypeRef mainType = LLVMFunctionType(LLVMInt32Type(), nullptr, 0, false);
// 	LLVMValueRef main = LLVMAddFunction(module, "main", mainType);
// 	LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main, "entry");
// 	LLVMPositionBuilder(builder, entry, nullptr);

// 	LLVMTypeRef valueType = LLVMStructCreateNamed(context, "struct.LLVMOpaqueValue");
// 	LLVMTypeRef _valueType = LLVMGetTypeByName(module, "struct.LLVMOpaqueValue");
// 	LLVMTypeRef valuePtrType = LLVMPointerType(valueType, 0);
// 	LLVMValueRef cvalue = LLVMBuildAlloca(builder, valuePtrType, "");
// 	llvm::Value* value = llvm::unwrap(cvalue);
// 	new Variable(value);*/

// 	LiteralExprAST* foo = new LiteralExprAST({0}, "123");
// 	//llvm::Value* val = ((XXXValue*)foo->codegen(nullptr).value)->val;
// 	llvm::Value* val = CodeGenValue(foo, nullptr);
// 	//new XXXValue(((XXXValue*)foo->codegen(nullptr).value)->val);
// 	//puts(foo->value);
// 	//wrap(val);
// 	//LLVMValueRef* wrapped = reinterpret_cast<LLVMValueRef*>(val);
// 	BlockExprAST block({0}, {});
// 	block.addToScope("foo", nullptr, new XXXValue(val));
// 	//block.addToScope("foo", nullptr, CreateVariableFromValue(val));

// 	/*LLVMBuildRet(builder, LLVMConstNull(LLVMInt32Type()));
// 	LLVMDumpModule(module);*/

// 	return 0;
// }