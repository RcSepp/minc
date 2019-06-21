// opt -enable-coroutines -O0 -S gen_coro.ll | llc -o gen_coro.o -filetype=obj
// gcc -o gen_coro gen_coro.o
// ./gen_coro

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
#include "CoroInstr.h"
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

    TheModule = llvm::make_unique<Module>("include.c", TheContext);

    // >>> declare coro.*

    Function *coro_id = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_id);
    Function *coro_alloc = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_alloc);
    Function *coro_free = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_free);
    Function *coro_size = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_size, { Type::getInt32Ty(TheContext) });
    Function *coro_suspend = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_suspend);
    Function *coro_resume = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_resume);
    Function *coro_destroy = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_destroy);
    Function *coro_begin = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_begin);
    Function *coro_end = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::coro_end);

    // >>> declare malloc

    FunctionType *mallocType = FunctionType::get(Type::getInt8PtrTy(TheContext), { Type::getInt32Ty(TheContext) }, false);
    Function *malloc = Function::Create(mallocType, Function::ExternalLinkage, "malloc", TheModule.get());
    verifyFunction(*malloc);

    // >>> declare malloc

    FunctionType *freeType = FunctionType::get(Type::getVoidTy(TheContext), { Type::getInt8PtrTy(TheContext) }, false);
    Function *free = Function::Create(freeType, Function::ExternalLinkage, "free", TheModule.get());
    verifyFunction(*free);

    // >>> declare printf

    std::vector<Type *> PrintFArgTypes = { Type::getInt8PtrTy(TheContext) };
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(TheContext), PrintFArgTypes, true);
    Function *printf = Function::Create(FT, Function::ExternalLinkage, "printf", TheModule.get());
    printf->setDSOLocal(true);
    verifyFunction(*printf);

    // >>> f

    FunctionType* fType = FunctionType::get(Type::getInt8PtrTy(TheContext), { Type::getInt32Ty(TheContext) }, false);
    Function* f = Function::Create(fType, Function::ExternalLinkage, "f", TheModule.get());
    Value* n = f->args().begin();
    n->setName("n");
    f->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::NoInline);
    f->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::OptimizeNone);

    BasicBlock *fEntry = BasicBlock::Create(TheContext, "entry", f);
    BasicBlock *fLoop = BasicBlock::Create(TheContext, "loop", f);
    BasicBlock *fResume = BasicBlock::Create(TheContext, "resume", f);
    BasicBlock *fSuspend = BasicBlock::Create(TheContext, "suspend", f);
    BasicBlock *fCleanup = BasicBlock::Create(TheContext, "cleanup", f);

    // entry block
    Builder.SetInsertPoint(fEntry);
    Value* id = Builder.CreateCall(coro_id, {
        ConstantInt::get(TheContext, APInt(32, 0)),
        Constant::getNullValue(IntegerType::getInt8PtrTy(TheContext)),
        Constant::getNullValue(IntegerType::getInt8PtrTy(TheContext)),
        Constant::getNullValue(IntegerType::getInt8PtrTy(TheContext))
    }, "id");
    Value* size = Builder.CreateCall(coro_size, {}, "size");
    //Builder.CreateAlloca(IntegerType::getInt8PtrTy(TheContext), size, "alloc");
    //CallInst::CreateMalloc(fEntry, IntegerType::getInt32Ty(TheContext), IntegerType::getInt8Ty(TheContext), size, nullptr, nullptr, "alloc");
    Value* alloc = Builder.CreateCall(malloc, size, "alloc");
    Value* hdl = Builder.CreateCall(coro_begin, {
        id,
        alloc
    }, "hdl");
    Value* np = Builder.CreateAlloca(IntegerType::getInt32Ty(TheContext), nullptr, "np");
    Builder.CreateStore(n, np);
    Builder.CreateBr(fLoop);

    // loop block
    Builder.SetInsertPoint(fLoop);
    Value* nt = Builder.CreateLoad(np);

Constant *StrConstant = ConstantDataArray::getString(TheContext, "%i\n");
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
Builder.CreateCall(printf, std::vector<Value*> { const_ptr_5, nt }, "calltmp");

    Value* suspend_result = Builder.CreateCall(coro_suspend, {
        Constant::getNullValue(Type::getTokenTy(TheContext)),
        ConstantInt::get(TheContext, APInt(1, 0))
    });
    SwitchInst* swch = Builder.CreateSwitch(suspend_result, fSuspend, 2);
    swch->addCase(ConstantInt::get(TheContext, APInt(8, 0)), fResume);
    swch->addCase(ConstantInt::get(TheContext, APInt(8, 1)), fCleanup);

    // resume block
    Builder.SetInsertPoint(fResume);
    nt = Builder.CreateAdd(nt, ConstantInt::get(TheContext, APInt(32, 1)));
    Builder.CreateStore(nt, np);
    Builder.CreateBr(fLoop);

    // cleanup block
    Builder.SetInsertPoint(fCleanup);
    Value* mem = Builder.CreateCall(coro_free, {
        id,
        hdl
    }, "mem");
    Builder.CreateCall(free, mem);
    Builder.CreateBr(fSuspend);

    // suspend block
    Builder.SetInsertPoint(fSuspend);
    Builder.CreateCall(coro_end, {
        hdl,
        ConstantInt::get(TheContext, APInt(1, 0))
    });
    Builder.CreateRet(hdl);

    verifyFunction(*f);


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

    hdl = Builder.CreateCall(f, {
        ConstantInt::get(TheContext, APInt(32, 4))
    }, "hdl");
    Builder.CreateCall(coro_resume, { hdl });
    Builder.CreateCall(coro_resume, { hdl });
    Builder.CreateCall(coro_destroy, { hdl });

    // Constant *hello_world = ConstantDataArray::getString(TheContext, "hello world", true);
    // Constant* int_0 = ConstantInt::get(TheContext, APInt(64, StringRef("0"), 10));
    // std::vector<Constant*> int_0v;
    // int_0v.push_back(int_0);
    // int_0v.push_back(int_0);
    // Constant* const_ptr_5 = ConstantExpr::getGetElementPtr(Type::getInt8Ty(TheContext), hello_world, int_0v);
    // GetElementPtrConstantExpr
    //Value *hello_world = Builder.CreateGlobalStringPtr("hello world!\n");
/*Constant *StrConstant = ConstantDataArray::getString(TheContext, "Hello World!\n");
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
    Builder.CreateCall(printf, std::vector<Value*> { const_ptr_5 }, "calltmp");*/

    Constant* c = ConstantInt::get(TheContext, APInt(32, 0));
    Builder.CreateRet(c);

    verifyFunction(*TheFunction);



    //TheModule->print(errs(), nullptr);
    std::error_code err;
    raw_fd_ostream fileStream("gen_coro.ll", err);
    TheModule->print(fileStream, nullptr);

    return 0;
}
