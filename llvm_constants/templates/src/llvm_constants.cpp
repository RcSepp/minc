#include "llvm_constants.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;

namespace Types
{
	// LLVM-c types
	StructType* LLVMOpaquePassRegistry;
	StructType* LLVMOpaqueContext;
	StructType* LLVMOpaqueDiagnosticInfo;
	StructType* LLVMOpaqueAttributeRef;
	StructType* LLVMOpaqueModule;
	StructType* LLVMOpaqueModuleFlagEntry;
	StructType* LLVMOpaqueMetadata;
	StructType* LLVMOpaqueValue;
	StructType* LLVMOpaqueType;
	StructType* LLVMOpaqueValueMetadataEntry;
	StructType* LLVMOpaqueUse;
	StructType* LLVMOpaqueNamedMDNode;
	StructType* LLVMOpaqueBasicBlock;
	StructType* LLVMOpaqueBuilder;
	StructType* LLVMOpaqueModuleProvider;
	StructType* LLVMOpaqueMemoryBuffer;
	StructType* LLVMOpaquePassManager;
	StructType* LLVMOpaqueDIBuilder;

	// LLVM primitive types
	StructType* Void;
	StructType* VoidPtr;
	StructType* Int1;
	StructType* Int1Ptr;
	StructType* Int8;
	StructType* Int8Ptr;
	StructType* Int16;
	StructType* Int16Ptr;
	StructType* Int32;
	StructType* Int32Ptr;
	StructType* Int64;
	StructType* Int64Ptr;
	StructType* Half;
	StructType* HalfPtr;
	StructType* Float;
	StructType* FloatPtr;
	StructType* Double;
	StructType* DoublePtr;

	// Misc. types
	StructType* LLVMType;
	StructType* LLVMValue;
	StructType* Value;
	StructType* Func;
	StructType* BaseType;
	StructType* Variable;
	PointerType* BuiltinType;
	PointerType* BuiltinPtrType;

	// AST types
	StructType* Location;
	StructType* ExprAST;
	StructType* ExprListAST;
	StructType* LiteralExprAST;
	StructType* IdExprAST;
	StructType* CastExprAST;
	StructType* BlockExprAST;
	StructType* StmtAST;
};

namespace BuiltinTypes
{
	// Misc. types
	BuiltinType* Base;
	BuiltinType* Builtin;
	BuiltinType* BuiltinPtr;
	BuiltinType* BuiltinValue;
	BuiltinType* BuiltinFunction;
	BuiltinType* BuiltinClass;
	BuiltinType* BuiltinInstance;
	BuiltinType* Value;
	BuiltinType* Func;

	// Primitive types
	BuiltinType* Void;
	BuiltinType* VoidPtr;
	BuiltinType* Int1;
	BuiltinType* Int1Ptr;
	BuiltinType* Int8;
	BuiltinType* Int8Ptr;
	BuiltinType* Int16;
	BuiltinType* Int16Ptr;
	BuiltinType* Int32;
	BuiltinType* Int32Ptr;
	BuiltinType* Int64;
	BuiltinType* Int64Ptr;
	BuiltinType* Half;
	BuiltinType* HalfPtr;
	BuiltinType* Float;
	BuiltinType* FloatPtr;
	BuiltinType* Double;
	BuiltinType* DoublePtr;

	// LLVM types
	BuiltinType* LLVMAttributeRef;
	BuiltinType* LLVMBasicBlockRef;
	BuiltinType* LLVMBuilderRef;
	BuiltinType* LLVMContextRef;
	BuiltinType* LLVMDiagnosticInfoRef;
	BuiltinType* LLVMDIBuilderRef;
	BuiltinType* LLVMMemoryBufferRef;
	BuiltinType* LLVMMetadataRef;
	BuiltinType* LLVMModuleRef;
	BuiltinType* LLVMModuleFlagEntryRef;
	BuiltinType* LLVMModuleProviderRef;
	BuiltinType* LLVMNamedMDNodeRef;
	BuiltinType* LLVMPassManagerRef;
	BuiltinType* LLVMPassRegistryRef;
	BuiltinType* LLVMTypeRef;
	BuiltinType* LLVMUseRef;
	BuiltinType* LLVMValueRef;
	BuiltinType* LLVMValueMetadataEntryRef;

	// AST types
	BuiltinType* Location;
	BuiltinType* ExprAST;
	BuiltinType* ExprListAST;
	BuiltinType* LiteralExprAST;
	BuiltinType* IdExprAST;
	BuiltinType* CastExprAST;
	BuiltinType* BlockExprAST;
	BuiltinType* StmtAST;
};

extern "C"
{
@	MODIFIED_LLVM_EXTERN_FUNC_DEF@
	void LLVMEXPositionBuilder(LLVMBasicBlockRef bb) { builder->SetInsertPoint(currentBB = unwrap(bb)); }
	LLVMValueRef LLVMEXBuildInBoundsGEP1(LLVMValueRef Pointer, LLVMValueRef Idx0, const char *Name) { return LLVMBuildInBoundsGEP(wrap(builder), Pointer, &Idx0, 1, Name); }
	LLVMValueRef LLVMEXBuildInBoundsGEP2(LLVMValueRef Pointer, LLVMValueRef Idx0, LLVMValueRef Idx1, const char *Name) { LLVMValueRef Idxs[] = { Idx0, Idx1 }; return LLVMBuildInBoundsGEP(wrap(builder), Pointer, Idxs, 2, Name); }
	LLVMValueRef LLVMEXConstInBoundsGEP1(LLVMValueRef ConstantVal, LLVMValueRef Idx0) { return LLVMConstInBoundsGEP(ConstantVal, &Idx0, 1); }
	LLVMValueRef LLVMEXConstInBoundsGEP2(LLVMValueRef ConstantVal, LLVMValueRef Idx0, LLVMValueRef Idx1) { LLVMValueRef Idxs[] = { Idx0, Idx1 }; return LLVMConstInBoundsGEP(ConstantVal, Idxs, 2); }
	LLVMMetadataRef LLVMEXDIBuilderCreateExpression() { return LLVMDIBuilderCreateExpression(wrap(dbuilder), nullptr, 0); }
	LLVMMetadataRef LLVMEXDIBuilderCreateDebugLocation(unsigned Line, unsigned Column, LLVMMetadataRef Scope) { return LLVMDIBuilderCreateDebugLocation(LLVMGetGlobalContext(), Line, Column, Scope, nullptr); }
}