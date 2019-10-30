#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;

extern "C"
{
@	PAWS_MODIFIED_LLVM_EXTERN_FUNC_DEF@
	void PAWSLLVMPositionBuilder(LLVMBasicBlockRef Block) { builder->SetInsertPoint(currentBB = unwrap(Block)); }
	LLVMValueRef PAWSLLVMBuildInBoundsGEP1(LLVMValueRef Pointer, LLVMValueRef Idx0, const char *Name) { return LLVMBuildInBoundsGEP(wrap(builder), Pointer, &Idx0, 1, Name); }
	LLVMValueRef PAWSLLVMBuildInBoundsGEP2(LLVMValueRef Pointer, LLVMValueRef Idx0, LLVMValueRef Idx1, const char *Name) { LLVMValueRef Idxs[] = { Idx0, Idx1 }; return LLVMBuildInBoundsGEP(wrap(builder), Pointer, Idxs, 2, Name); }
	LLVMValueRef PAWSLLVMConstInBoundsGEP1(LLVMValueRef ConstantVal, LLVMValueRef Idx0) { return LLVMConstInBoundsGEP(ConstantVal, &Idx0, 1); }
	LLVMValueRef PAWSLLVMConstInBoundsGEP2(LLVMValueRef ConstantVal, LLVMValueRef Idx0, LLVMValueRef Idx1) { LLVMValueRef Idxs[] = { Idx0, Idx1 }; return LLVMConstInBoundsGEP(ConstantVal, Idxs, 2); }
	LLVMMetadataRef PAWSLLVMDIBuilderCreateExpression() { return LLVMDIBuilderCreateExpression(wrap(dbuilder), nullptr, 0); }
	LLVMMetadataRef PAWSLLVMDIBuilderCreateDebugLocation(unsigned Line, unsigned Column, LLVMMetadataRef Scope) { return LLVMDIBuilderCreateDebugLocation(LLVMGetGlobalContext(), Line, Column, Scope, nullptr); }
}

struct PawsLLVM : public PawsPackage
{
	PawsLLVM() : PawsPackage("llvm") {}

private:
	void define(BlockExprAST* pkgScope)
	{
		registerType<PawsType<LLVMTypeRef>>(pkgScope, "PawsLLVMTypeRef");
		registerType<PawsType<LLVMTypeRef*>>(pkgScope, "PawsLLVMTypeRefArray");
		registerType<PawsType<LLVMValueRef>>(pkgScope, "PawsLLVMValueRef");
		registerType<PawsType<LLVMContextRef>>(pkgScope, "PawsLLVMContextRef");

		defineExpr2(pkgScope, "[ $E<PawsLLVMTypeRef>, ... ]",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				std::vector<ExprAST*>& elements = getExprListASTExpressions((ExprListAST*)params[0]);
				LLVMTypeRef* arr = new LLVMTypeRef[elements.size()];
				for (size_t i = 0; i < elements.size(); ++i)
					arr[i] = ((PawsType<LLVMTypeRef>*)codegenExpr(elements[i], parentBlock).value)->val;
				return Variable(PawsType<LLVMTypeRef*>::TYPE, new PawsType<LLVMTypeRef*>(arr));
			},
			PawsType<LLVMTypeRef*>::TYPE
		);

@		PAWS_LLVM_EXTERN_FUNC_DEF@
		defineExpr(pkgScope, "LLVMPositionBuilder($E<PawsLLVMBasicBlockRef>)", +[] (LLVMBasicBlockRef Block) -> void { return PAWSLLVMPositionBuilder(Block); });
		defineExpr(pkgScope, "LLVMBuildInBoundsGEP1($E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>, $E<PawsString>)", +[] (LLVMValueRef Pointer, LLVMValueRef Idx0, const char *Name) -> LLVMValueRef { return PAWSLLVMBuildInBoundsGEP1(Pointer, Idx0, Name); });
		defineExpr(pkgScope, "LLVMBuildInBoundsGEP2($E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>, $E<PawsString>)", +[] (LLVMValueRef Pointer, LLVMValueRef Idx0, LLVMValueRef Idx1, const char *Name) -> LLVMValueRef { return PAWSLLVMBuildInBoundsGEP2(Pointer, Idx0, Idx1, Name); });
		defineExpr(pkgScope, "LLVMConstInBoundsGEP1($E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>)", +[] (LLVMValueRef ConstVal, LLVMValueRef Idx0) -> LLVMValueRef { return PAWSLLVMConstInBoundsGEP1(ConstVal, Idx0); });
		defineExpr(pkgScope, "LLVMConstInBoundsGEP2($E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>, $E<PawsLLVMValueRef>)", +[] (LLVMValueRef ConstVal, LLVMValueRef Idx0, LLVMValueRef Idx1) -> LLVMValueRef { return PAWSLLVMConstInBoundsGEP2(ConstVal, Idx0, Idx1); });
		defineExpr(pkgScope, "LLVMDIBuilderCreateExpression()", +[] () -> LLVMMetadataRef { return PAWSLLVMDIBuilderCreateExpression(); });
		defineExpr(pkgScope, "LLVMDIBuilderCreateDebugLocation($E<PawsInt>, $E<PawsInt>, $E<PawsLLVMMetadataRef>)", +[] (unsigned Line, unsigned Column, LLVMMetadataRef Scope) -> LLVMMetadataRef { return PAWSLLVMDIBuilderCreateDebugLocation(Line, Column, Scope); });
	}
} PAWS_LLVM;