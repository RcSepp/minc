#include <string>
#include <cstring>
#include <filesystem>
#include <iostream>
#include "minc_api.hpp"
#include "minc_pkgmgr.h"

// LLVM IR creation
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/CFG.h>

// LLVM compilation
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

// LLVM optimization
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>

#include "minc_llvm.h"

#define DUMP_LLVM_IR

LlvmRunner LLVM_RUNNER;

extern "C"
{
	// Wrap handover method in C function to avoid name mangling
	void handover(MincRunner& from, MincRunner& to)
	{
		return from.handover(to);
	}
}

LlvmRunner::LlvmRunner()
	: MincRunner("LLVM")
{
}

void LlvmRunner::buildBegin(MincBuildtime& buildtime)
{
	// Create >>> Create minc types

	mincRunnerType = llvm::StructType::create(*context, "struct.MincRunner");
	mincExprType = llvm::StructType::create(*context, "class.MincExpr");
	mincBlockExprType = llvm::StructType::create(*context, "class.MincBlockExpr");
	mincObjectType = llvm::StructType::create(*context, "struct.MincObject");
	llvm::StructType* mincStackFrameType = llvm::StructType::create(*context, "struct.MincStackFrame");
	mincRuntimeType = llvm::StructType::create("struct.MincRuntime",
		mincBlockExprType->getPointerTo(),
		mincExprType->getPointerTo(),
		mincObjectType->getPointerTo(),
		mincObjectType->getPointerTo(),
		builder->getInt8Ty(),
		mincStackFrameType->getPointerTo(),
		mincStackFrameType->getPointerTo(),
		builder->getInt64Ty(),
		builder->getInt64Ty(),
		builder->getInt8PtrTy(),
		builder->getInt64Ty()
	);
	mincKernelType = llvm::StructType::create("struct.MincKernel",
		llvm::FunctionType::get(builder->getInt32Ty(), true)->getPointerTo()->getPointerTo(),
		mincRunnerType->getPointerTo()
	);
	mincStackSymbolType = llvm::StructType::create("struct.MincStackSymbol",
		mincObjectType->getPointerTo(),
		mincBlockExprType->getPointerTo(),
		builder->getInt64Ty()
	);
	mincEnteredBlockExprType = llvm::StructType::create("struct.MincEnteredBlockExpr",
		mincRuntimeType->getPointerTo(),
		mincBlockExprType->getPointerTo(),
		mincStackFrameType->getPointerTo()
	);
	mincInteropDataType = llvm::StructType::create("struct.MincInteropData",
		mincExprType->getPointerTo(),
		mincObjectType->getPointerTo()
	);
}

void LlvmRunner::buildEnd(MincBuildtime& buildtime)
{
}

void LlvmRunner::buildBeginFile(MincBuildtime& buildtime, const char* path)
{
	// Create file module
	std::string filename = path;
	size_t c = filename.find_last_of("/\\");
	if (c != (size_t)-1)
		filename = filename.substr(c + 1);
	c = filename.find('.');
	if (c != (size_t)-1)
		filename = filename.substr(0, c);
	module = new ::Module(filename, *context);

	// >>> Create threading extern functions

	handoverFunc = llvm::Function::Create(
		llvm::FunctionType::get(builder->getVoidTy(), {mincRunnerType->getPointerTo(), mincRunnerType->getPointerTo()}, false),
		llvm::Function::ExternalLinkage, "handover", module
	);
	handoverFunc->setDSOLocal(true);

	// >>> Create minc extern functions

	llvm::FunctionType* runExprFuncType = llvm::FunctionType::get(builder->getInt8Ty(), {mincExprType->getPointerTo(), mincRuntimeType->getPointerTo()}, false);
	runExprFunc = llvm::Function::Create(runExprFuncType, llvm::Function::ExternalLinkage, "runExpr", module);
	runExprFunc->setDSOLocal(true);
	// runExprFunc->addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AttrKind::ZExt);

	llvm::FunctionType* getStackSymbolFuncType = llvm::FunctionType::get(mincObjectType->getPointerTo(), {mincRuntimeType->getPointerTo(), mincStackSymbolType->getPointerTo()}, false);
	getStackSymbolFunc = llvm::Function::Create(getStackSymbolFuncType, llvm::Function::ExternalLinkage, "getStackSymbol", module);
	getStackSymbolFunc->setDSOLocal(true);
	// getStackSymbolFunc->addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AttrKind::ZExt);

	llvm::FunctionType* enterBlockExprFuncType = llvm::FunctionType::get(mincEnteredBlockExprType->getPointerTo(), {mincRuntimeType->getPointerTo(), mincBlockExprType->getPointerTo()}, false);
	enterBlockExprFunc = llvm::Function::Create(enterBlockExprFuncType, llvm::Function::ExternalLinkage, "enterBlockExpr", module);
	enterBlockExprFunc->setDSOLocal(true);
	// enterBlockExprFunc->addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AttrKind::ZExt);

	llvm::FunctionType* exitBlockExprFuncType = llvm::FunctionType::get(builder->getVoidTy(), {mincEnteredBlockExprType->getPointerTo()}, false);
	exitBlockExprFunc = llvm::Function::Create(exitBlockExprFuncType, llvm::Function::ExternalLinkage, "exitBlockExpr", module);
	exitBlockExprFunc->setDSOLocal(true);
	// exitBlockExprFunc->addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AttrKind::ZExt);

	// Create global variable to hold interop data
	mincInteropDataValue = new llvm::GlobalVariable(
		*module,
		mincInteropDataType->getPointerTo(),
		false,
		llvm::GlobalValue::PrivateLinkage,
		llvm::Constant::getNullValue(mincInteropDataType->getPointerTo()),
		"MINC_INTEROP_DATA"
	);
	mincInteropDataValue->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
	mincInteropDataValue->setAlignment(llvm::Align(8));

	// Create mincMain function
	llvm::FunctionType* mincMainType = llvm::FunctionType::get(builder->getVoidTy(), {
		mincInteropDataType->getPointerTo()
	}, false);
	llvm::Function* mincMainFunction = module->createFunction(mincMainType, llvm::Function::ExternalLinkage, "mincMain");
	mincMainFunction->setDSOLocal(true);
	mincMainFunction->addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AttrKind::NoInline);
	builder->SetInsertPoint(llvm::BasicBlock::Create(*context, "entry", mincMainFunction));
	builder->CreateStore(mincMainFunction->getArg(0), mincInteropDataValue);

	exprSwitchStack.push(ExprSwitch(mincInteropDataValue));
}

void LlvmRunner::buildEndFile(MincBuildtime& buildtime, const char* path)
{
	if (!exprSwitchStack.empty())
		exprSwitchStack.pop();

	// Set interopData.followupExpr = nullptr
	llvm::Value* nullValue = llvm::Constant::getNullValue(mincExprType->getPointerTo());
	llvm::Value* followupExprValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(0) });
	builder->CreateStore(nullValue, followupExprValue)->setAlignment(llvm::Align(8));

	// Finalize mincMain function and file module
	builder->CreateRetVoid();
	module->finalize();
#ifdef DUMP_LLVM_IR
	module->dump();
#endif

	// Get executable pointer to mincMain function
	mincMain = (MainFunction)module->getFunctionAddress("mincMain");
}

void LlvmRunner::buildStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	exprSwitchStack.push(ExprSwitch(mincInteropDataValue));
	MincRunner::buildStmt(buildtime, stmt);
	exprSwitchStack.pop();
}

void LlvmRunner::buildSuspendStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	// Set interopData.followupExpr = nullptr
	llvm::Value* nullValue = llvm::Constant::getNullValue(mincExprType->getPointerTo());
	llvm::Value* followupExprValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(0) });
	builder->CreateStore(nullValue, followupExprValue)->setAlignment(llvm::Align(8));

	// Handover to stmt->resolvedKernel->runner
	llvm::Value* thisRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)this, true));
	llvm::Value* nextRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)&stmt->resolvedKernel->runner, true));
	builder->CreateCall(handoverFunc, {thisRunnerValue, nextRunnerValue});

	exprSwitchStack.push(ExprSwitch(mincInteropDataValue));
}

void LlvmRunner::buildResumeStmt(MincBuildtime& buildtime, MincStmt* stmt)
{
	exprSwitchStack.pop();
}

void LlvmRunner::buildSuspendExpr(MincBuildtime& buildtime, MincExpr* expr)
{
	exprSwitchStack.push(ExprSwitch(mincInteropDataValue));

	// Set interopData.followupExpr = expr
	llvm::Value* exprValue = llvm::Constant::getIntegerValue(mincExprType->getPointerTo(), llvm::APInt(64, (uint64_t)expr, true));
	llvm::Value* followupExprValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(0) });
	builder->CreateStore(exprValue, followupExprValue)->setAlignment(llvm::Align(8));

	// Handover to expr->resolvedKernel->runner
	llvm::Value* thisRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)this, true));
	llvm::Value* nextRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)&expr->resolvedKernel->runner, true));
	builder->CreateCall(handoverFunc, {thisRunnerValue, nextRunnerValue});
}

void LlvmRunner::buildResumeExpr(MincBuildtime& buildtime, MincExpr* expr)
{
	exprSwitchStack.pop();

	//TODO: Test this with switch stack. It may have to be moved to the end of buildSuspendExpr(), in which currentRunner->buildSuspendExpr
	//		would have to be moved after resolvedKernel->runner.buildNestedExpr in case MincExpr::build() to avoid overwriting buildtime.result.value
	llvm::Value* exprResultValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(1) });
	buildtime.result.value = (MincObject*)exprResultValue;
}

void LlvmRunner::buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next)
{
	// Wrap expr and handover in expr-switch case
	exprSwitchStack.top().beginCast(expr);
	buildExpr(buildtime, expr);

	// Set interopData.followupExpr = nullptr
	llvm::Value* nullValue = llvm::Constant::getNullValue(mincExprType->getPointerTo());
	llvm::Value* followupExprValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(0) });
	builder->CreateStore(nullValue, followupExprValue)->setAlignment(llvm::Align(8));

	// Set interopData.exprResult = buildtime.result.value
	llvm::Value* exprResult = buildtime.result.value == nullptr ? llvm::Constant::getNullValue(mincObjectType->getPointerTo()) : (llvm::Value*)buildtime.result.value;
	llvm::Value* exprResultValue = builder->CreateInBoundsGEP(mincInteropDataType, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(1) });
	builder->CreateStore(exprResult, exprResultValue)->setAlignment(llvm::Align(8));

	// Handover to next
	llvm::Value* thisRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)this, true));
	llvm::Value* nextRunnerValue = llvm::Constant::getIntegerValue(mincRunnerType->getPointerTo(), llvm::APInt(64, (uint64_t)&next, true));
	builder->CreateCall(handoverFunc, {thisRunnerValue, nextRunnerValue});

	// End expr-switch case
	exprSwitchStack.top().endCast();
}

int LlvmRunner::run(MincExpr* expr, MincInteropData& interopData)
{
	mincMain(interopData);
	return 0; // TODO: Change return type of mincMain to int
}

void LlvmRunner::ExprSwitch::beginExprSwitch()
{
	llvm::Function* const currentFunction = builder->GetInsertBlock()->getParent();
	switchBlock = llvm::BasicBlock::Create(*context, "beginExprSwitch", currentFunction);
	builder->CreateBr(switchBlock);
	builder->SetInsertPoint(switchBlock);

	defaultBlock = llvm::BasicBlock::Create(*context, "endExprSwitch", currentFunction);
	llvm::Value* followupExprValue = builder->CreateInBoundsGEP(nullptr, builder->CreateLoad(mincInteropDataValue), { builder->getInt64(0), builder->getInt32(0) });
	llvm::Value* switchValue = builder->CreatePtrToInt(builder->CreateLoad(followupExprValue), builder->getInt64Ty());
	//TODO: Can we evaluate expr addresses to indices at compile using a map<> time to enable switch jump table optimizations?
	switchInst = builder->CreateSwitch(switchValue, defaultBlock);
}
void LlvmRunner::ExprSwitch::endExprSwitch()
{
	builder->SetInsertPoint(defaultBlock);
}

LlvmRunner::ExprSwitch::ExprSwitch(llvm::GlobalVariable* mincInteropDataValue)
	: switchInst(nullptr), mincInteropDataValue(mincInteropDataValue)
{
}

LlvmRunner::ExprSwitch::~ExprSwitch()
{
	if (switchInst != nullptr)
		endExprSwitch();
}

void LlvmRunner::ExprSwitch::beginCast(MincExpr* expr)
{
	if (switchInst == nullptr) // Defer switch creation until it's actually needed
		beginExprSwitch();

	prevBlock = builder->GetInsertBlock();
	llvm::BasicBlock* caseBlock = llvm::BasicBlock::Create(*context, "exprCase", prevBlock->getParent());
	switchInst->addCase((llvm::ConstantInt*)llvm::Constant::getIntegerValue(builder->getInt64Ty(), llvm::APInt(64, (uint64_t)expr, true)), caseBlock);

	builder->SetInsertPoint(caseBlock);
}

void LlvmRunner::ExprSwitch::endCast()
{
	builder->CreateBr(switchBlock);
	builder->SetInsertPoint(prevBlock);
}
