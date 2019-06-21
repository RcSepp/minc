#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
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
    std::unique_ptr<Module> TheModule = llvm::make_unique<Module>("include.c", TheContext);

    TheModule->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);
    std::unique_ptr<DIBuilder> DBuilder = llvm::make_unique<DIBuilder>(*TheModule);
    DICompileUnit* TheCU = DBuilder->createCompileUnit(dwarf::DW_LANG_C, DBuilder->createFile("include.c", "."), "MyCompiler", 0, "", 0);

    DIType* IntTy = DBuilder->createBasicType("int", 32, dwarf::DW_ATE_signed);
    DIType* DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);

    // >>> printf

    std::vector<Type *> PrintFArgTypes = { Type::getInt8PtrTy(TheContext) };
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(TheContext), PrintFArgTypes, true);
    Function *printf = Function::Create(FT, Function::ExternalLinkage, "printf", TheModule.get());
    printf->setDSOLocal(true);
    verifyFunction(*printf);

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

        DIFile *Unit = DBuilder->createFile(TheCU->getFilename(), TheCU->getDirectory());
        DIScope *FContext = Unit;
        unsigned LineNo = 8;
        unsigned ScopeLine = 9;
        DISubprogram *SP = DBuilder->createFunction(
            FContext, "main", StringRef(), Unit, LineNo,
            DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(SmallVector<Metadata*, 8>({ IntTy }))),
            ScopeLine, DINode::FlagPrototyped, DISubprogram::SPFlagDefinition);
        TheFunction->setSubprogram(SP);
    }

    BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    // Constant *hello_world = ConstantDataArray::getString(TheContext, "hello world", true);
    // Constant* int_0 = ConstantInt::get(TheContext, APInt(64, StringRef("0"), 10));
    // std::vector<Constant*> int_0v;
    // int_0v.push_back(int_0);
    // int_0v.push_back(int_0);
    // Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(Type::getInt8Ty(TheContext), hello_world, int_0v);
    // GetElementPtrConstantExpr
    //Value *hello_world = Builder.CreateGlobalStringPtr("hello world!\n");
Constant *StrConstant = ConstantDataArray::getString(TheContext, "Hello World!\n");
GlobalVariable *hello_world = new GlobalVariable(*TheModule, StrConstant->getType(), true,
                            GlobalValue::PrivateLinkage, StrConstant, "",
                            nullptr, GlobalVariable::NotThreadLocal,
                            0);
hello_world->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
hello_world->setAlignment(1);
    Constant *zero_32 = Constant::getNullValue(IntegerType::getInt32Ty(TheContext));
    std::vector<Value *>gep_params = {
        zero_32,
        zero_32
    };
    Value* const_ptr_5 = Builder.CreateGEP(StrConstant->getType(), hello_world, gep_params);
    Builder.CreateCall(printf, std::vector<Value*> { const_ptr_5 }, "calltmp");

    Constant* c = ConstantInt::get(TheContext, APInt(32, 0));
    Builder.CreateRet(c);

    verifyFunction(*TheFunction);


    DBuilder->finalize();

    TheModule->print(errs(), nullptr);

    return 0;
}
