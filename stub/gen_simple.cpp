#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
//#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

int main()
{
	LLVMContext TheContext;
	IRBuilder<> Builder(TheContext);
	std::unique_ptr<Module> TheModule;

	TheModule = llvm::make_unique<Module>("simple.c", TheContext);

	// >>> main

	Function *TheFunction = TheModule->getFunction("main"); // Check for existing "main" function

	if (!TheFunction)
	{
		std::vector<std::string> Args = { };

		// Make the function type:  double(double,double) etc.
		std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
		FunctionType *FT = FunctionType::get(Type::getInt32Ty(TheContext), Doubles, false);

		TheFunction = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
		TheFunction->setDSOLocal(true);
		TheFunction->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::NoInline);
		TheFunction->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::OptimizeNone);

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto &Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);
	}

	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);


	AllocaInst* var = Builder.CreateAlloca(Type::getInt32Ty(TheContext), nullptr);
	var->setAlignment(4);
	Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(TheContext), APInt(32, 4, true)), var);


	Builder.CreateRet(ConstantInt::get(TheContext, APInt(32, 0)));

	bool haserr = verifyFunction(*TheFunction, &errs());



	TheModule->print(outs(), nullptr);

	return 0;
}
