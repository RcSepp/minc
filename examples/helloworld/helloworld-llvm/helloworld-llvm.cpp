#include <string>
#include <cstring>
#include <iostream>
#include "minc_api.hpp"
#include "minc_pkgmgr.h"

// LLVM IR creation
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

// LLVM compilation
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

// LLVM optimization
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>

#include "minc_llvm.h"

using namespace llvm;

MincObject STRING_TYPE, META_TYPE;

struct Object : public MincObject
{
	Value* value;
	Object(Value* value) : value(value) {}
};

struct String : public std::string, public MincObject
{
	String(const std::string val) : std::string(val) {}
};

MincPackage HELLOWORLD_LLVM_PKG("helloworld-llvm", [](MincBlockExpr* pkgScope) {
	pkgScope->defineSymbol("string", &META_TYPE, &STRING_TYPE);

	struct LiteralKernel : MincKernel
	{
		LiteralKernel() : MincKernel(&LLVM_RUNNER) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& value = ((MincLiteralExpr*)params[0])->value;

			if (value.back() == '"' || value.back() == '\'')
			{
				GlobalVariable* glob = new GlobalVariable(*LLVM_RUNNER.module, ArrayType::get(Type::getInt8Ty(context), value.size() - 1), false, GlobalValue::ExternalLinkage, nullptr, "LITERAL");
				glob->setLinkage(GlobalValue::PrivateLinkage);
				glob->setConstant(true);
				glob->setInitializer(ConstantDataArray::getString(context, StringRef(value.c_str() + 1, value.size() - 2)));
				glob->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
				glob->setAlignment(MaybeAlign(1));
				Value* value = builder.CreateInBoundsGEP(
					cast<PointerType>(glob->getType()->getScalarType())->getElementType(),
					glob,
					{ builder.getInt64(0), builder.getInt64(0) }
				);
				buildtime.result.type = &STRING_TYPE;
				buildtime.result.value = new Object(value);
			}
			else
				raiseCompileError("Non-string literals not implemented", params[0]);
			return this;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			const std::string& value = ((MincLiteralExpr*)params[0])->value;
			if (value.back() == '"' || value.back() == '\'')
				return &STRING_TYPE;
			else
				return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$L")[0], new LiteralKernel());

	struct PrintKernel : MincKernel
	{
		Value* fromLlvmString;
		Function* printfFunction;
		PrintKernel() : MincKernel(&LLVM_RUNNER), fromLlvmString(nullptr), printfFunction(nullptr) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			if (fromLlvmString == nullptr)
			{
				// Define "%s from LLVM!\n" string constant
				GlobalVariable* glob = new GlobalVariable(*LLVM_RUNNER.module, ArrayType::get(Type::getInt8Ty(context), strlen("%s from LLVM!\n") + 1), false, GlobalValue::ExternalLinkage, nullptr, "STRING_CONSTANT");
				glob->setLinkage(GlobalValue::PrivateLinkage);
				glob->setConstant(true);
				glob->setInitializer(ConstantDataArray::getString(context, StringRef("%s from LLVM!\n")));
				glob->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
				glob->setAlignment(MaybeAlign(1));
				fromLlvmString = builder.CreateInBoundsGEP(
					cast<PointerType>(glob->getType()->getScalarType())->getElementType(),
					glob,
					{ builder.getInt64(0), builder.getInt64(0) }
				);
			}

			if (printfFunction == nullptr)
			{
				// Declare printf() function
				FunctionType* printfType = FunctionType::get(builder.getInt32Ty(), { builder.getInt8PtrTy() }, true);
				printfFunction = Function::Create(printfType, GlobalValue::ExternalLinkage, "printf", *LLVM_RUNNER.module);
			}

			// Call printf("%s from LLVM!\n", message)
			Object* const message = (Object*)params[0]->build(buildtime).value;
			builder.CreateCall(printfFunction, { fromLlvmString, message->value });
			return this;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("print($E<string>)"), new PrintKernel());
});