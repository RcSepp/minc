//#define DEBUG_PARAMETER_COLLECTION
#include <sstream>

// LLVM-C //DELETE
#include <llvm-c/Core.h> //DELETE

// LLVM IR creation
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

#include "api.h"
#include "llvm_constants.h"

extern llvm::LLVMContext* context;
extern llvm::IRBuilder<>* builder;
extern llvm::Module* currentModule;
extern llvm::Function* currentFunc;
extern llvm::BasicBlock* currentBB;
extern llvm::DIBuilder* dbuilder;
extern llvm::DIFile* dfile;
extern llvm::DIBasicType* intType;
extern llvm::Value* closure;

std::list<Func> llvm_c_functions;
std::list<std::pair<std::string, const Variable>> capturedScope;

namespace MincFunctions
{
	Func* getExprListASTExpression;
	Func* getExprListASTSize;
	Func* getIdExprASTName;
	Func* getLiteralExprASTValue;
	Func* getBlockExprASTParent;
	Func* setBlockExprASTParent;
	Func* getCastExprASTSource;
	Func* getExprLine;
	Func* getExprColumn;
	Func* getPointerToBuiltinType;

	Func* addToScope;
	Func* addToFileScope;
	Func* lookupScope;
	Func* codegenExprValue;
	Func* codegenExprConstant;
	Func* codegenStmt;
	Func* getType;
	//Func* defineStmt;
	Func* getValueFunction;
	Func* createFuncType;
	Func* cppNew;
	Func* raiseCompileError;
	Func* defineFunction;
}

extern "C"
{
	BuiltinType* getPointerToBuiltinType(BuiltinType* type)
	{
		return type->Ptr();
	}

	BaseType* createFuncType(const char* name, bool isVarArg, BaseType* resultType, BaseType** argTypes, int numArgTypes)
	{
		std::vector<BuiltinType*> builtinArgTypes;
		for (int i = 0; i < numArgTypes; ++i)
			builtinArgTypes.push_back((BuiltinType*)argTypes[i]);
		return new FuncType(name, (BuiltinType*)resultType, builtinArgTypes, isVarArg);
	}

	Func* defineFunction(BlockExprAST* scope, const char* name, BuiltinType* resultType, BuiltinType** argTypes, size_t numArgTypes, bool isVarArg)
	{
		Func* func = new Func(name, resultType, std::vector<BuiltinType*>(argTypes, argTypes + numArgTypes), isVarArg);
		defineSymbol(scope, func->type.name, &func->type, func);
		defineCast2(scope, &func->type, BuiltinTypes::LLVMValueRef,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

				Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
				Value* resultVal = builder->CreateCall(func, { funcVal });
				return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
			}
		);
	}
}

Value* createStringConstant(StringRef str, const Twine& name)
{
	Constant* val = ConstantDataArray::getString(*context, str);
	GlobalVariable* filenameGlobal = new GlobalVariable(
		*currentFunc->getParent(), val->getType(), true, GlobalValue::PrivateLinkage,
		val, name, nullptr, GlobalVariable::NotThreadLocal, 0
	);
	filenameGlobal->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
	filenameGlobal->setAlignment(1);
	return builder->CreateInBoundsGEP(val->getType(), filenameGlobal, { ConstantInt::getNullValue(Types::Int64), ConstantInt::getNullValue(Types::Int64) });
}

std::string mangle(const std::string& className, const std::string& funcName, BuiltinType* returnType, std::vector<BuiltinType*>& argTypes)
{
	const std::map<BuiltinType*, const char*> TYPE_CODES = {
		{ BuiltinTypes::Void, "v" },
	};

	std::stringstream mangled;
	mangled << "_Z";
	mangled << className.size() << className;
	if (funcName == className) mangled << "C2"; // Constructor
	mangled << "E";
	mangled << TYPE_CODES.at(returnType);
	return mangled.str();
}

void defineType(BlockExprAST* scope, const char* name, BuiltinType* metaType, BuiltinType* type)
{
	defineType(name, type);
	defineSymbol(scope, name, metaType, new XXXValue(unwrap(metaType->llvmtype), (uint64_t)type));
}

Variable lookupVariable(const BlockExprAST* parentBlock, const IdExprAST* id)
{
	bool isCaptured;
	const Variable* var = lookupSymbol(parentBlock, getIdExprASTName(id), isCaptured);
	if (var == nullptr)
		raiseCompileError(("`" + std::string(getIdExprASTName(id)) + "` was not declared in this scope").c_str(), (ExprAST*)id);
auto foo = getIdExprASTName(id);
	XXXValue* varVal = (XXXValue*)var->value;
	if (!varVal) raiseCompileError(("invalid use of type `" + std::string(getIdExprASTName(id)) + "` as expression").c_str(), (ExprAST*)id);
	if (varVal->isFunction() || varVal->isConstant() )//|| dynamic_cast<ClassType* const>((BuiltinType* const)var->type))
		return *var;

	if (isCaptured //)
&& closure) //DELETE
	{
		if (!closure) assert(0);

		capturedScope.push_back({getIdExprASTName(id), *var});

Type** capturedTypes = new Type*[capturedScope.size()];
int i = 0;
for (auto&& [name, var]: capturedScope)
{
capturedTypes[i++] = unwrap(((BuiltinType*)var.type)->llvmtype);
}
StructType* closureType = (StructType*)closure->getType()->getPointerElementType();
closureType->setBody(ArrayRef<Type*>(capturedTypes, capturedScope.size()));

		// expr = closure[idxVal]
		Value* gep = builder->CreateInBoundsGEP(closure, {
			Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(64, 0, true)),
			Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(32, capturedScope.size() - 1, true))
		});
		LoadInst* exprVal = builder->CreateLoad(gep);
		exprVal->setAlignment(8);
		varVal = new XXXValue(exprVal);

//		parentBlock->addToScope(id->name, var->type, varVal); //TODO: Check if closures still work without this
	}

	return Variable(var->type, varVal);
}

void defineReturnStmt(BlockExprAST* scope, const BaseType* returnType, const char* funcName = nullptr)
{
	if (returnType == BuiltinTypes::Void)
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				BaseType* returnType = getType(params[0], parentBlock);
				if (funcName)
					raiseCompileError(("void function '" + std::string(funcName) + "' should not return a value").c_str(), params[0]);
				else
					raiseCompileError("void function should not return a value", params[0]);
			},
			(void*)funcName
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				builder->CreateRetVoid();
			}
		);
	}
	else
	{
		// Define return statement with incorrect type in function scope
		defineStmt2(scope, "return $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* returnType = getType(params[0], parentBlock);
				raiseCompileError(("invalid return type `" + getTypeName(returnType) + "`").c_str(), params[0]);
			}
		);

		// Define return statement with correct type in function scope
		defineStmt2(scope, ("return $E<" + getTypeName(returnType) + ">").c_str(),
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				builder->CreateRet(((XXXValue*)codegenExpr(params[0], parentBlock).value)->val);
			}
		);

		// Define return statement without type in function scope
		defineStmt2(scope, "return",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				const char* funcName = (const char*)stmtArgs;
				if (funcName)
					raiseCompileError(("non-void function '" + std::string(funcName) + "' should return a value").c_str(), params[0]);
				else
					raiseCompileError("non-void function should return a value", params[0]);
			},
			(void*)funcName
		);
	}
}

Func* defineFunction(BlockExprAST* scope, BlockExprAST* funcBlock, const char* funcName, BuiltinType* returnType, BuiltinType* classType, std::vector<BuiltinType*>& argTypes, std::vector<IdExprAST*>& argNames, bool isVarArg)
{
	Func* func = new Func(funcName, returnType, argTypes, false);
	Function* parentFunc = currentFunc;
	currentFunc = func->getFunction(currentModule);
	currentFunc->setDSOLocal(true);

	size_t numArgs = argTypes.size();
	assert(argNames.size() == numArgs);

	// Create entry BB in currentFunc
	BasicBlock *parentBB = currentBB;
	builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", currentFunc));

	if (dbuilder)
	{
		DIScope *FContext = dfile;
		DISubprogram *subprogram = dbuilder->createFunction(
			parentFunc->getSubprogram(), funcName, funcName, dfile, getExprLine((ExprAST*)funcBlock),
			dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray(std::vector<Metadata*>(numArgs, intType))), //TODO: Replace {} with array of argument types
			getExprLine((ExprAST*)funcBlock), DINode::FlagPrototyped, DISubprogram::SPFlagDefinition)
		;
		currentFunc->setSubprogram(subprogram);
		builder->SetCurrentDebugLocation(DebugLoc());
	}

	// Define argument symbols in function scope
	for (size_t i = 0; i < numArgs; ++i)
	{
		AllocaInst* arg = builder->CreateAlloca(unwrap(argTypes[i]->llvmtype), nullptr, argNames[i] ? getIdExprASTName(argNames[i]) : "");
		arg->setAlignment(argTypes[i]->align);
		builder->CreateStore(currentFunc->args().begin() + i, arg)->setAlignment(argTypes[i]->align);
		defineSymbol(funcBlock, argNames[i] ? getIdExprASTName(argNames[i]) : "", argTypes[i], new XXXValue(arg));
	}

	if (dbuilder)
	{
		for (size_t i = 0; i < numArgs; ++i)
			if (argNames[i] != nullptr)
			{
				DILocalVariable *D = dbuilder->createParameterVariable(
					currentFunc->getSubprogram(),
					getIdExprASTName(argNames[i]),
					i + 1,
					dfile,
					getExprLine((ExprAST*)argNames[i]),
					intType, //TODO: Replace with argTypes[i].ditype
					true
				);
				dbuilder->insertDeclare(
					currentFunc->args().begin() + i, D, dbuilder->createExpression(),
					DebugLoc::get(getExprLine((ExprAST*)argNames[i]), getExprColumn((ExprAST*)argNames[i]), currentFunc->getSubprogram()),
					builder->GetInsertBlock()
				);
			}
	}

	defineReturnStmt(funcBlock, returnType, funcName);

	if (classType != nullptr)
	{
		AllocaInst* thisPtr = builder->CreateAlloca(unwrap(classType->Ptr()->llvmtype), nullptr, "this");
		thisPtr->setAlignment(8);
		builder->CreateStore(currentFunc->args().begin(), thisPtr)->setAlignment(8);
		defineSymbol(funcBlock, "this", classType->Ptr(), new XXXValue(thisPtr));
	}

	// Codegen function body
	codegenExpr((ExprAST*)funcBlock, scope).value;

	if (currentFunc->getReturnType()->isVoidTy()) // If currentFunc is void function
		builder->CreateRetVoid(); // Add implicit `return;`

	// Close function
	if (dbuilder)
	{
		dbuilder->finalizeSubprogram(currentFunc->getSubprogram());
		builder->SetCurrentDebugLocation(DebugLoc());
	}
	builder->SetInsertPoint(currentBB = parentBB);

	std::string errstr;
	raw_string_ostream errstream(errstr);
	bool haserr = verifyFunction(*currentFunc, &errstream);

//	currentFunc = parentFunc;

	if (haserr && errstr[0] != '\0')
	{
		std::error_code ec;
		raw_fd_ostream ostream("error.ll", ec);
		currentModule->print(ostream, nullptr);
		ostream.close();
//				raiseCompileError(("error compiling module\n" + errstr).c_str(), (ExprAST*)scope);
verifyFunction(*currentFunc, &outs());
	}

currentFunc = parentFunc;

	return func;
}

void initBuiltinSymbols()
{
	// >>> Create builtin types

	BuiltinTypes::Base = BuiltinType::get("BaseType", wrap(Types::BaseType->getPointerTo()), 8);
	BuiltinTypes::Builtin = BuiltinType::get("BuiltinType", wrap(Types::BuiltinType), 8);
	BuiltinTypes::BuiltinValue = BuiltinType::get("BuiltinValue", nullptr, 0);
	BuiltinTypes::BuiltinClass = BuiltinType::get("BuiltinClass", nullptr, 0);
	BuiltinTypes::BuiltinInstance = BuiltinType::get("BuiltinInstance", nullptr, 0);
	BuiltinTypes::Value = BuiltinType::get("Value", wrap(Types::Value->getPointerTo()), 8);

	// Primitive types
	BuiltinTypes::Void = (BuiltinType*)getVoidType();
	BuiltinTypes::VoidPtr = BuiltinTypes::Void->Ptr();
	BuiltinTypes::Int1 = BuiltinType::get("bool", wrap(Types::Int1), 1);
	BuiltinTypes::Int1Ptr =BuiltinTypes::Int1->Ptr();
	BuiltinTypes::Int8 = BuiltinType::get("char", wrap(Types::Int8), 1);
	BuiltinTypes::Int8Ptr = BuiltinType::get("string", wrap(Types::Int8Ptr), 8);
	BuiltinTypes::Int16 = BuiltinType::get("short", wrap(Types::Int16), 2);
	BuiltinTypes::Int16Ptr = BuiltinTypes::Int16->Ptr();
	BuiltinTypes::Int32 = BuiltinType::get("int", LLVMInt32Type(), 4);
	BuiltinTypes::Int32Ptr = BuiltinTypes::Int32->Ptr();
	BuiltinTypes::Int64 = BuiltinType::get("long", wrap(Types::Int64), 8);
	BuiltinTypes::Int64Ptr = BuiltinTypes::Int64->Ptr();
	BuiltinTypes::Half = BuiltinType::get("half", LLVMHalfType(), 2);
	BuiltinTypes::HalfPtr = BuiltinTypes::Half->Ptr();
	BuiltinTypes::Float = BuiltinType::get("float", LLVMFloatType(), 4);
	BuiltinTypes::FloatPtr = BuiltinTypes::Float->Ptr();
	BuiltinTypes::Double = BuiltinType::get("double", LLVMDoubleType(), 8);
	BuiltinTypes::DoublePtr = BuiltinTypes::Double->Ptr();

	// LLVM types
	BuiltinTypes::LLVMAttributeRef = BuiltinType::get("LLVMAttributeRef", wrap(Types::LLVMOpaqueAttributeRef->getPointerTo()), 8);
	BuiltinTypes::LLVMBasicBlockRef = BuiltinType::get("LLVMBasicBlockRef", wrap(Types::LLVMOpaqueBasicBlock->getPointerTo()), 8);
	BuiltinTypes::LLVMBuilderRef = BuiltinType::get("LLVMBuilderRef", wrap(Types::LLVMOpaqueBuilder->getPointerTo()), 8);
	BuiltinTypes::LLVMContextRef = BuiltinType::get("LLVMContextRef", wrap(Types::LLVMOpaqueContext->getPointerTo()), 8);
	BuiltinTypes::LLVMDiagnosticInfoRef = BuiltinType::get("LLVMDiagnosticInfoRef", wrap(Types::LLVMOpaqueDiagnosticInfo->getPointerTo()), 8);
	BuiltinTypes::LLVMDIBuilderRef = BuiltinType::get("LLVMDIBuilderRef", wrap(Types::LLVMOpaqueDIBuilder->getPointerTo()), 8);
	BuiltinTypes::LLVMMemoryBufferRef = BuiltinType::get("LLVMMemoryBufferRef", wrap(Types::LLVMOpaqueMemoryBuffer->getPointerTo()), 8);
	BuiltinTypes::LLVMMetadataRef = BuiltinType::get("LLVMMetadataRef", wrap(Types::LLVMOpaqueMetadata->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleRef = BuiltinType::get("LLVMModuleRef", wrap(Types::LLVMOpaqueModule->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleFlagEntryRef = BuiltinType::get("LLVMModuleFlagEntryRef", wrap(Types::LLVMOpaqueModuleFlagEntry->getPointerTo()), 8);
	BuiltinTypes::LLVMModuleProviderRef = BuiltinType::get("LLVMModuleProviderRef", wrap(Types::LLVMOpaqueModuleProvider->getPointerTo()), 8);
	BuiltinTypes::LLVMNamedMDNodeRef = BuiltinType::get("LLVMNamedMDNodeRef", wrap(Types::LLVMOpaqueNamedMDNode->getPointerTo()), 8);
	BuiltinTypes::LLVMPassManagerRef = BuiltinType::get("LLVMPassManagerRef", wrap(Types::LLVMOpaquePassManager->getPointerTo()), 8);
	BuiltinTypes::LLVMPassRegistryRef = BuiltinType::get("LLVMPassRegistryRef", wrap(Types::LLVMOpaquePassRegistry->getPointerTo()), 8);
	BuiltinTypes::LLVMTypeRef = BuiltinType::get("LLVMTypeRef", wrap(Types::LLVMOpaqueType->getPointerTo()), 8);
	BuiltinTypes::LLVMUseRef = BuiltinType::get("LLVMUseRef", wrap(Types::LLVMOpaqueUse->getPointerTo()), 8);
	BuiltinTypes::LLVMValueRef = BuiltinType::get("LLVMValueRef", wrap(Types::LLVMOpaqueValue->getPointerTo()), 8);
	BuiltinTypes::LLVMValueMetadataEntryRef = BuiltinType::get("LLVMValueRef", wrap(Types::LLVMOpaqueValueMetadataEntry->getPointerTo()), 8);

	// AST types
	BuiltinTypes::ExprAST = BuiltinType::get("ExprAST", wrap(Types::ExprAST->getPointerTo()), 8);
	BuiltinTypes::ExprListAST = BuiltinType::get("ExprListAST", wrap(Types::ExprListAST->getPointerTo()), 8);
	BuiltinTypes::LiteralExprAST = BuiltinType::get("LiteralExprAST", wrap(Types::LiteralExprAST->getPointerTo()), 8);
	BuiltinTypes::IdExprAST = BuiltinType::get("IdExprAST", wrap(Types::IdExprAST->getPointerTo()), 8);
	BuiltinTypes::CastExprAST = BuiltinType::get("CastExprAST", wrap(Types::CastExprAST->getPointerTo()), 8);
	BuiltinTypes::BlockExprAST = BuiltinType::get("BlockExprAST", wrap(Types::BlockExprAST->getPointerTo()), 8);
	BuiltinTypes::StmtAST = BuiltinType::get("StmtAST", wrap(Types::StmtAST->getPointerTo()), 8);

	// >>> Create LLVM-c extern functions

	create_llvm_c_functions(*context, llvm_c_functions);

	// >>> Create Minc extern functions

	MincFunctions::getExprListASTExpression = new Func("getExprListASTExpression", BuiltinTypes::ExprAST, { BuiltinTypes::ExprListAST, BuiltinTypes::Int64 }, false);
	MincFunctions::getExprListASTSize = new Func("getExprListASTSize", BuiltinTypes::Int64, { BuiltinTypes::ExprListAST }, false);
	MincFunctions::getIdExprASTName = new Func("getIdExprASTName", BuiltinTypes::Int8Ptr, { BuiltinTypes::IdExprAST }, false);
	MincFunctions::getLiteralExprASTValue = new Func("getLiteralExprASTValue", BuiltinTypes::Int8Ptr, { BuiltinTypes::LiteralExprAST }, false);
	MincFunctions::getBlockExprASTParent = new Func("getBlockExprASTParent", BuiltinTypes::BlockExprAST, { BuiltinTypes::BlockExprAST }, false);
	MincFunctions::setBlockExprASTParent = new Func("setBlockExprASTParent", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::getCastExprASTSource = new Func("getCastExprASTSource", BuiltinTypes::ExprAST, { BuiltinTypes::CastExprAST }, false);
	MincFunctions::getExprLine = new Func("getExprLine", BuiltinTypes::Int32, { BuiltinTypes::ExprAST }, false);
	MincFunctions::getExprColumn = new Func("getExprColumn", BuiltinTypes::Int32, { BuiltinTypes::ExprAST }, false);
	MincFunctions::getPointerToBuiltinType = new Func("getPointerToBuiltinType", BuiltinTypes::Builtin, { BuiltinTypes::Builtin }, false);

	MincFunctions::addToScope = new Func("AddToScope", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
	MincFunctions::addToFileScope = new Func("AddToFileScope", BuiltinTypes::Void, { BuiltinTypes::IdExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
	MincFunctions::lookupScope = new Func("LookupScope", BuiltinTypes::LLVMValueRef, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST }, false);
	MincFunctions::codegenExprValue = new Func("codegenExprValue", BuiltinTypes::LLVMValueRef, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::codegenExprConstant = new Func("codegenExprConstant", BuiltinTypes::LLVMValueRef, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::codegenStmt = new Func("codegenStmt", BuiltinTypes::Void, { BuiltinTypes::StmtAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::getType = new Func("getType", BuiltinTypes::Base, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
	//MincFunctions::defineStmt = new Func("DefineStatement", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::ExprAST->Ptr(), BuiltinTypes::Int32, TODO, BuiltinTypes::Int8Ptr }, false);
	MincFunctions::getValueFunction = new Func("getValueFunction", BuiltinTypes::LLVMValueRef, { BuiltinTypes::Value }, false);
	MincFunctions::createFuncType = new Func("createFuncType", BuiltinTypes::Base, { BuiltinTypes::Int8Ptr, BuiltinTypes::Int8, BuiltinTypes::Base, BuiltinTypes::Base->Ptr(), BuiltinTypes::Int32 }, false);
	MincFunctions::cppNew = new Func("_Znwm", BuiltinTypes::Int8Ptr, { BuiltinTypes::Int64 }, false); //TODO: Replace hardcoded "_Znwm" with mangled "new"
	MincFunctions::raiseCompileError = new Func("raiseCompileError", BuiltinTypes::Void, { BuiltinTypes::Int8Ptr, BuiltinTypes::ExprAST }, false);
	MincFunctions::defineFunction = new Func("defineFunction", BuiltinTypes::Builtin, { BuiltinTypes::BlockExprAST, BuiltinTypes::Int8Ptr, BuiltinTypes::Builtin, BuiltinTypes::Builtin->Ptr(), BuiltinTypes::Int64, BuiltinTypes::Int1 }, false);
}

void defineBuiltinSymbols(BlockExprAST* rootBlock)
{
	// Define LLVM-c extern functions
	for (Func& func: llvm_c_functions)
		defineSymbol(rootBlock, func.type.name, &func.type, &func);

	// Define Minc extern functions
	for (Func* func: {
		MincFunctions::getExprListASTSize,
		MincFunctions::getIdExprASTName,
		MincFunctions::getLiteralExprASTValue,
		MincFunctions::getBlockExprASTParent,
		MincFunctions::setBlockExprASTParent,
		MincFunctions::getPointerToBuiltinType,
		MincFunctions::getExprLine,
		MincFunctions::getExprColumn,
		MincFunctions::lookupScope,
		MincFunctions::defineFunction,
	})
	{
		defineSymbol(rootBlock, func->type.name, &func->type, func);
		defineCast2(rootBlock, &func->type, BuiltinTypes::LLVMValueRef,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

				Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
				Value* resultVal = builder->CreateCall(func, { funcVal });
				return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
			}
		);
	}

	BaseType* baseType = getBaseType();
	defineType("BaseType", baseType);
	defineSymbol(rootBlock, "BaseType", baseType, new XXXValue(Types::BaseType, (uint64_t)baseType));

	defineType(rootBlock, "BuiltinType", BuiltinTypes::Builtin, BuiltinTypes::Builtin);
	defineType(rootBlock, "BuiltinTypePtr", BuiltinTypes::Builtin, BuiltinTypes::Builtin->Ptr());

	defineType(rootBlock, "void", BuiltinTypes::Builtin, BuiltinTypes::Void);
	defineType(rootBlock, "bool", BuiltinTypes::Builtin, BuiltinTypes::Int1);
	defineType(rootBlock, "char", BuiltinTypes::Builtin, BuiltinTypes::Int8);
	defineType(rootBlock, "int", BuiltinTypes::Builtin, BuiltinTypes::Int32);
	defineType(rootBlock, "long", BuiltinTypes::Builtin, BuiltinTypes::Int64);
	defineType(rootBlock, "double", BuiltinTypes::Builtin, BuiltinTypes::Double);
	defineType(rootBlock, "doublePtr", BuiltinTypes::Builtin, BuiltinTypes::DoublePtr);
	defineType(rootBlock, "string", BuiltinTypes::Builtin, BuiltinTypes::Int8Ptr);
	defineType(rootBlock, "ExprAST", BuiltinTypes::Builtin, BuiltinTypes::ExprAST);
	defineType(rootBlock, "ExprASTPtr", BuiltinTypes::Builtin, BuiltinTypes::ExprAST->Ptr());
	defineType(rootBlock, "ExprListAST", BuiltinTypes::Builtin, BuiltinTypes::ExprListAST);
	defineType(rootBlock, "LiteralExprAST", BuiltinTypes::Builtin, BuiltinTypes::LiteralExprAST);
	defineType(rootBlock, "IdExprAST", BuiltinTypes::Builtin, BuiltinTypes::IdExprAST);
	defineType(rootBlock, "CastExprAST", BuiltinTypes::Builtin, BuiltinTypes::CastExprAST);
	defineType(rootBlock, "BlockExprAST", BuiltinTypes::Builtin, BuiltinTypes::BlockExprAST);
	defineType(rootBlock, "LLVMValueRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMValueRef);
	defineType(rootBlock, "LLVMValueRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMValueRef->Ptr());
	defineType(rootBlock, "LLVMBasicBlockRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMBasicBlockRef);
	defineType(rootBlock, "LLVMTypeRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMTypeRef);
	defineType(rootBlock, "LLVMTypeRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMTypeRef->Ptr());
	defineType(rootBlock, "LLVMMetadataRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMMetadataRef);
	defineType(rootBlock, "LLVMMetadataRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMMetadataRef->Ptr());

	defineType(rootBlock, "BuiltinValue", BuiltinTypes::BuiltinValue, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int1, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int8, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int32, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int64, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Double, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::DoublePtr, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int8Ptr, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::ExprAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::ExprListAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LiteralExprAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::IdExprAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::CastExprAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::BlockExprAST, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMValueRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMValueRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMBasicBlockRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMTypeRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMTypeRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMMetadataRef, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::LLVMMetadataRef, BuiltinTypes::BuiltinValue);

	defineType(rootBlock, "BuiltinClass", BuiltinTypes::Builtin, BuiltinTypes::BuiltinClass);
	defineOpaqueCast(rootBlock, BuiltinTypes::BuiltinClass, BuiltinTypes::Builtin);
	defineType(rootBlock, "BuiltinInstance", BuiltinTypes::Builtin, BuiltinTypes::BuiltinInstance);
//	defineType(rootBlock, "BuiltinInstancePtr", BuiltinTypes::Builtin, BuiltinTypes::BuiltinInstance->Ptr());

defineSymbol(rootBlock, "intType", BuiltinTypes::LLVMMetadataRef, new XXXValue(Types::LLVMOpaqueMetadata->getPointerTo(), (uint64_t)intType));

	// Define single-expr statement
	defineStmt2(rootBlock, "$E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define context-free block statement
	defineStmt2(rootBlock, "$B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			codegenExpr(params[0], parentBlock);
		}
	);

	// Define function call
	defineExpr3(rootBlock, "$I($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* funcAST = params[0];
			Variable funcVar = codegenExpr(funcAST, parentBlock);
			FuncType* funcType = (FuncType*)funcVar.type;
			if (funcType == nullptr)
				raiseCompileError(("function `" + std::string(getIdExprASTName((IdExprAST*)funcAST)) + "` was not declared in this scope").c_str(), funcAST);
			Function* func = ((XXXValue*)funcVar.value)->getFunction(currentModule);
			if (func == nullptr)
				raiseCompileError(("`" + std::string(getIdExprASTName((IdExprAST*)funcAST)) + "` is not a function").c_str(), funcAST);
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);
			const size_t numArgs = argExprs.size();

			if ((func->isVarArg() && func->arg_size() > numArgs) ||
				(!func->isVarArg() && func->arg_size() != numArgs))
				raiseCompileError("invalid number of function arguments", funcAST);
			
			for (size_t i = 0; i < func->arg_size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = funcType->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
//printf("implicit cast from %s to %s in %s:%i\n", getTypeName(gotType).c_str(), getTypeName(expectedType).c_str(), getExprFilename(argExpr), getExprLine(argExpr));
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						raiseCompileError(
							("invalid function argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExpr
						);
					}
					argExprs[i] = castExpr;
				}
			}

			std::vector<Value*> argValues;
			for (ExprAST* argExpr: argExprs)
				argValues.push_back(((XXXValue*)codegenExpr(argExpr, parentBlock).value)->val);
	
			return Variable(funcType->resultType, new XXXValue(builder->CreateCall(func, argValues)));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			FuncType* funcType = (FuncType*)getType(params[0], parentBlock);
			return funcType == nullptr ? nullptr : funcType->resultType;
		}
	);

	// Define `import`
	defineStmt2(rootBlock, "import $L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			LiteralExprAST* pathAST = (LiteralExprAST*)params[0];
			std::string path(getLiteralExprASTValue(pathAST));
			path = path.substr(1, path.length() - 2);

			importModule(parentBlock, path.c_str(), (ExprAST*)pathAST);
		}
	);
	defineStmt2(rootBlock, "import <$I.$I>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* filenameAST = (IdExprAST*)params[0];
			IdExprAST* fileextAST = (IdExprAST*)params[1];

			importModule(parentBlock, ("../lib/" + std::string(getIdExprASTName(filenameAST)) + '.' + std::string(getIdExprASTName(fileextAST))).c_str(), (ExprAST*)filenameAST);
		}
	);

	// Define `stmtdef`
	defineStmt2(rootBlock, "stmtdef $E ... $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) { //TODO: Consider switching to const params
			std::vector<ExprAST*>& stmtParamsAST = getExprListASTExpressions((ExprListAST*)params[0]);
			BlockExprAST* blockAST = (BlockExprAST*)params[1];

			std::vector<ExprAST*> stmtParams;
			for (ExprAST* stmtParam: stmtParamsAST)
				collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

			// Generate JIT function name
			std::string jitFuncName = "";
			for (ExprAST* stmtParam: stmtParamsAST)
			{
				if (stmtParam != stmtParamsAST.front())
					jitFuncName += ' ';
				jitFuncName += ExprASTIsBlock(stmtParam) ? std::string("{}") : ExprASTToString(stmtParam);
			}

#ifdef DEBUG_PARAMETER_COLLECTION
			printf("\033[34m%s:\033[0m", jitFuncName.c_str());
			for (ExprAST* stmtParam: stmtParams)
				printf(" %s", ExprASTToString(stmtParam).c_str());
			printf("\n");
#endif

			defineReturnStmt(blockAST, BuiltinTypes::Void);

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::Void, stmtParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineStmt(parentBlock, stmtParamsAST, jitFunc);
			removeJitFunction(jitFunc);
		}
	);

// 	// Define `stmtdef2` //TODO: Should be `$B.stmtdef ... { ... }`
// 	defineStmt2(rootBlock, "stmtdef2 $E $E ... $B",
// 		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
// 			Value* targetBlockVal = builder->CreateBitCast(codegenExpr(params.front(), parentBlock).value->val, Types::BlockExprAST->getPointerTo());
// 			BlockExprAST* blockAST = (BlockExprAST*)params.back();
// 			params = std::vector<ExprAST*>(params.begin() + 1, params.end() - 1);

// 			std::vector<ExprAST*> stmtParams;
// 			for (int i = 0; i < params.size(); ++i)
// 				collectParams(parentBlock, params[i], params[i], stmtParams);

// 			// Generate JIT function name
// 			std::string jitFuncName = "";
// 			for (auto param: params)
// 			{
// 				if (param != params.front())
// 					jitFuncName += ' ';
// 				jitFuncName += ExprASTIsBlock(param) ? std::string("{}") : ExprASTToString(param);
// 			}

//			defineReturnStmt(blockAST, BuiltinTypes::Void);

// 			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::Void, stmtParams, jitFuncName);
// 			capturedScope.clear();
// 			codegenExpr((ExprAST*)blockAST, parentBlock);

// ExprAST** paramsCopy = new ExprAST*[params.size()];
// memcpy(paramsCopy, params.data(), params.size() * sizeof(ExprAST*));
// 			Value* paramsVal = Constant::getIntegerValue(Types::ExprAST->getPointerTo()->getPointerTo(), APInt(64, (uint64_t)paramsCopy, true));
// 			Value* numParamsVal = ConstantInt::get(*context, APInt(32, params.size()));
// 			Value* jitFuncVal = Constant::getIntegerValue(TODO, APInt(64, (uint64_t)jitFunc, true));
// 			Value* closure = builder->CreateAlloca(jitFunc.closureType, nullptr, "closure");
// 			removeJitFunction(jitFunc);
// 			int i = 0;
// 			for (auto&& [name, var]: capturedScope)
// 			{
// 				Value* gep = builder->CreateInBoundsGEP(closure, {
// 					Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(64, 0, true)),
// 					Constant::getIntegerValue(IntegerType::getInt32Ty(*context), APInt(32, i++, true))
// 				});
// 				builder->CreateStore(var.value->val, gep)->setAlignment(8);
// 			}
// 			closure = builder->CreateBitCast(closure, Type::getInt8PtrTy(*context));
// 			Function* defineStatementFunc = MincFunctions::defineStmt->getFunction(currentModule);
// 			builder->CreateCall(defineStatementFunc, { targetBlockVal, paramsVal, numParamsVal, jitFuncVal, closure });
// 		}
// 	);

	// Define `exprdef`
	defineStmt2(rootBlock, "exprdef<$I> $E $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BaseType* exprType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			//TODO: Check for errors
			ExprAST* exprAST = params[1];
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprAST, exprAST, exprParams);

			// Generate JIT function name
			std::string jitFuncName = ExprASTIsBlock(exprAST) ? std::string("{}") : ExprASTToString(exprAST);

			defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, exprParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineExpr(parentBlock, exprAST, jitFunc, exprType);
			removeJitFunction(jitFunc);
		}
	);

	// Define `castdef`
	defineStmt2(rootBlock, "castdef<$I> $E $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			//TODO: Check for errors
			BaseType* fromType = getType(params[1], parentBlock);
			//TODO: Check for errors
			BlockExprAST* blockAST = (BlockExprAST*)params[2];

			std::vector<ExprAST*> castParams(1, params[1]);

			// Generate JIT function name
			std::string jitFuncName("cast " + getTypeName(fromType) + " -> " + getTypeName(toType));

			defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, castParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineCast(parentBlock, fromType, toType, jitFunc);
			removeJitFunction(jitFunc);
		}
	);

	// Define `typedef`
	defineStmt2(rootBlock, "typedef<$I> $I $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BuiltinType* metaType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			IdExprAST* nameAST = (IdExprAST*)params[1];
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			// Generate JIT function name
			std::string jitFuncName("typedef " + std::string(getIdExprASTName(nameAST)));

			defineReturnStmt(blockAST, metaType);

			std::vector<ExprAST*> typeParams;
			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, metaType, typeParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			typedef BuiltinType* (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params);
			funcPtr jitFunctionPtr = reinterpret_cast<funcPtr>(compileJitFunction(jitFunc));
			BuiltinType* type = jitFunctionPtr(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, {});
			removeJitFunctionModule(jitFunc);
			removeJitFunction(jitFunc);

			defineType(parentBlock, getIdExprASTName(nameAST), metaType, type);
		}
	);

	// Define variable declaration
	defineStmt2(rootBlock, "int_ref $I",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* varAST = (IdExprAST*)params[0];

			Type* type = IntegerType::getInt32Ty(*context);
			AllocaInst* var = builder->CreateAlloca(type, nullptr, getIdExprASTName(varAST));
			var->setAlignment(4);

			if (dbuilder)
			{
				DILocalVariable *D = dbuilder->createAutoVariable(currentFunc->getSubprogram(), getIdExprASTName(varAST), dfile, getExprLine((ExprAST*)varAST), intType, true);
				dbuilder->insertDeclare(
					var, D, dbuilder->createExpression(),
					DebugLoc::get(getExprLine((ExprAST*)varAST), getExprColumn((ExprAST*)varAST), currentFunc->getSubprogram()),
					builder->GetInsertBlock()
				);
			}

			defineSymbol(parentBlock, getIdExprASTName(varAST), BuiltinTypes::Int32, new XXXValue(var));
		}
	);

	// Define variable declaration with initialization
	defineStmt2(rootBlock, "int_ref $I = $E<int>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* varAST = (IdExprAST*)params[0];
			ExprAST* valAST = (ExprAST*)params[1];

			Type* type = IntegerType::getInt32Ty(*context);
			AllocaInst* var = builder->CreateAlloca(type, nullptr, getIdExprASTName(varAST));
			var->setAlignment(4);

			if (dbuilder)
			{
				DILocalVariable *D = dbuilder->createAutoVariable(currentFunc->getSubprogram(), getIdExprASTName(varAST), dfile, getExprLine((ExprAST*)varAST), intType, true);
				dbuilder->insertDeclare(
					var, D, dbuilder->createExpression(),
					DebugLoc::get(getExprLine((ExprAST*)varAST), getExprColumn((ExprAST*)varAST), currentFunc->getSubprogram()),
					builder->GetInsertBlock()
				);
			}

			XXXValue* val = (XXXValue*)codegenExpr(valAST, parentBlock).value;
			builder->CreateStore(val->val, var)->setAlignment(4);

			defineSymbol(parentBlock, getIdExprASTName(varAST), BuiltinTypes::Int32, new XXXValue(var));
		}
	);

	// Define `auto` variable declaration with initialization //TODO: not working
	defineStmt2(rootBlock, "auto $I = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* varAST = (IdExprAST*)params[0];
			ExprAST* valAST = (ExprAST*)params[1];
			XXXValue* val = (XXXValue*)codegenExpr(valAST, parentBlock).value;

			val->val->setName(getIdExprASTName(varAST));
			defineSymbol(parentBlock, getIdExprASTName(varAST), nullptr, val);
		}
	);

	// Define function declaration
	defineStmt2(rootBlock, "$E<BuiltinType> $I($E<BuiltinType> $I, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);

			std::vector<BuiltinType*> argTypes; argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				argTypes.push_back((BuiltinType*)codegenExpr(argTypeExpr, parentBlock).value->getConstantValue());
			
			Func* func = new Func(funcName, returnType, argTypes, false);

			// Define function symbol in parent scope
			defineSymbol(parentBlock, funcName, &func->type, func);
			defineCast2(parentBlock, &func->type, BuiltinTypes::LLVMValueRef,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
					Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

					Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
					Value* resultVal = builder->CreateCall(func, { funcVal });
					return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
				}
			);
		}
	);
	defineStmt2(rootBlock, "$E<BuiltinType> $I($E<BuiltinType> $I, ..., $V)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);

			std::vector<BuiltinType*> argTypes; argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				argTypes.push_back((BuiltinType*)codegenExpr(argTypeExpr, parentBlock).value->getConstantValue());
			
			Func* func = new Func(funcName, returnType, argTypes, true);

			// Define function symbol in parent scope
			defineSymbol(parentBlock, funcName, &func->type, func);
			defineCast2(parentBlock, &func->type, BuiltinTypes::LLVMValueRef,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
					Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

					Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
					Value* resultVal = builder->CreateCall(func, { funcVal });
					return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
				}
			);
		}
	);
	defineStmt2(rootBlock, "$E<BuiltinType> $I($E<BuiltinType>, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[2]);

			const size_t numArgs = argExprs.size();
			std::vector<BuiltinType*> argTypes; argTypes.reserve(numArgs);
			for (ExprAST* argExpr: argExprs)
				argTypes.push_back((BuiltinType*)codegenExpr(argExpr, parentBlock).value->getConstantValue());

			// Handle $E<BuiltinType> $I(void)
			if (numArgs == 1 && argTypes[0] == BuiltinTypes::Void)
				argTypes.pop_back();
			
			Func* func = new Func(funcName, returnType, argTypes, false);

			// Define function symbol in parent scope
			defineSymbol(parentBlock, funcName, &func->type, func);
			defineCast2(parentBlock, &func->type, BuiltinTypes::LLVMValueRef,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
					Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

					Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
					Value* resultVal = builder->CreateCall(func, { funcVal });
					return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
				}
			);
		}
	);

	// Define function definition
	defineStmt2(rootBlock, "$E<BuiltinType> $I($E<BuiltinType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			std::vector<BuiltinType*> argTypes; argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				argTypes.push_back((BuiltinType*)codegenExpr(argTypeExpr, parentBlock).value->getConstantValue());

			std::vector<IdExprAST*> argNames; argTypes.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				argNames.push_back((IdExprAST*)argNameExpr);
			
			Func* func = defineFunction(parentBlock, blockAST, funcName, returnType, nullptr, argTypes, argNames, false);

			// Define function symbol in parent scope
			defineSymbol(parentBlock, funcName, &func->type, func);
		}
	);

	// Define struct definition
	defineStmt2(rootBlock, "struct $I $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* nameAST = (IdExprAST*)params.front();
			const char* name = getIdExprASTName(nameAST);
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			// Define struct type in parent scope
			ClassType* structType = new ClassType();
			defineType(parentBlock, getIdExprASTName(nameAST), BuiltinTypes::BuiltinClass, structType);
			defineOpaqueCast(parentBlock, structType, BuiltinTypes::BuiltinInstance);

			// Define struct variable definition
			defineStmt2(blockAST, "public $E<BuiltinType> $I",
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					ClassType* structType = (ClassType*)stmtArgs;
					BuiltinType* memberType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
					const char* memberName = getIdExprASTName((IdExprAST*)params[1]);
					unsigned int memberIndex = structType->variables.size();
					structType->variables[memberName] = ClassVariable{PUBLIC, memberType, memberIndex};
				},
				structType
			);

			// Define struct constructor
			defineStmt2(blockAST, ("public " + std::string(name) + "($E<BuiltinType> $I, ...) $B").c_str(),
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					ClassType* structType = (ClassType*)stmtArgs;
					std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[0]);
					std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[1]);
					BlockExprAST* blockAST = (BlockExprAST*)params.back();

					std::vector<BuiltinType*> argTypes; argTypes.reserve(argTypeExprs.size());
					for (ExprAST* argTypeExpr: argTypeExprs)
						argTypes.push_back((BuiltinType*)codegenExpr(argTypeExpr, parentBlock).value->getConstantValue());

					std::vector<IdExprAST*> argNames; argTypes.reserve(argNameExprs.size());
					for (ExprAST* argNameExpr: argNameExprs)
						argNames.push_back((IdExprAST*)argNameExpr);

					// Mangle function name
					const std::string& structName = getTypeName(structType);
					std::string funcName = mangle(structName, structName, BuiltinTypes::Void, argTypes);
					char* _funcName = new char[funcName.size() + 1];
					strcpy(_funcName, funcName.c_str());

					Func* func = defineFunction(parentBlock, blockAST, _funcName, BuiltinTypes::Void, structType, argTypes, argNames, false);

					structType->constructors.push_back(ClassMethod{PUBLIC, func});
				},
				structType
			);

			// Codegen struct body
			codegenExpr((ExprAST*)blockAST, parentBlock);

			// Define default constructor if no custom constructors are defined
			if (structType->constructors.empty())
			{
				std::vector<BuiltinType*> argTypes;
				argTypes.push_back(structType->Ptr());

				// Mangle function name
				std::string funcName = mangle(name, name, BuiltinTypes::Void, argTypes);
				char* _funcName = new char[funcName.size() + 1];
				strcpy(_funcName, funcName.c_str());
				
				Func* func = new Func(_funcName, BuiltinTypes::Void, argTypes, false, "");
				structType->constructors.push_back(ClassMethod{PUBLIC, func});
			}

			// Prepend "struct." to structType->llvmtype
			const size_t nameLen = strlen(name);
			char* structLLVMTypeName = new char[nameLen + 8];
			strcpy(structLLVMTypeName, "struct.");
			strcpy(structLLVMTypeName + 7, name);
			((StructType*)unwrap(structType->llvmtype))->setName(structLLVMTypeName);

			// Set structType->llvmtype body to array of llvmtype's of structType->variables
			std::vector<llvm::Type*> memberLlvmTypes;
			memberLlvmTypes.resize(structType->variables.size());
			for (std::pair<std::string, ClassVariable> variable: structType->variables)
				memberLlvmTypes[variable.second.index] = unwrap(variable.second.type->llvmtype);
			((StructType*)unwrap(structType->llvmtype))->setBody(memberLlvmTypes);
		}
	);

	// Define struct member getter
	defineExpr3(rootBlock, "$E<BuiltinInstance>.$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			Variable structVar = codegenExpr(structAST, parentBlock);
			ClassType* structType = (ClassType*)structVar.type;
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = structType->variables.find(memberName);
			if (variable == structType->variables.end())
			{
				if (structType->methods.find(memberName) != structType->methods.end())
					raiseCompileError(("method '" + memberName + "' in '" + getTypeName(structType) + "' must be called").c_str(), params[1]);
				raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(structType) + "'").c_str(), params[1]);
			}

			Value* gep = builder->CreateStructGEP(((XXXValue*)structVar.value)->val, variable->second.index, "gep");
			LoadInst* memberVal = builder->CreateLoad(gep);
			memberVal->setAlignment(variable->second.type->align);
			return Variable(variable->second.type, new XXXValue(memberVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			ClassType* structType = (ClassType*)getType(structAST, parentBlock);
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = structType->variables.find(memberName);
			return variable == structType->variables.end() ? nullptr : variable->second.type;
		}
	);

	// Define struct member setter
	defineExpr3(rootBlock, "$E<BuiltinInstance>.$I = $E<BuiltinValue>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			Variable structVar = codegenExpr(structAST, parentBlock);
			ClassType* structType = (ClassType*)structVar.type;
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = structType->variables.find(memberName);
			if (variable == structType->variables.end())
			{
				if (structType->methods.find(memberName) != structType->methods.end())
					raiseCompileError(("method '" + memberName + "' in '" + getTypeName(structType) + "' must be called").c_str(), params[1]);
				raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(structType) + "'").c_str(), params[1]);
			}

			ExprAST* varExpr = params[2];
			if (ExprASTIsCast(varExpr))
				varExpr = getCastExprASTSource((CastExprAST*)varExpr);
			BaseType *memberType = variable->second.type, *valueType = getType(varExpr, parentBlock);
			if (memberType != valueType)
			{
				ExprAST* castExpr = lookupCast(parentBlock, varExpr, memberType);
				if (castExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, varExpr);
					raiseCompileError(
						("cannot assign <" + getTypeName(valueType) + "> to <" + getTypeName(memberType) + ">\n" + candidateReport).c_str(),
						varExpr
					);
				}
				varExpr = castExpr;
			}
			XXXValue* valVal = (XXXValue*)codegenExpr(varExpr, parentBlock).value;

			Value* gep = builder->CreateStructGEP(((XXXValue*)structVar.value)->val, variable->second.index, "gep");
			builder->CreateStore(valVal->val, gep)->setAlignment(variable->second.type->align);
			LoadInst* memberVal = builder->CreateLoad(gep);
			memberVal->setAlignment(variable->second.type->align);
			return Variable(variable->second.type, new XXXValue(memberVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			ClassType* structType = (ClassType*)getType(structAST, parentBlock);
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = structType->variables.find(memberName);
			return variable == structType->variables.end() ? nullptr : variable->second.type;
		}
	);

	// Define new struct
	defineExpr3(rootBlock, "new $E<BuiltinClass>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ClassType* structType = (ClassType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);
			const size_t numArgs = argExprs.size();

			for (ClassMethod& constructor: structType->constructors)
			{
				FuncType& constructorType = constructor.func->type;
				if (constructorType.argTypes.size() - 1 != numArgs)
					continue;

				for (size_t i = 0; i < numArgs; ++i)
				{
					ExprAST* argExpr = argExprs[i];
					BaseType *expectedType = constructorType.argTypes[i + 1], *gotType = getType(argExpr, parentBlock);

					if (expectedType != gotType)
					{
						ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
						if (castExpr == nullptr)
						{
							//TODO: Continue instead of throwing errors!
							std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
							raiseCompileError(
								("invalid constructor argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
								argExpr
							);
						}
						argExprs[i] = castExpr;
					}
				}

				// Allocate memory using C++ new operator
				Value* const structSize = ConstantInt::get(*context, APInt(64, DataLayout(currentModule).getTypeAllocSize(unwrap(structType->llvmtype)), true));
				Function* func = MincFunctions::cppNew->getFunction(currentModule);
				Value* valMem = builder->CreateCall(func, { (Value*)structSize });
				Value* val = builder->CreateBitCast(valMem, unwrap(structType->Ptr()->llvmtype));

				if (constructor.func->symName[0] == '\0') // If constructor is default constructor
				{
					// Memset struct body to zero
					builder->CreateMemSet(valMem, Constant::getNullValue(Types::Int8), structSize, 8);
				}
				else
				{
					// Call constructor
					std::vector<Value*> argValues(1, val);
					for (ExprAST* argExpr: argExprs)
						argValues.push_back(((XXXValue*)codegenExpr(argExpr, parentBlock).value)->val);
					val = builder->CreateCall(constructor.func->getFunction(currentModule), argValues);
				}
		
				return Variable(structType->Ptr(), new XXXValue(val));
			}
			raiseCompileError("TODO: no constructor found that takes numArgs arguments", params[0]);

			// auto member = structType->members.find(memberName);
			// if (member == structType->members.end())
			// 	raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(structType) + "'").c_str(), params[1]);

			// assert(0); //TODO: Not implemented below this
			// Value* gep = builder->CreateStructGEP(structVar.value->val, member->second.index, "gep");
			// LoadInst* memberVal = builder->CreateLoad(gep);
			// memberVal->setAlignment(member->second.type->align);
			// return Variable(member->second.type, new XXXValue(memberVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return ((ClassType*)codegenExpr(params[0], const_cast<BlockExprAST*>(parentBlock)).value->getConstantValue())->Ptr();
		}
	);

	/*// Define struct method call
	defineExpr3(rootBlock, "$E<BuiltinInstance>.$I($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			Variable structVar = codegenExpr(structAST, parentBlock);
			ClassType* structType = (ClassType*)structVar.type;
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto member = structType->members.find(memberName);
			if (member == structType->members.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(structType) + "'").c_str(), params[1]);

			assert(0); //TODO: Not implemented below this
			Value* gep = builder->CreateStructGEP(structVar.value->val, member->second.index, "gep");
			LoadInst* memberVal = builder->CreateLoad(gep);
			memberVal->setAlignment(member->second.type->align);
			return Variable(member->second.type, new XXXValue(memberVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			ExprAST* structAST = params[0];
			if (ExprASTIsCast(structAST))
				structAST = getCastExprASTSource((CastExprAST*)structAST);
			ClassType* structType = (ClassType*)getType(structAST, parentBlock);
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto member = structType->members.find(memberName);
			return member == structType->members.end() || !member->second.isMethod() ? nullptr : ((FuncType*)member->second.type)->resultType;
		}
	);*/

	// Define codegen($E<ExprAST>, $E<BlockExprAST>)
	defineExpr2(rootBlock, "codegen($E<ExprAST>, $E<BlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			XXXValue* exprVal = (XXXValue*)codegenExpr(params[0], parentBlock).value;
			XXXValue* parentBlockVal = (XXXValue*)codegenExpr(params[1], parentBlock).value;

			Function* func = MincFunctions::codegenExprValue->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(func, { exprVal->val, parentBlockVal->val });
			return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
		},
		BuiltinTypes::LLVMValueRef
	);
	// Define codegen() with invalid parameters
	defineExpr2(rootBlock, "codegen($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[0]);
			if (argExprs.size() != 2)
				raiseCompileError("invalid number of function arguments", params[0]);

			BaseType* const expectedTypes[] = { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST };
			for (size_t i = 0; i < 2; ++i)
			{
				BaseType *expectedType = expectedTypes[i], *gotType = getType(argExprs[i], parentBlock);
				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExprs[i], expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExprs[i]);
						raiseCompileError(
							("invalid function argument type: " + ExprASTToString(argExprs[i]) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExprs[i]
						);
					}
					argExprs[i] = castExpr;
				}
			}
		},
		BuiltinTypes::LLVMValueRef
	);

	// Define getconst($E<ExprAST>, $E<BlockExprAST>)
	defineExpr3(rootBlock, "getconst($E<ExprAST>, $E<BlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			assert(ExprASTIsCast(params[0]));
			ExprAST* exprAST = getCastExprASTSource((CastExprAST*)params[0]);
			Variable expr = codegenExpr(exprAST, parentBlock);
			const TpltType* exprType = dynamic_cast<const TpltType*>((const BuiltinType*)expr.type);
			assert(exprType != 0); //TODO: Non-template builtin types don't resolve to 0!
			Value* exprVal = ((XXXValue*)expr.value)->val;
			Value* parentBlockVal = ((XXXValue*)codegenExpr(params[1], parentBlock).value)->val;

			Function* func = MincFunctions::codegenExprConstant->getFunction(currentModule);
			Value* int64ConstantVal = builder->CreateCall(func, { exprVal, parentBlockVal });
			Value* resultVal = builder->CreateBitCast(int64ConstantVal, unwrap(((BuiltinType*)exprType->tpltType)->llvmtype));
			return Variable(exprType->tpltType, new XXXValue(resultVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			ExprAST* exprAST = getCastExprASTSource((CastExprAST*)params[0]);
			const TpltType* exprType = dynamic_cast<const TpltType*>((const BuiltinType*)getType(exprAST, parentBlock));
			assert(exprType != 0); //TODO: Non-template builtin types don't resolve to 0!
			return exprType->tpltType;
		}
	);

	// Define gettype($E<ExprAST>, $E<BlockExprAST>)
	defineExpr3(rootBlock, "gettype($E<ExprAST>, $E<BlockExprAST>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* exprVal = ((XXXValue*)codegenExpr(params[0], parentBlock).value)->val;
			Value* parentBlockVal = ((XXXValue*)codegenExpr(params[1], parentBlock).value)->val;

			exprVal = builder->CreateBitCast(exprVal, Types::CastExprAST->getPointerTo()); // exprVal = (CastExprAST*)exprVal
			Function* func = MincFunctions::getCastExprASTSource->getFunction(currentModule);
			exprVal = builder->CreateCall(func, { exprVal }); // exprVal = getCastExprASTSource(exprVal)

			func = MincFunctions::getType->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(func, { exprVal, parentBlockVal }); // resultVal = getCastExprASTSource(exprVal, parentBlockVal)
			resultVal = builder->CreateBitCast(resultVal, Types::BuiltinType); // resultVal = (BuiltinType*)resultVal //TODO: Return BaseType instead of BuiltinType
			return Variable(BuiltinTypes::/*Base*/Builtin, new XXXValue(resultVal)); //TODO: Return BaseType instead of BuiltinType
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return BuiltinTypes::/*Base*/Builtin; //TODO: Return BaseType instead of BuiltinType
		}
	);

	// Define addToScope()
	defineExpr2(rootBlock, "addToScope($E<BlockExprAST>, $E<IdExprAST>, $E, $E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* parentBlockVal = ((XXXValue*)codegenExpr(params[0], parentBlock).value)->val;
			Value* nameVal; //= ((XXXValue*)codegenExpr(params[1], parentBlock).value)->val;
			Value* typeVal = ((XXXValue*)codegenExpr(params[2], parentBlock).value)->val;
			Value* valVal = ((XXXValue*)codegenExpr(params[3], parentBlock).value)->val;

			if (ExprASTIsParam(params[1]))
				nameVal = ((XXXValue*)codegenExpr(params[1], parentBlock).value)->val;
			else if (ExprASTIsId(params[1]))
				nameVal = builder->CreateBitCast(ConstantInt::get(*context, APInt(64, (uint64_t)params[1], true)), Types::ExprAST->getPointerTo());
			else
				assert(0);

			typeVal = builder->CreateBitCast(typeVal, Types::BaseType->getPointerTo());

			/*IdExprAST* typeAST = (IdExprAST*)params[2];
			const Variable* typeVar = parentBlock->lookupScope(getIdExprASTName(typeAST));
			if (!typeVar)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` was not declared in this scope").c_str(), (ExprAST*)typeAST);
			if (typeVar->value)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` is not a type").c_str(), (ExprAST*)typeAST);
			Value* typeVal = Constant::getIntegerValue(Type::getInt8PtrTy(*context), APInt(64, (uint64_t)typeVar->type, true));*/

			Function* addToScopeFunc = MincFunctions::addToScope->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(addToScopeFunc, { parentBlockVal, nameVal, typeVal, valVal });
			return Variable(nullptr, new XXXValue(Constant::getNullValue(Type::getVoidTy(*context)->getPointerTo())));
		},
		nullptr
	);

	// Define gettype()
	defineExpr2(rootBlock, "gettype($E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			BaseType* type = getType(params[0], parentBlock);
auto foo = getTypeName(type);
printf("gettype(%s) == %s\n", ExprASTToString(params[0]).c_str(), getTypeName(type).c_str());
			//return Variable(nullptr, new XXXValue(Constant::getIntegerValue(Types::BaseType->getPointerTo(), APInt(64, (uint64_t)type))));
return Variable(BuiltinTypes::Builtin, new XXXValue(Constant::getIntegerValue(Types::BuiltinType, APInt(64, (uint64_t)type))));
		},
		BuiltinTypes::Builtin
	);

	// Define symdef
	defineStmt2(rootBlock, "symdef<$E> $E = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			IdExprAST* typeAST = (IdExprAST*)params[0];
			bool isCaptured;
			const Variable* typeVar = lookupSymbol(parentBlock, getIdExprASTName(typeAST), isCaptured);
			if (!typeVar)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` was not declared in this scope").c_str(), (ExprAST*)typeAST);
			if (typeVar->value)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` is not a type").c_str(), (ExprAST*)typeAST);
			Value* typeVal = Constant::getIntegerValue(Type::getInt8PtrTy(*context), APInt(64, (uint64_t)typeVar->type, true));

			Value* nameVal;
			if (ExprASTIsParam(params[1]))
				nameVal = ((XXXValue*)codegenExpr(params[1], parentBlock).value)->val;
			else if (ExprASTIsId(params[1]))
				nameVal = builder->CreateBitCast(Constant::getIntegerValue(Types::IdExprAST->getPointerTo(), APInt(64, (uint64_t)params[1], true)), Types::ExprAST->getPointerTo());
			else
				assert(0);

			Value* valVal = ((XXXValue*)codegenExpr(params[2], parentBlock).value)->val;

			Function* addToScopeFunc = MincFunctions::addToFileScope->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(addToScopeFunc, { nameVal, typeVal, valVal });
		}
	);

	// Define symdef
	defineStmt2(rootBlock, "throw $E<string>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			Value* msgVal = ((XXXValue*)codegenExpr(params[0], parentBlock).value)->val;

			// Allocate expr = ExprAST*
			Value* locExprVal = builder->CreateAlloca(Types::ExprAST);

			// Set expr->loc.filename
			builder->CreateStore(
				createStringConstant(getExprFilename(params[0]), ""),
				builder->CreateInBoundsGEP(locExprVal, {
					ConstantInt::get(Types::Int64, 0),
					ConstantInt::get(Types::Int32, 1),
					ConstantInt::get(Types::Int32, 0),
				}, "gep")
			)->setAlignment(8);

			// Set expr->loc.begin_line
			builder->CreateStore(
				ConstantInt::get(Types::Int32, getExprLine(params[0])),
				builder->CreateInBoundsGEP(locExprVal, {
					ConstantInt::get(Types::Int64, 0),
					ConstantInt::get(Types::Int32, 1),
					ConstantInt::get(Types::Int32, 1),
				}, "gep")
			)->setAlignment(8);

			// Set expr->loc.begin_col
			builder->CreateStore(
				ConstantInt::get(Types::Int32, getExprColumn(params[0])),
				builder->CreateInBoundsGEP(locExprVal, {
					ConstantInt::get(Types::Int64, 0),
					ConstantInt::get(Types::Int32, 1),
					ConstantInt::get(Types::Int32, 2),
				}, "gep")
			)->setAlignment(8);

			// Set expr->loc.end_line
			builder->CreateStore(
				ConstantInt::get(Types::Int32, getExprEndLine(params[0])),
				builder->CreateInBoundsGEP(locExprVal, {
					ConstantInt::get(Types::Int64, 0),
					ConstantInt::get(Types::Int32, 1),
					ConstantInt::get(Types::Int32, 3),
				}, "gep")
			)->setAlignment(8);

			// Set expr->loc.end_col
			builder->CreateStore(
				ConstantInt::get(Types::Int32, getExprEndColumn(params[0])),
				builder->CreateInBoundsGEP(locExprVal, {
					ConstantInt::get(Types::Int64, 0),
					ConstantInt::get(Types::Int32, 1),
					ConstantInt::get(Types::Int32, 4),
				}, "gep")
			)->setAlignment(8);

			Function* raiseCompileErrorFunc = MincFunctions::raiseCompileError->getFunction(currentModule);
			builder->CreateCall(raiseCompileErrorFunc, { msgVal, locExprVal });
		}
	);

	/*// Define subscript
	defineExpr2(rootBlock, "$E[$E]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* var = codegenExpr(params[0], parentBlock).value->val;
			Value* idx = codegenExpr(params[1], parentBlock).value->val;

			Value* gep = builder->CreateInBoundsGEP(var, {
				idx
			}, "gep");
			LoadInst* val = builder->CreateLoad(gep);
			val->setAlignment(8);
			return Variable(nullptr, new XXXValue(val));
		},
		nullptr
	);*/

	/*// Define subscript variable assignment
	defineExpr3(rootBlock, "$E[$E] = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* var = codegenExpr(params[0], parentBlock).value->val;
			Value* idx = codegenExpr(params[1], parentBlock).value->val;

			Value* gep = builder->CreateInBoundsGEP(var, {
				idx
			}, "gep");

			Variable expr = codegenExpr(params[2], parentBlock);
			builder->CreateStore(expr.value->val, gep);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return getType(params[2], parentBlock);
		}
	);*/

	// Define $E<LiteralExprAST>.value_ref
	defineExpr2(rootBlock, "$E<LiteralExprAST>.value_ref",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* varVal = ((XXXValue*)codegenExpr(params[0], parentBlock).value)->val;
			varVal = builder->CreateBitCast(varVal, Types::LiteralExprAST->getPointerTo());

			Function* getLiteralExprASTValueFunc = currentModule->getFunction("getLiteralExprASTValue");
			return Variable(BuiltinTypes::Int8Ptr, new XXXValue(builder->CreateCall(getLiteralExprASTValueFunc, { varVal })));
		},
		BuiltinTypes::Int8Ptr
	);

	// Define variable lookup
	defineExpr3(rootBlock, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable var = lookupVariable(parentBlock, (IdExprAST*)params[0]);
			if (((XXXValue*)var.value)->isFunction() || ((XXXValue*)var.value)->isConstant() )//|| dynamic_cast<ClassType* const>((BuiltinType* const)var.type))
				return var;

			LoadInst* loadVal = builder->CreateLoad(((XXXValue*)var.value)->val, getIdExprASTName((IdExprAST*)params[0]));
			loadVal->setAlignment(4);
			return Variable(var.type, new XXXValue(loadVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(rootBlock, "$L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);

			if (value[0] == '"' || value[0] == '\'')
				return Variable(BuiltinTypes::Int8Ptr, new XXXValue(createStringConstant(StringRef(value + 1, strlen(value) - 2), "MY_CONSTANT")));

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return Variable(BuiltinTypes::Double, new XXXValue(unwrap(LLVMConstReal(LLVMDoubleType(), doubleValue))));
			}

			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);

			return Variable(BuiltinTypes::Int32, new XXXValue(unwrap(LLVMConstInt(LLVMInt32Type(), intValue, 1))));
			//return Variable(Type::getInt32Ty(*context), new XXXValue(ConstantInt::get(*context, APInt(32, value, true))));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			if (value[0] == '"' || value[0] == '\'')
				return BuiltinTypes::Int8Ptr;
			if (strchr(value, '.'))
				return BuiltinTypes::Double;
			return BuiltinTypes::Int32;
		}
	);

	// Define variable assignment
	defineExpr3(rootBlock, "$I = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable var = lookupVariable(parentBlock, (IdExprAST*)params[0]);
			Variable expr = codegenExpr(params[1], parentBlock);

			builder->CreateStore(((XXXValue*)expr.value)->val, ((XXXValue*)var.value)->val);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return getType(params[1], parentBlock);
		}
	);

	defineExpr2(rootBlock, "FuncType($E<string>, $E<int>, $E<BaseType>, $E<BaseType>, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* nameVal = ((XXXValue*)codegenExpr(params[0], parentBlock).value)->val;
			Value* isVarArgVal = builder->CreateBitCast(((XXXValue*)codegenExpr(params[1], parentBlock).value)->val, Types::Int8);
			Value* resultTypeVal = ((XXXValue*)codegenExpr(params[2], parentBlock).value)->val;
			std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[3]);

			const size_t numArgTypes = argTypeExprs.size();
			AllocaInst* argTypes = builder->CreateAlloca(ArrayType::get(Types::BaseType->getPointerTo(), numArgTypes), nullptr, "argTypes");
			argTypes->setAlignment(8);
			for (size_t i = 0; i < numArgTypes; ++i)
				builder->CreateStore(((XXXValue*)codegenExpr(argTypeExprs[i], parentBlock).value)->val, builder->CreateConstInBoundsGEP2_64(argTypes, 0, i));
			Value* numArgTypesVal = Constant::getIntegerValue(Types::Int32, APInt(32, numArgTypes, true));
			Value* argTypesVal = builder->CreateConstInBoundsGEP2_64(argTypes, 0, 0);

			Function* func = MincFunctions::createFuncType->getFunction(currentModule);
			return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(builder->CreateCall(func, { nameVal, isVarArgVal, resultTypeVal, argTypesVal, numArgTypesVal })));
		},
		BuiltinTypes::LLVMValueRef
	);

	/*defineExpr2(rootBlock, "$E<BaseType>.llvmtype",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Value* typeVal = codegenExpr(params[0], parentBlock).value->val;

			//TODO
		},
		BuiltinTypes::LLVMTypeRef
	);*/

defineExpr2(rootBlock, "getfunc($E)",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		Variable expr = codegenExpr(params[0], parentBlock);
		LLVMValueRef func = wrap(((XXXValue*)expr.value)->getFunction(currentModule));

		return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(Constant::getIntegerValue(Types::LLVMOpaqueValue->getPointerTo(), APInt(64, (uint64_t)func))));
	},
	BuiltinTypes::LLVMValueRef
);

	defineExpr3(rootBlock, "$E<ExprListAST>[$E<long>]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable listVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			TpltType* listType = dynamic_cast<TpltType*>((BuiltinType*)listVar.type);
			Variable idxVar = codegenExpr(params[1], parentBlock);

			Function* getExprListASTSizeFunc = MincFunctions::getExprListASTSize->getFunction(currentModule);
			Value* listSizeVal = builder->CreateCall(getExprListASTSizeFunc, { ((XXXValue*)listVar.value)->val });

			Value* ifCondVal = builder->CreateICmpUGE(((XXXValue*)idxVar.value)->val, listSizeVal);

			BasicBlock *ifBlock = BasicBlock::Create(*context, "", currentFunc);
			BasicBlock *elseBlock = BasicBlock::Create(*context, "", currentFunc);
			builder->CreateCondBr(ifCondVal, ifBlock, elseBlock);

			builder->SetInsertPoint(currentBB = ifBlock);
			Function* raiseCompileErrorFunc = MincFunctions::raiseCompileError->getFunction(currentModule);
			builder->CreateCall(raiseCompileErrorFunc, { createStringConstant("list index out of range", ""), builder->CreateBitCast(((XXXValue*)listVar.value)->val, Types::ExprAST->getPointerTo()) });
			builder->CreateBr(elseBlock);

			builder->SetInsertPoint(currentBB = elseBlock);
			Function* getExprListASTExprFunc = MincFunctions::getExprListASTExpression->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(getExprListASTExprFunc, { ((XXXValue*)listVar.value)->val, ((XXXValue*)idxVar.value)->val });
			return Variable(listType->tpltType, new XXXValue(resultVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			ExprAST* arrAST = getCastExprASTSource((CastExprAST*)params[0]);
			TpltType* listType = dynamic_cast<TpltType*>((BuiltinType*)getType(arrAST, parentBlock));
			assert(listType);
			return listType->tpltType;
		}
	);
}