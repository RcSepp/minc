#include "llvm_constants.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern Function* currentFunc;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;
extern DIFile* dfile;
extern Value* closure;

namespace Types
{
	// LLVM-c types
@	LLVM_TYPE_DECL@

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
	StructType* BaseType;
	StructType* Variable;
	PointerType* BuiltinType;

	// AST types
	StructType* Location;
	StructType* AST;
	StructType* ExprAST;
	StructType* LiteralExprAST;
	StructType* IdExprAST;
	StructType* BlockExprAST;
	StructType* StmtAST;

	void create(LLVMContext& c)
	{
@		LLVM_TYPE_DEF@

		Void = (StructType*)unwrap(LLVMVoidType());
		VoidPtr = (StructType*)unwrap(LLVMPointerType(LLVMVoidType(), 0));
		Int1 = (StructType*)unwrap(LLVMInt1Type());
		Int1Ptr = (StructType*)unwrap(LLVMPointerType(LLVMInt1Type(), 0));
		Int8 = (StructType*)unwrap(LLVMInt8Type());
		Int8Ptr = (StructType*)unwrap(LLVMPointerType(LLVMInt8Type(), 0));
		Int16 = (StructType*)unwrap(LLVMInt16Type());
		Int16Ptr = (StructType*)unwrap(LLVMPointerType(LLVMInt16Type(), 0));
		Int32 = (StructType*)unwrap(LLVMInt32Type());
		Int32Ptr = (StructType*)unwrap(LLVMPointerType(LLVMInt32Type(), 0));
		Int64 = (StructType*)unwrap(LLVMInt64Type());
		Int64Ptr = (StructType*)unwrap(LLVMPointerType(LLVMInt64Type(), 0));
		Half = (StructType*)unwrap(LLVMHalfType());
		HalfPtr = (StructType*)unwrap(LLVMPointerType(LLVMHalfType(), 0));
		Float = (StructType*)unwrap(LLVMFloatType());
		FloatPtr = (StructType*)unwrap(LLVMPointerType(LLVMFloatType(), 0));
		Double = (StructType*)unwrap(LLVMDoubleType());
		DoublePtr = (StructType*)unwrap(LLVMPointerType(LLVMDoubleType(), 0));

		LLVMType = StructType::create(c, "class.llvm::Type");
		LLVMValue = StructType::create(c, "class.llvm::Value");
		Value = StructType::create(c, "struct.Value");
		BaseType = StructType::create(c, "BaseType");
		Variable = StructType::create("struct.Variable",
			BaseType->getPointerTo(),
			Value->getPointerTo()
		);
		BuiltinType = StructType::create("BuiltinType",
			Type::getInt8PtrTy(c),
			Type::getInt8PtrTy(c),
			LLVMOpaqueType->getPointerTo(),
			Type::getInt32Ty(c)
		)->getPointerTo();

		Location = StructType::create("struct.Location",
			Type::getInt8PtrTy(c),
			Type::getInt32Ty(c),
			Type::getInt32Ty(c),
			Type::getInt32Ty(c),
			Type::getInt32Ty(c)
		);
		AST = StructType::create("class.AST",
			Location
		);
		ExprAST = StructType::create("class.ExprAST",
			FunctionType::get(Type::getInt32Ty(c), true)->getPointerTo()->getPointerTo(),
			AST,
			Type::getInt32Ty(c),
			Value->getPointerTo()
		);
		LiteralExprAST = StructType::create("class.LiteralExprAST",
			ExprAST,
			Type::getInt8PtrTy(c)
		);
		IdExprAST = StructType::create("class.IdExprAST",
			ExprAST,
			Type::getInt8PtrTy(c)
		);
		BlockExprAST = StructType::create(c, "class.BlockExprAST");
		StmtAST = StructType::create(c, "class.StmtAST");
	}
};

namespace BuiltinTypes
{
	BuiltinType* Builtin;

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
	BuiltinType* ExprAST;
	BuiltinType* LiteralExprAST;
	BuiltinType* IdExprAST;
	BuiltinType* BlockExprAST;

	// Misc. types
	BuiltinType* Function;
};

void create_llvm_c_constants(LLVMContext& c, std::map<std::string, Value*>& llvm_c_constants)
{
	llvm_c_constants["LLVMIntEQ"] = Constant::getIntegerValue(Types::Int32, APInt(32, 32, true));
	llvm_c_constants["LLVMIntNE"] = Constant::getIntegerValue(Types::Int32, APInt(32, 33, true));
}

void create_llvm_c_functions(LLVMContext& c, std::list<Func>& llvm_c_functions)
{
llvm_c_functions.push_back(Func("puts", BuiltinTypes::Int32, { BuiltinTypes::Int8Ptr }, false));
llvm_c_functions.push_back(Func("printf", BuiltinTypes::Int32, { BuiltinTypes::Int8Ptr }, true));
llvm_c_functions.push_back(Func("atoi", BuiltinTypes::Int32, { BuiltinTypes::Int8Ptr }, false));
llvm_c_functions.push_back(Func("malloc", BuiltinTypes::Int8Ptr, { BuiltinTypes::Int64 }, false));
llvm_c_functions.push_back(Func("free", BuiltinTypes::Void, { BuiltinTypes::Int8Ptr }, false));

@	LLVM_EXTERN_FUNC_DEF@
}

extern "C"
{
@	MODIFIED_LLVM_EXTERN_FUNC_DEF@
}