#ifndef __LLVM_CONSTANTS_H
#define __LLVM_CONSTANTS_H

#include <list>
#include <map>
#include <string>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include "types.h"

using namespace llvm;

namespace Types
{
	// LLVM-c types
@	LLVM_TYPE_EXTERN_DECL@

	// LLVM primitive types
	extern StructType* Void;
	extern StructType* VoidPtr;
	extern StructType* Int1;
	extern StructType* Int1Ptr;
	extern StructType* Int8;
	extern StructType* Int8Ptr;
	extern StructType* Int16;
	extern StructType* Int16Ptr;
	extern StructType* Int32;
	extern StructType* Int32Ptr;
	extern StructType* Int64;
	extern StructType* Int64Ptr;
	extern StructType* Half;
	extern StructType* HalfPtr;
	extern StructType* Float;
	extern StructType* FloatPtr;
	extern StructType* Double;
	extern StructType* DoublePtr;

	// Misc. types
	extern StructType* LLVMType;
	extern StructType* LLVMValue;
	extern StructType* Value;
	extern StructType* BaseType;
	extern StructType* Variable;
	extern PointerType* BuiltinType;

	// AST types
	extern StructType* Location;
	extern StructType* ExprAST;
	extern StructType* ExprListAST;
	extern StructType* LiteralExprAST;
	extern StructType* IdExprAST;
	extern StructType* CastExprAST;
	extern StructType* BlockExprAST;
	extern StructType* StmtAST;

	void create(LLVMContext& c);
};

namespace BuiltinTypes
{
	extern BuiltinType* Base;
	extern BuiltinType* Builtin;
	extern BuiltinType* BuiltinValue;
	extern BuiltinType* BuiltinFunction;
	extern BuiltinType* BuiltinClass;
	extern BuiltinType* BuiltinInstance;
	extern BuiltinType* Value;

	// Primitive types
	extern BuiltinType* Void;
	extern BuiltinType* VoidPtr;
	extern BuiltinType* Int1;
	extern BuiltinType* Int1Ptr;
	extern BuiltinType* Int8;
	extern BuiltinType* Int8Ptr;
	extern BuiltinType* Int16;
	extern BuiltinType* Int16Ptr;
	extern BuiltinType* Int32;
	extern BuiltinType* Int32Ptr;
	extern BuiltinType* Int64;
	extern BuiltinType* Int64Ptr;
	extern BuiltinType* Half;
	extern BuiltinType* HalfPtr;
	extern BuiltinType* Float;
	extern BuiltinType* FloatPtr;
	extern BuiltinType* Double;
	extern BuiltinType* DoublePtr;

	// LLVM types
	extern BuiltinType* LLVMAttributeRef;
	extern BuiltinType* LLVMBasicBlockRef;
	extern BuiltinType* LLVMBuilderRef;
	extern BuiltinType* LLVMContextRef;
	extern BuiltinType* LLVMDiagnosticInfoRef;
	extern BuiltinType* LLVMDIBuilderRef;
	extern BuiltinType* LLVMMemoryBufferRef;
	extern BuiltinType* LLVMMetadataRef;
	extern BuiltinType* LLVMModuleRef;
	extern BuiltinType* LLVMModuleFlagEntryRef;
	extern BuiltinType* LLVMModuleProviderRef;
	extern BuiltinType* LLVMNamedMDNodeRef;
	extern BuiltinType* LLVMPassManagerRef;
	extern BuiltinType* LLVMPassRegistryRef;
	extern BuiltinType* LLVMTypeRef;
	extern BuiltinType* LLVMUseRef;
	extern BuiltinType* LLVMValueRef;
	extern BuiltinType* LLVMValueMetadataEntryRef;

	// AST types
	extern BuiltinType* Location;
	extern BuiltinType* ExprAST;
	extern BuiltinType* ExprListAST;
	extern BuiltinType* LiteralExprAST;
	extern BuiltinType* IdExprAST;
	extern BuiltinType* CastExprAST;
	extern BuiltinType* BlockExprAST;
	extern BuiltinType* StmtAST;
};

void create_llvm_c_constants(LLVMContext& c, std::map<std::string, Value*>& llvm_c_constants);
void create_llvm_c_functions(LLVMContext& c, std::list<Func>& llvm_c_functions);

#endif