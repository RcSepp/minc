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

using namespace llvm;

struct BaseType {};

struct BuiltinType : public BaseType
{
private:
	BuiltinType* ptr;

public:
	const char* name;
	LLVMTypeRef llvmtype;
	int32_t align;

	BuiltinType(const char* name, LLVMTypeRef llvmtype, int32_t align)
		: ptr(nullptr), name(name), llvmtype(llvmtype), align(align) {}

	BuiltinType* Ptr()
	{
		if (ptr == nullptr)
		{
			size_t nameLen = strlen(name);
			char* ptrName = new char[nameLen + 3];
			strcpy(ptrName, name);
			strcpy(ptrName + nameLen, "Ptr");
			ptr = new BuiltinType(ptrName, LLVMPointerType(llvmtype, 0), 8);
		}
		return ptr;
	}
};

struct FuncType : public BuiltinType
{
	BuiltinType* resultType;
	std::vector<BuiltinType*> argTypes;

	FuncType(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg)
		: BuiltinType(name, nullptr, 8), resultType(resultType), argTypes(argTypes)
	{
		std::vector<llvm::Type*> argLlvmTypes;
		for (BuiltinType* argType: argTypes)
			argLlvmTypes.push_back(unwrap(argType->llvmtype));
		llvmtype = wrap(FunctionType::get(unwrap(resultType->llvmtype), argLlvmTypes, isVarArg));
	}
};

struct XXXValue
{
private:
	uint64_t constantValue;

public:
	llvm::Value* val;

	XXXValue(llvm::Value* val)
		: val(val), constantValue(0) {}
	XXXValue(llvm::Type* type, uint64_t value)
		: val(llvm::Constant::getIntegerValue(type, llvm::APInt(64, value))), constantValue(value) {}

	uint64_t getConstantValue()
	{
		return constantValue;
	}

	virtual llvm::Function* getFunction(llvm::Module* module)
	{
		return nullptr;
	}

	virtual bool isFunction()
	{
		return false;
	}
};

struct Func : XXXValue
{
public:
	FuncType type;
	const char* symName;

	Func(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg, const char* symName = nullptr)
		: XXXValue(nullptr), type(name, resultType, argTypes, isVarArg), symName(symName ? symName : name) {}

	llvm::Function* getFunction(llvm::Module* module)
	{
		/*if (!val)
			val = Function::Create((llvm::FunctionType*)unwrap(type.llvmtype), GlobalValue::ExternalLinkage, symName, module);
		return (llvm::Function*)val;*/
		val = module->getFunction(symName);
		if (val == nullptr)
			val = Function::Create((llvm::FunctionType*)unwrap(type.llvmtype), GlobalValue::ExternalLinkage, symName, module);
		return (llvm::Function*)val;
	}

	bool isFunction()
	{
		return true;
	}	
};

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
	extern StructType* AST;
	extern StructType* ExprAST;
	extern StructType* LiteralExprAST;
	extern StructType* IdExprAST;
	extern StructType* BlockExprAST;
	extern StructType* StmtAST;

	void create(LLVMContext& c);
};

namespace BuiltinTypes
{
	extern BuiltinType* Builtin;

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
	extern BuiltinType* ExprAST;
	extern BuiltinType* LiteralExprAST;
	extern BuiltinType* IdExprAST;

	// Misc. types
	extern BuiltinType* Function;
};

void create_llvm_c_constants(LLVMContext& c, std::map<std::string, Value*>& llvm_c_constants);
void create_llvm_c_functions(LLVMContext& c, std::list<Func>& llvm_c_functions);

#endif