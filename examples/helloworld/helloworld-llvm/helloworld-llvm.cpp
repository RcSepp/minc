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

using namespace llvm;

// Constants
#define MODULE_NAME "helloworld" // Internal name of LLVM module
#define OUTPUT_PATH "./helloworld-llvm.o" // Compiled binary object file name
#define OPTIMIZE // If defined, perform function and module optimizations (similar to gcc's `-O3` option)
//#define PRINT_IR // If defined, print generated LLVM IR code to stdout

MincObject STRING_TYPE, META_TYPE;

struct Object : public MincObject
{
	Value* value;
	Object(Value* value) : value(value) {}
};

class HelloworldLlvmPkg : public MincPackage
{
private:
	TargetMachine* target;
	LLVMContext* context;
	IRBuilder<>* builder;
	Module* module;
	legacy::FunctionPassManager* jitFunctionPassManager;
	legacy::PassManager* jitModulePassManager;
	Function *mainFunction, *printfFunction;
	BasicBlock* currentBB;
	Value* fromLlvmString;

public:
	HelloworldLlvmPkg() : MincPackage("helloworld-llvm")
	{
		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();
		InitializeNativeTargetAsmParser();

		target = EngineBuilder().selectTarget();

		// Create context
		context = new LLVMContext();

		// Create builder
		builder = new IRBuilder<>(*context);

		// Create module
		module = new Module(MODULE_NAME, *context);
		module->setTargetTriple(sys::getDefaultTargetTriple());
		module->setDataLayout(target->createDataLayout());

#ifdef OPTIMIZE
		// Create pass manager for module
		jitFunctionPassManager = new legacy::FunctionPassManager(module);
		jitModulePassManager = new legacy::PassManager();
		PassManagerBuilder jitPassManagerBuilder;
		jitPassManagerBuilder.OptLevel = 3; // -O3
		jitPassManagerBuilder.SizeLevel = 0;
		jitPassManagerBuilder.Inliner = createFunctionInliningPass(jitPassManagerBuilder.OptLevel, jitPassManagerBuilder.SizeLevel, false);
		jitPassManagerBuilder.DisableUnrollLoops = false;
		jitPassManagerBuilder.LoopVectorize = true;
		jitPassManagerBuilder.SLPVectorize = true;
		target->adjustPassManager(jitPassManagerBuilder);
		jitPassManagerBuilder.populateFunctionPassManager(*jitFunctionPassManager);
		jitPassManagerBuilder.populateModulePassManager(*jitModulePassManager);
		jitFunctionPassManager->doInitialization();
#endif

		// Define "%s from LLVM!\n" string constant
		GlobalVariable* glob = new GlobalVariable(*module, ArrayType::get(Type::getInt8Ty(*context), strlen("%s from LLVM!\n") + 1), false, GlobalValue::ExternalLinkage, nullptr, "STRING_CONSTANT");
		glob->setLinkage(GlobalValue::PrivateLinkage);
		glob->setConstant(true);
		glob->setInitializer(ConstantDataArray::getString(*context, StringRef("%s from LLVM!\n")));
		glob->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
		glob->setAlignment(MaybeAlign(1));
		fromLlvmString = builder->CreateInBoundsGEP(
			cast<PointerType>(glob->getType()->getScalarType())->getElementType(),
			glob,
			{ builder->getInt64(0), builder->getInt64(0) }
		);

		// Declare printf() function
		FunctionType* printfType = FunctionType::get(builder->getInt32Ty(), { builder->getInt8PtrTy() }, false);
		printfFunction = Function::Create(printfType, GlobalValue::ExternalLinkage, "printf", *module);

		// Create main function
		FunctionType* mainType = FunctionType::get(builder->getInt32Ty(), {}, false);
		mainFunction = Function::Create(mainType, Function::ExternalLinkage, "main", *module);
		mainFunction->setDSOLocal(true);
		mainFunction->addAttribute(AttributeList::FunctionIndex, Attribute::AttrKind::NoInline);

		// Create entry BB in main function
		builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", mainFunction));
	}
	~HelloworldLlvmPkg()
	{
		// Add implicit `return 0;` to main function
		if (!currentBB->getTerminator())
			builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));

		// Close main function
		std::string errstr;
		raw_string_ostream errstream(errstr);
		if (verifyFunction(*mainFunction, &errstream) && errstr.empty())
		{
			std::cerr << "Error verifying main functiion\n";
			return;
		}

#ifdef OPTIMIZE
		// Optimize main function and module
		jitFunctionPassManager->run(*mainFunction);
		jitFunctionPassManager->doFinalization();
		jitModulePassManager->run(*module);
#endif

#ifdef PRINT_IR
		// Print IR
		module->print(outs(), nullptr);
#endif

		// Compile module
		std::error_code err;
		raw_fd_ostream dest(OUTPUT_PATH, err, sys::fs::F_None);
		if (err)
		{
			std::cerr << "Could not open file: " << err.message() << "\n";
			return;
		}

		// Prepare to emit object file
		legacy::PassManager pass;
		if (target->addPassesToEmitFile(pass, dest, nullptr, CodeGenFileType::CGFT_ObjectFile))
		{
			std::cerr << "The TargetMachine can't emit a file of type CGFT_ObjectFile\n";
			return;
		}

		// Compile module
		// Note: To create executable run linker (e.g. `gcc -o helloworld helloworld-llvm.o`)
		pass.run(*module);
		dest.flush();

		// Free LLVM resources
#ifdef OPTIMIZE
		delete jitModulePassManager;
		delete jitFunctionPassManager;
#endif
		delete module;
		delete builder;
		delete context;
	}

	void definePackage(MincBlockExpr* pkgScope)
	{
		pkgScope->defineSymbol("string", &META_TYPE, &STRING_TYPE);

		pkgScope->defineExpr(MincBlockExpr::parseCTplt("$L")[0],
			[&](MincRuntime& runtime, std::vector<MincExpr*>& params) -> bool {
				const std::string& value = ((MincLiteralExpr*)params[0])->value;

				if (value.back() == '"' || value.back() == '\'')
				{
					GlobalVariable* glob = new GlobalVariable(*module, ArrayType::get(Type::getInt8Ty(*context), value.size() - 1), false, GlobalValue::ExternalLinkage, nullptr, "LITERAL");
					glob->setLinkage(GlobalValue::PrivateLinkage);
					glob->setConstant(true);
					glob->setInitializer(ConstantDataArray::getString(*context, StringRef(value.c_str() + 1, value.size() - 2)));
					glob->setUnnamedAddr(GlobalVariable::UnnamedAddr::Global);
					glob->setAlignment(MaybeAlign(1));
					Value* value = builder->CreateInBoundsGEP(
						cast<PointerType>(glob->getType()->getScalarType())->getElementType(),
						glob,
						{ builder->getInt64(0), builder->getInt64(0) }
					);
					runtime.result = MincSymbol(&STRING_TYPE, new Object(value));
				}
				else
					raiseCompileError("Non-string literals not implemented", params[0]);
				return false;
			},
			[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) -> MincObject* {
				const std::string& value = ((MincLiteralExpr*)params[0])->value;
				if (value.back() == '"' || value.back() == '\'')
					return &STRING_TYPE;
				else
					return nullptr;
			}
		);

		pkgScope->defineStmt(MincBlockExpr::parseCTplt("print($E<string>)"),
			[](MincBuildtime& buildtime, std::vector<MincExpr*>& params) {
				params[0]->build(buildtime);
			},
			[&](MincRuntime& runtime, std::vector<MincExpr*>& params) -> bool {
				if (params[0]->run(runtime))
					return true;
				Object* const message = (Object*)runtime.result.value;
				builder->CreateCall(printfFunction, { fromLlvmString, message->value });
				std::cout << "( compiled print statement )\n";
				return false;
			}
		);
	}
} HELLOWORLD_LLVM_PKG;