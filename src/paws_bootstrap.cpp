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

#include "llvm_constants.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

extern llvm::LLVMContext* context;
extern llvm::IRBuilder<>* builder;
extern llvm::Module* currentModule;
extern llvm::Function* currentFunc;
extern llvm::BasicBlock* currentBB;
extern llvm::DIBuilder* dbuilder;
extern llvm::DIFile* dfile;
extern llvm::Value* closure;

std::list<Func> llvm_c_functions;
std::list<std::pair<std::string, const Variable>> capturedScope;
BlockExprAST *defScope, *pawsDefScope;

namespace BuiltinScopes
{
	BaseScopeType* File = nullptr;
	BaseScopeType* Function = new BaseScopeType();
	BaseScopeType* JitFunction = new BaseScopeType();
};

namespace MincFunctions
{
	Func* resolveExprAST;
	Func* getExprListASTExpression;
	Func* getExprListASTSize;
	Func* getIdExprASTName;
	Func* getLiteralExprASTValue;
	Func* getBlockExprASTParent;
	Func* setBlockExprASTParent;
	Func* getCastExprASTSource;
	Func* getExprLoc;
	Func* getExprLine;
	Func* getExprColumn;
	Func* getTypeName;
	Func* getPointerToBuiltinType;

	Func* lookupCast;
	Func* addToScope;
	Func* addToFileScope;
	Func* lookupSymbol;
	Func* lookupScopeType;
	Func* importSymbol;
	Func* codegenExprValue;
	Func* codegenExprConstant;
	Func* codegenStmt;
	Func* getType;
	//Func* defineStmt;
	Func* getValueFunction;
	Func* cppNew;
	Func* raiseCompileError;
}

extern "C"
{
	BuiltinType* getPointerToBuiltinType(BuiltinType* type)
	{
		return type->Ptr();
	}

	void AddToScope(BlockExprAST* targetBlock, IdExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		defineSymbol(targetBlock, getIdExprASTName(nameAST), type, new XXXValue(unwrap(val)));
	}

	void AddToFileScope(IdExprAST* nameAST, BaseType* type, LLVMValueRef val)
	{
		defineSymbol(getFileScope(), getIdExprASTName(nameAST), type, new XXXValue(unwrap(val)));
	}

	LLVMValueRef LookupScope(BlockExprAST* scope, IdExprAST* nameAST)
	{
		const Variable* var = lookupSymbol(scope, getIdExprASTName(nameAST));
		return var == nullptr ? nullptr : wrap(((XXXValue*)var->value)->val);
	}
	BaseType* LookupScopeType(BlockExprAST* scope, IdExprAST* nameAST)
	{
		const Variable* var = lookupSymbol(scope, getIdExprASTName(nameAST));
		return var == nullptr ? nullptr : var->type;
	}
	LLVMValueRef ImportSymbol(BlockExprAST* scope, IdExprAST* nameAST)
	{
		if (ExprASTIsCast((ExprAST*)nameAST))
			nameAST = (IdExprAST*)getDerivedExprAST((ExprAST*)nameAST);
		const Variable* var = importSymbol(scope, getIdExprASTName(nameAST));
		return var == nullptr ? nullptr : wrap(((XXXValue*)var->value)->val);
	}

	LLVMValueRef codegenExprValue(ExprAST* expr, BlockExprAST* scope)
	{
		XXXValue* value = (XXXValue*)codegenExpr(expr, scope).value;
		return wrap(value == nullptr ? Constant::getNullValue(Type::getVoidTy(*context)) : value->val);
	}

	Value* getValueFunction(Func* value)
	{
		return value->getFunction(currentModule);
	}

	Func* defineFunction(BlockExprAST* scope, const char* name, BuiltinType* resultType, BuiltinType** argTypes, size_t numArgTypes, bool isVarArg)
	{
		Func* func = new Func(name, resultType, std::vector<BuiltinType*>(argTypes, argTypes + numArgTypes), isVarArg);
		defineSymbol(scope, name, func->type, func);
		defineOpaqueInheritanceCast(scope, func->type, BuiltinTypes::BuiltinFunction);
		defineTypeCast2(defScope, func->type, BuiltinTypes::LLVMValueRef,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				Value* funcVal = Constant::getIntegerValue(Types::Func->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

				Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
				Value* resultVal = builder->CreateCall(func, { funcVal });
				return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
			}
		);
		defineTypeCast2(pawsDefScope, func->type, PawsType<LLVMValueRef>::TYPE,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				Variable funcVar = codegenExpr(params[0], parentBlock);
				FuncType* funcType = (FuncType*)funcVar.type;
				Function* func = ((Func*)funcVar.value)->getFunction(currentModule);
				return Variable(PawsType<LLVMValueRef>::TYPE, new PawsType<LLVMValueRef>(wrap(func)));
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
	// Define type symbol in scope
	defineSymbol(scope, name, metaType, new XXXValue(unwrap(metaType->llvmtype), (uint64_t)type));

	// Make type a sub-class of metaType
	defineOpaqueInheritanceCast(scope, type, metaType);
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
					raiseCompileError(("non-void function '" + std::string(funcName) + "' should return a value").c_str(), (ExprAST*)parentBlock);
				else
					raiseCompileError("non-void function should return a value", (ExprAST*)parentBlock);
			},
			(void*)funcName
		);
	}
}

void VarDeclStmt(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs)
{
	BuiltinType* type = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
	const char* typeName = getTypeName(type).c_str();
	LLVMValueRef storage = LLVMBuildAlloca(wrap(builder), type->llvmtype, "");
	LLVMSetAlignment(storage, type->align);
	defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[1]), type, new XXXValue(unwrap(storage)));

	if (dfile && type->encoding != 0)
	{
		LLVMDIBuilderInsertDeclareAtEnd(
			wrap(dbuilder),
			storage,
			LLVMDIBuilderCreateAutoVariable(
				wrap(dbuilder),
				LLVMGetSubprogram(wrap(currentFunc)),
				getIdExprASTName((IdExprAST*)params[1]),
				strlen(getIdExprASTName((IdExprAST*)params[1])),
				wrap(dfile),
				getExprLine(params[1]),
				LLVMDIBuilderCreateBasicType(wrap(dbuilder), typeName, strlen(typeName), type->numbits, type->encoding, LLVMDIFlagZero),
				1,
				LLVMDIFlagZero,
				0
			),
			LLVMDIBuilderCreateExpression(wrap(dbuilder), nullptr, 0),
			LLVMDIBuilderCreateDebugLocation(wrap(context), getExprLine(params[1]), getExprColumn(params[1]), LLVMGetSubprogram(wrap(currentFunc)), nullptr),
			LLVMGetInsertBlock(wrap(builder))
		);
	}
}

void InitializedVarDeclStmt(BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs)
{
	BuiltinType* varType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
	const char* varTypeName = getTypeName(varType).c_str();
	assert(ExprASTIsCast(params[2]));
	ExprAST* expr = getDerivedExprAST(params[2]);
	BuiltinType* exprType = (BuiltinType*)getType(expr, parentBlock);
	if (varType != exprType)
	{
		expr = lookupCast(parentBlock, expr, varType);
		if (!expr)
		{
			char* err = (char*)malloc(256);
			strcpy(err, "cannot convert <");
			strcat(err, getTypeName(exprType).c_str());
			strcat(err, "> to <");
			strcat(err, varTypeName);
			strcat(err, "> in initialization");
			raiseCompileError(err, params[0]);
		}
	}

	LLVMValueRef storage = LLVMBuildAlloca(wrap(builder), varType->llvmtype, "");
	LLVMSetAlignment(storage, varType->align);
	LLVMBuildStore(wrap(builder), wrap(((XXXValue*)codegenExpr(expr, parentBlock).value)->val), storage);
	defineSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[1]), varType, new XXXValue(unwrap(storage)));

	if (dfile && varType->encoding != 0)
	{
		LLVMDIBuilderInsertDeclareAtEnd(
			wrap(dbuilder),
			storage,
			LLVMDIBuilderCreateAutoVariable(
				wrap(dbuilder),
				LLVMGetSubprogram(wrap(currentFunc)),
				getIdExprASTName((IdExprAST*)params[1]),
				strlen(getIdExprASTName((IdExprAST*)params[1])),
				wrap(dfile),
				getExprLine(params[1]),
				LLVMDIBuilderCreateBasicType(wrap(dbuilder), varTypeName, strlen(varTypeName), varType->numbits, varType->encoding, LLVMDIFlagZero),
				1,
				LLVMDIFlagZero,
				0
			),
			LLVMDIBuilderCreateExpression(wrap(dbuilder), nullptr, 0),
			LLVMDIBuilderCreateDebugLocation(wrap(context), getExprLine(params[1]), getExprColumn(params[1]), LLVMGetSubprogram(wrap(currentFunc)), nullptr),
			LLVMGetInsertBlock(wrap(builder))
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
	std::vector<DIBasicType*> argDTypes;

	setScopeType(funcBlock, BuiltinScopes::Function);

	// Create entry BB in currentFunc
	BasicBlock *parentBB = currentBB;
	builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", currentFunc));

	if (dbuilder)
	{
		argDTypes.reserve(numArgs);
		for (BuiltinType* argType: argTypes)
			argDTypes.push_back(dbuilder->createBasicType(getTypeName(argType), argType->numbits, argType->encoding));

		DIScope *FContext = dfile;
		DISubprogram *subprogram = dbuilder->createFunction(
			parentFunc->getSubprogram(), funcName, funcName, dfile, getExprLine((ExprAST*)funcBlock),
			dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray(std::vector<Metadata*>(argDTypes.begin(), argDTypes.end()))),
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
					argDTypes[i],
					false
				);
				dbuilder->insertDeclare(
					currentFunc->args().begin() + i, D, dbuilder->createExpression(),
					DebugLoc::get(getExprLine((ExprAST*)argNames[i]), getExprColumn((ExprAST*)argNames[i]), currentFunc->getSubprogram()),
					builder->GetInsertBlock()
				);
			}
	}

	defineReturnStmt(funcBlock, returnType, funcName);
	defineStmt2(funcBlock, "$E<BuiltinType> $I", VarDeclStmt);
	defineStmt2(funcBlock, "$E<BuiltinType> $I = $E<BuiltinValue>", InitializedVarDeclStmt);

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

struct PawsBootstrapCodegenContext : public PawsCodegenContext
{
public:
	PawsBootstrapCodegenContext(BlockExprAST* expr, BaseType* type, const std::vector<Variable>& blockParams)
		: PawsCodegenContext(expr, type, blockParams) {}
	Variable codegen(BlockExprAST* parentBlock, std::vector<ExprAST*>& params)
	{
		// Wrap result of PawsCodegenContext::codegen() into an XXXValue
		Value* llvmValue = unwrap(((PawsType<LLVMValueRef>*)PawsCodegenContext::codegen(parentBlock, params).value)->get());
		return Variable(type, new XXXValue(llvmValue));
	}
};

struct PawsBootstrap : public PawsPackage
{
	PawsBootstrap() : PawsPackage("bootstrap") {}

private:
	void define(BlockExprAST* rootBlock)
	{
		// >>> Create Minc extern functions

		MincFunctions::resolveExprAST = new Func("resolveExprAST", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::ExprAST }, false);
		MincFunctions::getExprListASTExpression = new Func("getExprListASTExpression", BuiltinTypes::ExprAST, { BuiltinTypes::ExprListAST, BuiltinTypes::Int64 }, false);
		MincFunctions::getExprListASTSize = new Func("getExprListASTSize", BuiltinTypes::Int64, { BuiltinTypes::ExprListAST }, false);
		MincFunctions::getIdExprASTName = new Func("getIdExprASTName", BuiltinTypes::Int8Ptr, { BuiltinTypes::IdExprAST }, false);
		MincFunctions::getLiteralExprASTValue = new Func("getLiteralExprASTValue", BuiltinTypes::Int8Ptr, { BuiltinTypes::LiteralExprAST }, false);
		MincFunctions::getBlockExprASTParent = new Func("getBlockExprASTParent", BuiltinTypes::BlockExprAST, { BuiltinTypes::BlockExprAST }, false);
		MincFunctions::setBlockExprASTParent = new Func("setBlockExprASTParent", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::BlockExprAST }, false);
		MincFunctions::getCastExprASTSource = new Func("getCastExprASTSource", BuiltinTypes::ExprAST, { BuiltinTypes::CastExprAST }, false);
		MincFunctions::getExprLoc = new Func("getExprLoc", BuiltinTypes::Location, { BuiltinTypes::ExprAST }, false);
		MincFunctions::getExprLine = new Func("getExprLine", BuiltinTypes::Int32, { BuiltinTypes::ExprAST }, false);
		MincFunctions::getExprColumn = new Func("getExprColumn", BuiltinTypes::Int32, { BuiltinTypes::ExprAST }, false);
		MincFunctions::getTypeName = new Func("getTypeName", BuiltinTypes::Int8Ptr, { BuiltinTypes::Base }, false, "getTypeName2");
		MincFunctions::getPointerToBuiltinType = new Func("getPointerToBuiltinType", BuiltinTypes::Builtin, { BuiltinTypes::Builtin }, false);

		MincFunctions::lookupCast = new Func("lookupCast", BuiltinTypes::ExprAST, { BuiltinTypes::BlockExprAST, BuiltinTypes::ExprAST, BuiltinTypes::Base }, false);
		MincFunctions::addToScope = new Func("AddToScope", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
		MincFunctions::addToFileScope = new Func("AddToFileScope", BuiltinTypes::Void, { BuiltinTypes::IdExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
		MincFunctions::lookupSymbol = new Func("LookupScope", BuiltinTypes::LLVMValueRef, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST }, false);
		MincFunctions::lookupScopeType = new Func("LookupScopeType", BuiltinTypes::Base, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST }, false);
		MincFunctions::importSymbol = new Func("ImportSymbol", BuiltinTypes::LLVMValueRef, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST }, false);
		MincFunctions::codegenExprValue = new Func("codegenExprValue", BuiltinTypes::LLVMValueRef, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
		MincFunctions::codegenExprConstant = new Func("codegenExprConstant", BuiltinTypes::LLVMValueRef, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
		MincFunctions::codegenStmt = new Func("codegenStmt", BuiltinTypes::Void, { BuiltinTypes::StmtAST, BuiltinTypes::BlockExprAST }, false);
		MincFunctions::getType = new Func("getType", BuiltinTypes::Base, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
		//MincFunctions::defineStmt = new Func("DefineStatement", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::ExprAST->Ptr(), BuiltinTypes::Int32, TODO, BuiltinTypes::Int8Ptr }, false);
		MincFunctions::getValueFunction = new Func("getValueFunction", BuiltinTypes::LLVMValueRef, { BuiltinTypes::Func }, false);
		MincFunctions::cppNew = new Func("_Znwm", BuiltinTypes::Int8Ptr, { BuiltinTypes::Int64 }, false); //TODO: Replace hardcoded "_Znwm" with mangled "new"
		MincFunctions::raiseCompileError = new Func("raiseCompileError", BuiltinTypes::Void, { BuiltinTypes::Int8Ptr, BuiltinTypes::ExprAST }, false);


		BuiltinScopes::File = FILE_SCOPE_TYPE;

		defineSymbol(rootBlock, "NULL", nullptr, new XXXValue(nullptr, (uint64_t)0));

	//	defineType("BaseType", BuiltinTypes::Base);
		defineSymbol(rootBlock, "BaseType", BuiltinTypes::Builtin, new XXXValue(unwrap(BuiltinTypes::Builtin->llvmtype), (uint64_t)BuiltinTypes::Base));

		defineType(rootBlock, "BuiltinType", BuiltinTypes::Builtin, BuiltinTypes::Builtin);
		defineType(rootBlock, "BuiltinTypePtr", BuiltinTypes::Builtin, BuiltinTypes::Builtin->Ptr());
		defineInheritanceCast2(rootBlock, BuiltinTypes::Builtin, BuiltinTypes::Base,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				return Variable(BuiltinTypes::Base, new XXXValue(builder->CreateBitCast(((XXXValue*)codegenExpr(params[0], parentBlock).value)->val, unwrap(BuiltinTypes::Base->llvmtype))));
			}
		);

		defineType(rootBlock, "void", BuiltinTypes::Builtin, BuiltinTypes::Void);
		defineType(rootBlock, "bool", BuiltinTypes::Builtin, BuiltinTypes::Int1);
		defineType(rootBlock, "char", BuiltinTypes::Builtin, BuiltinTypes::Int8);
		defineType(rootBlock, "int", BuiltinTypes::Builtin, BuiltinTypes::Int32);
		defineType(rootBlock, "intPtr", BuiltinTypes::Builtin, BuiltinTypes::Int32Ptr);
		defineType(rootBlock, "long", BuiltinTypes::Builtin, BuiltinTypes::Int64);
		defineType(rootBlock, "longPtr", BuiltinTypes::Builtin, BuiltinTypes::Int64Ptr);
		defineType(rootBlock, "string", BuiltinTypes::Builtin, BuiltinTypes::Int8Ptr);
		defineType(rootBlock, "stringPtr", BuiltinTypes::Builtin, BuiltinTypes::Int8Ptr->Ptr());
		defineType(rootBlock, "Location", BuiltinTypes::Builtin, BuiltinTypes::Location);
		defineType(rootBlock, "ExprAST", BuiltinTypes::Builtin, BuiltinTypes::ExprAST);
		defineType(rootBlock, "ExprASTPtr", BuiltinTypes::Builtin, BuiltinTypes::ExprAST->Ptr());
		defineType(rootBlock, "ExprListAST", BuiltinTypes::Builtin, BuiltinTypes::ExprListAST);
		defineType(rootBlock, "LiteralExprAST", BuiltinTypes::Builtin, BuiltinTypes::LiteralExprAST);
		defineType(rootBlock, "IdExprAST", BuiltinTypes::Builtin, BuiltinTypes::IdExprAST);
		defineType(rootBlock, "CastExprAST", BuiltinTypes::Builtin, BuiltinTypes::CastExprAST);
		defineType(rootBlock, "BlockExprAST", BuiltinTypes::Builtin, BuiltinTypes::BlockExprAST);
		defineType(rootBlock, "LLVMValueRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMValueRef);
		defineType(rootBlock, "LLVMValueRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMValueRef->Ptr());
		defineType(rootBlock, "LLVMAttributeRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMAttributeRef);
		defineType(rootBlock, "LLVMAttributeRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMAttributeRef->Ptr());
		defineType(rootBlock, "LLVMAttributeRefPtrPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMAttributeRef->Ptr()->Ptr());
		defineType(rootBlock, "LLVMBasicBlockRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMBasicBlockRef);
		defineType(rootBlock, "LLVMBasicBlockRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMBasicBlockRef->Ptr());
		defineType(rootBlock, "LLVMBuilderRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMBuilderRef);
		defineType(rootBlock, "LLVMContextRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMContextRef);
		defineType(rootBlock, "LLVMDiagnosticInfoRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMDiagnosticInfoRef);
		defineType(rootBlock, "LLVMDIBuilderRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMDIBuilderRef);
		defineType(rootBlock, "LLVMMemoryBufferRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMMemoryBufferRef);
		defineType(rootBlock, "LLVMMemoryBufferRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMMemoryBufferRef->Ptr());
		defineType(rootBlock, "LLVMNamedMDNodeRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMNamedMDNodeRef);
		defineType(rootBlock, "LLVMPassManagerRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMPassManagerRef);
		defineType(rootBlock, "LLVMPassRegistryRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMPassRegistryRef);
		defineType(rootBlock, "LLVMTypeRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMTypeRef);
		defineType(rootBlock, "LLVMTypeRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMTypeRef->Ptr());
		defineType(rootBlock, "LLVMMetadataRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMMetadataRef);
		defineType(rootBlock, "LLVMMetadataRefPtr", BuiltinTypes::Builtin, BuiltinTypes::LLVMMetadataRef->Ptr());
		defineType(rootBlock, "LLVMModuleRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMModuleRef);
		defineType(rootBlock, "LLVMModuleFlagEntryRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMModuleFlagEntryRef);
		defineType(rootBlock, "LLVMModuleProviderRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMModuleProviderRef);
		defineType(rootBlock, "LLVMUseRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMUseRef);
		defineType(rootBlock, "LLVMValueMetadataEntryRef", BuiltinTypes::Builtin, BuiltinTypes::LLVMValueMetadataEntryRef);

		defineType(rootBlock, "BuiltinValue", BuiltinTypes::Builtin, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int1, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int8, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int32, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int32Ptr, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int64, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int64Ptr, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int8Ptr, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int8Ptr->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprAST->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprListAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LiteralExprAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::IdExprAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::CastExprAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::BlockExprAST, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMValueRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMValueRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMAttributeRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMAttributeRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMAttributeRef->Ptr()->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMBasicBlockRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMBasicBlockRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMBuilderRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMContextRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMDiagnosticInfoRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMDIBuilderRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMemoryBufferRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMemoryBufferRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMNamedMDNodeRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMPassManagerRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMPassRegistryRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMTypeRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMTypeRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMetadataRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMetadataRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMModuleRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMModuleFlagEntryRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMModuleProviderRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMUseRef, BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMValueMetadataEntryRef, BuiltinTypes::BuiltinValue);

		defineType(rootBlock, "BuiltinPtr", BuiltinTypes::Builtin, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int8Ptr, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int8Ptr->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int32Ptr, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::Int64Ptr, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprAST->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::ExprListAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LiteralExprAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::IdExprAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::CastExprAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::BlockExprAST, BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMValueRef->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMemoryBufferRef->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMTypeRef->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMAttributeRef->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMAttributeRef->Ptr()->Ptr(), BuiltinTypes::BuiltinPtr);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMBasicBlockRef->Ptr(), BuiltinTypes::BuiltinValue);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::LLVMMetadataRef->Ptr(), BuiltinTypes::BuiltinPtr);

		defineType(rootBlock, "BuiltinFunction", BuiltinTypes::Builtin, BuiltinTypes::BuiltinFunction);

		defineType(rootBlock, "BuiltinClass", BuiltinTypes::Builtin, BuiltinTypes::BuiltinClass);
		defineOpaqueInheritanceCast(rootBlock, BuiltinTypes::BuiltinClass, BuiltinTypes::Builtin);
		defineType(rootBlock, "BuiltinInstance", BuiltinTypes::Builtin, BuiltinTypes::BuiltinInstance);
	//	defineType(rootBlock, "BuiltinInstancePtr", BuiltinTypes::Builtin, BuiltinTypes::BuiltinInstance->Ptr());

		defScope = createEmptyBlockExprAST();
		setBlockExprASTParent(defScope, rootBlock);
		defineSymbol(rootBlock, "defScope", PawsBlockExprAST::TYPE, new PawsBlockExprAST(defScope));

		// Define LLVM-c constants in define blocks
		defineSymbol(defScope, "LLVMIntEQ", BuiltinTypes::Int32, new XXXValue(Types::Int32, 32));
		defineSymbol(defScope, "LLVMIntNE", BuiltinTypes::Int32, new XXXValue(Types::Int32, 33));
		defineSymbol(defScope, "LLVMRealOEQ", BuiltinTypes::Int32, new XXXValue(Types::Int32, 1)); //TODO: Untested
		defineSymbol(defScope, "LLVMRealONE", BuiltinTypes::Int32, new XXXValue(Types::Int32, 6)); //TODO: Untested

		// DWARF attribute type encodings
		defineSymbol(defScope, "DW_ATE_address", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x01));
		defineSymbol(defScope, "DW_ATE_boolean", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x02));
		defineSymbol(defScope, "DW_ATE_complex_float", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x03));
		defineSymbol(defScope, "DW_ATE_float", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x04));
		defineSymbol(defScope, "DW_ATE_signed", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x05));
		defineSymbol(defScope, "DW_ATE_signed_char", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x06));
		defineSymbol(defScope, "DW_ATE_unsigned", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x07));
		defineSymbol(defScope, "DW_ATE_unsigned_char", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x08));
		defineSymbol(defScope, "DW_ATE_imaginary_float", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x09));
		defineSymbol(defScope, "DW_ATE_packed_decimal", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0a));
		defineSymbol(defScope, "DW_ATE_numeric_string", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0b));
		defineSymbol(defScope, "DW_ATE_edited", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0c));
		defineSymbol(defScope, "DW_ATE_signed_fixed", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0d));
		defineSymbol(defScope, "DW_ATE_unsigned_fixed", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0e));
		defineSymbol(defScope, "DW_ATE_decimal_float", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x0f));
		defineSymbol(defScope, "DW_ATE_UTF", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x10));
		defineSymbol(defScope, "DW_ATE_UCS", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x11));
		defineSymbol(defScope, "DW_ATE_ASCII", BuiltinTypes::Int32, new XXXValue(Types::Int32, 0x12));

		// Define variable declaration statements in define blocks
		defineStmt2(defScope, "$E<BuiltinType> $I", VarDeclStmt);
		defineStmt2(defScope, "$E<BuiltinType> $I = $E<BuiltinValue>", InitializedVarDeclStmt);

		pawsDefScope = createEmptyBlockExprAST();
		setBlockExprASTParent(pawsDefScope, rootBlock);
		defineSymbol(rootBlock, "pawsDefScope", PawsBlockExprAST::TYPE, new PawsBlockExprAST(pawsDefScope));

		// Define LLVM-c constants in paws define blocks
		defineSymbol(pawsDefScope, "LLVMIntEQ", PawsInt::TYPE, new PawsInt(32));
		defineSymbol(pawsDefScope, "LLVMIntNE", PawsInt::TYPE, new PawsInt(33));
		defineSymbol(pawsDefScope, "LLVMRealOEQ", PawsInt::TYPE, new PawsInt(1)); //TODO: Untested
		defineSymbol(pawsDefScope, "LLVMRealONE", PawsInt::TYPE, new PawsInt(6)); //TODO: Untested

		// DWARF attribute type encodings
		defineSymbol(pawsDefScope, "DW_ATE_address", PawsInt::TYPE, new PawsInt(0x01));
		defineSymbol(pawsDefScope, "DW_ATE_boolean", PawsInt::TYPE, new PawsInt(0x02));
		defineSymbol(pawsDefScope, "DW_ATE_complex_float", PawsInt::TYPE, new PawsInt(0x03));
		defineSymbol(pawsDefScope, "DW_ATE_float", PawsInt::TYPE, new PawsInt(0x04));
		defineSymbol(pawsDefScope, "DW_ATE_signed", PawsInt::TYPE, new PawsInt(0x05));
		defineSymbol(pawsDefScope, "DW_ATE_signed_char", PawsInt::TYPE, new PawsInt(0x06));
		defineSymbol(pawsDefScope, "DW_ATE_unsigned", PawsInt::TYPE, new PawsInt(0x07));
		defineSymbol(pawsDefScope, "DW_ATE_unsigned_char", PawsInt::TYPE, new PawsInt(0x08));
		defineSymbol(pawsDefScope, "DW_ATE_imaginary_float", PawsInt::TYPE, new PawsInt(0x09));
		defineSymbol(pawsDefScope, "DW_ATE_packed_decimal", PawsInt::TYPE, new PawsInt(0x0a));
		defineSymbol(pawsDefScope, "DW_ATE_numeric_string", PawsInt::TYPE, new PawsInt(0x0b));
		defineSymbol(pawsDefScope, "DW_ATE_edited", PawsInt::TYPE, new PawsInt(0x0c));
		defineSymbol(pawsDefScope, "DW_ATE_signed_fixed", PawsInt::TYPE, new PawsInt(0x0d));
		defineSymbol(pawsDefScope, "DW_ATE_unsigned_fixed", PawsInt::TYPE, new PawsInt(0x0e));
		defineSymbol(pawsDefScope, "DW_ATE_decimal_float", PawsInt::TYPE, new PawsInt(0x0f));
		defineSymbol(pawsDefScope, "DW_ATE_UTF", PawsInt::TYPE, new PawsInt(0x10));
		defineSymbol(pawsDefScope, "DW_ATE_UCS", PawsInt::TYPE, new PawsInt(0x11));
		defineSymbol(pawsDefScope, "DW_ATE_ASCII", PawsInt::TYPE, new PawsInt(0x12));

		// Declare LLVM-C extern functions in paws define blocks
		PAWS_PACKAGE_MANAGER().importPackage(pawsDefScope, "llvm");

		// Cast all builtin- and base types to PawsType<LLVMValueRef> in paws define blocks
		defineInheritanceCast2(pawsDefScope, BuiltinTypes::Builtin, PawsType<LLVMValueRef>::TYPE,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				XXXValue* llvmvalue = (XXXValue*)codegenExpr(params[0], parentBlock).value;
				return Variable(PawsType<LLVMValueRef>::TYPE, new PawsType<LLVMValueRef>(wrap(llvmvalue->val)));
			},
			PawsType<LLVMValueRef>::TYPE
		);
		defineInheritanceCast2(pawsDefScope, BuiltinTypes::Base, PawsType<LLVMValueRef>::TYPE,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				XXXValue* llvmvalue = (XXXValue*)codegenExpr(params[0], parentBlock).value;
				return Variable(PawsType<LLVMValueRef>::TYPE, new PawsType<LLVMValueRef>(wrap(llvmvalue->val)));
			},
			PawsType<LLVMValueRef>::TYPE
		);

		// Cast arrays of PawsType<LLVMValueRef> to PawsType<LLVMValueRef*> in paws define blocks
defineSymbol(pawsDefScope, "PawsBase", PawsMetaType::TYPE, new PawsMetaType(PawsBase::TYPE)); //DELETE
defineSymbol(pawsDefScope, "PawsInt", PawsMetaType::TYPE, new PawsMetaType(PawsInt::TYPE)); //DELETE
		PAWS_PACKAGE_MANAGER().importPackage(pawsDefScope, "array");
		defineTypeCast2(pawsDefScope, PawsTpltType::get(PawsType<std::vector<BaseValue*>>::TYPE, PawsType<LLVMValueRef>::TYPE), PawsType<LLVMValueRef*>::TYPE,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
				std::vector<BaseValue*>& arr = ((PawsType<std::vector<BaseValue*>>*)codegenExpr(params[0], parentBlock).value)->get();
				LLVMValueRef* values = new LLVMValueRef[arr.size()];
				for (size_t i = 0; i < arr.size(); ++i)
					values[i] = ((PawsType<LLVMValueRef>*)arr[i])->get();
				return Variable(PawsType<LLVMValueRef*>::TYPE, new PawsType<LLVMValueRef*>(values));
			},
			PawsType<LLVMValueRef*>::TYPE
		);

		// Define Minc extern functions
		for (Func* func: {
			MincFunctions::resolveExprAST,
			MincFunctions::getExprListASTSize,
			MincFunctions::getIdExprASTName,
			MincFunctions::getLiteralExprASTValue,
			MincFunctions::getBlockExprASTParent,
			MincFunctions::setBlockExprASTParent,
			MincFunctions::getPointerToBuiltinType,
			MincFunctions::getExprLoc,
			MincFunctions::getExprLine,
			MincFunctions::getExprColumn,
			MincFunctions::getTypeName,
			MincFunctions::lookupCast,
			MincFunctions::addToScope,
			MincFunctions::lookupSymbol,
			MincFunctions::importSymbol,
			MincFunctions::lookupScopeType,
			MincFunctions::getType,
			MincFunctions::raiseCompileError,
		})
		{
			defineSymbol(rootBlock, func->name, func->type, func);
			defineOpaqueInheritanceCast(rootBlock, func->type, BuiltinTypes::BuiltinFunction);
			defineTypeCast2(defScope, func->type, BuiltinTypes::LLVMValueRef,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
					Value* funcVal = Constant::getIntegerValue(Types::Func->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

					Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
					Value* resultVal = builder->CreateCall(func, { funcVal });
					return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
				}
			);
			defineTypeCast2(pawsDefScope, func->type, PawsType<LLVMValueRef>::TYPE,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* castArgs) -> Variable {
					Variable funcVar = codegenExpr(params[0], parentBlock);
					FuncType* funcType = (FuncType*)funcVar.type;
					Function* func = ((Func*)funcVar.value)->getFunction(currentModule);
					return Variable(PawsType<LLVMValueRef>::TYPE, new PawsType<LLVMValueRef>(wrap(func)));
				}
			);
		}

		// Define function call
		defineExpr3(rootBlock, "$E<BuiltinFunction>($E, ...)",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				assert(ExprASTIsCast(params[0]));
				ExprAST* funcAST = getDerivedExprAST(params[0]);
				Variable funcVar = codegenExpr(funcAST, parentBlock);
				FuncType* funcType = (FuncType*)funcVar.type;
				Function* func = ((Func*)funcVar.value)->getFunction(currentModule);
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
				assert(ExprASTIsCast(params[0]));
				ExprAST* funcAST = getDerivedExprAST(params[0]);
				FuncType* funcType = (FuncType*)getType(funcAST, parentBlock);
				return funcType == nullptr ? nullptr : funcType->resultType;
			}
		);
		// Define function call on non-function expression
		defineExpr2(rootBlock, "$E($E, ...)",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				raiseCompileError("expression cannot be used as a function", params[0]);
			},
			BuiltinTypes::Void
		);
		// Define function call on non-function identifier
		defineExpr2(rootBlock, "$I($E, ...)",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				const char* name = getIdExprASTName((IdExprAST*)params[0]);
				if (lookupSymbol(parentBlock, name) == nullptr)
					raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
				else
					raiseCompileError(('`' + std::string(name) + "` cannot be used as a function").c_str(), params[0]);
			},
			BuiltinTypes::Void
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
					jitFuncName += ExprASTToShortString(stmtParam);
				}

	#ifdef DEBUG_PARAMETER_COLLECTION
				printf("\033[34m%s:\033[0m", jitFuncName.c_str());
				for (ExprAST* stmtParam: stmtParams)
					printf(" %s", ExprASTToString(stmtParam).c_str());
				printf("\n");
	#endif

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, BuiltinTypes::Void);
				importBlock(blockAST, defScope);

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
	// 				jitFuncName += ExprASTToShortString(param);
	// 			}

	//			setScopeType(blockAST, BuiltinScopes::JitFunction);
	//			defineReturnStmt(blockAST, BuiltinTypes::Void);
	//			importBlock(blockAST, defScope);

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
				std::string jitFuncName = ExprASTToShortString(exprAST);

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);
				importBlock(blockAST, defScope);

				JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, exprParams, jitFuncName);
				capturedScope.clear();
				codegenExpr((ExprAST*)blockAST, parentBlock);

				defineExpr(parentBlock, exprAST, jitFunc, exprType);
				removeJitFunction(jitFunc);
			}
		);
		defineStmt2(rootBlock, "exprdef<$E> $E $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				ExprAST* exprTypeAST = params[0];
				ExprAST* exprAST = params[1];
				BlockExprAST* blockAST = (BlockExprAST*)params.back();

				std::vector<ExprAST*> exprParams;
				collectParams(parentBlock, exprAST, exprAST, exprParams);

				// Generate JIT function name
				std::string jitFuncName = ExprASTToShortString(exprAST);

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);
				importBlock(blockAST, defScope);

				JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, exprParams, jitFuncName);
				capturedScope.clear();
				codegenExpr((ExprAST*)blockAST, parentBlock);

				jitFuncName = '<' + jitFuncName + '>';

				BlockExprAST* exprTypeBlock = wrapExprAST(exprTypeAST);
				setScopeType(exprTypeBlock, BuiltinScopes::JitFunction);
				setBlockExprASTParent(exprTypeBlock, parentBlock);
				JitFunction* jitTypeFunc = createJitFunction(parentBlock, exprTypeBlock, BuiltinTypes::Base, exprParams, jitFuncName);
				capturedScope.clear();
				resolveExprAST(exprTypeBlock, exprTypeAST); // Reresolve exprTypeAST context
				BaseType* exprTypeType = getType(exprTypeAST, parentBlock);
				if (exprTypeType != BuiltinTypes::Builtin)
				{
					ExprAST* castExpr = lookupCast(parentBlock, exprTypeAST, BuiltinTypes::Builtin);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, exprTypeAST);
						raiseCompileError(
							("can't convert <" + getTypeName(exprTypeType) + "> to <" + getTypeName(BuiltinTypes::Builtin) + ">\n" + candidateReport).c_str(),
							exprTypeAST
						);
					}
					else
						exprTypeAST = castExpr;
				}
				builder->CreateRet(builder->CreateBitCast(((XXXValue*)codegenExpr(exprTypeAST, exprTypeBlock).value)->val, Types::BaseType->getPointerTo()));

				defineExpr4(parentBlock, exprAST, jitFunc, jitTypeFunc);
				removeJitFunction(jitTypeFunc);
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

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);
				importBlock(blockAST, defScope);

				JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, castParams, jitFuncName);
				capturedScope.clear();
				codegenExpr((ExprAST*)blockAST, parentBlock);

				defineTypeCast(parentBlock, fromType, toType, jitFunc);
				removeJitFunction(jitFunc);
			}
		);

		// Define `inhtdef`
		defineStmt2(rootBlock, "inhtdef<$I> $E $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				//TODO: Check for errors
				BaseType* fromType = getType(params[1], parentBlock);
				//TODO: Check for errors
				BlockExprAST* blockAST = (BlockExprAST*)params[2];

				std::vector<ExprAST*> castParams(1, params[1]);

				// Generate JIT function name
				std::string jitFuncName("cast " + getTypeName(fromType) + " -> " + getTypeName(toType));

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, BuiltinTypes::LLVMValueRef);
				importBlock(blockAST, defScope);

				JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, castParams, jitFuncName);
				capturedScope.clear();
				codegenExpr((ExprAST*)blockAST, parentBlock);

				defineInheritanceCast(parentBlock, fromType, toType, jitFunc);
				removeJitFunction(jitFunc);
			}
		);

		// Define opaque `inhtdef`
		defineStmt2(rootBlock, "inhtdef<$I> $E",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				//TODO: Check for errors
				BaseType* fromType = getType(params[1], parentBlock);
				//TODO: Check for errors

				defineOpaqueInheritanceCast(parentBlock, fromType, toType);
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

				setScopeType(blockAST, BuiltinScopes::JitFunction);
				defineReturnStmt(blockAST, metaType);
				importBlock(blockAST, defScope);

				std::vector<ExprAST*> typeParams;
				JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, metaType, typeParams, jitFuncName);
				capturedScope.clear();
				codegenExpr((ExprAST*)blockAST, parentBlock);

				typedef BuiltinType* (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params);
				funcPtr jitFunctionPtr = reinterpret_cast<funcPtr>(compileJitFunction(jitFunc));
				BuiltinType* type = jitFunctionPtr(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, {});
				removeJitFunctionModule(jitFunc);
				removeJitFunction(jitFunc);

				defineType(getIdExprASTName(nameAST), type);
				defineType(parentBlock, getIdExprASTName(nameAST), metaType, type);
			}
		);

		// Define `stmtdef` from paws type
		defineStmt2(rootBlock, "paws_stmtdef $E ... $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				std::vector<ExprAST*>& stmtParamsAST = getExprListASTExpressions((ExprListAST*)params[0]);
				BlockExprAST* blockAST = (BlockExprAST*)params[1];

				setBlockExprASTParent(blockAST, parentBlock);

				// Collect parameters
				std::vector<ExprAST*> stmtParams;
				for (ExprAST* stmtParam: stmtParamsAST)
					collectParams(parentBlock, stmtParam, stmtParam, stmtParams);

				// Get block parameter types
				std::vector<Variable> blockParams;
				getBlockParameterTypes(parentBlock, stmtParams, blockParams);

				importBlock(blockAST, pawsDefScope);
				definePawsReturnStmt(pawsDefScope, PawsType<LLVMValueRef>::TYPE);

				defineStmt3(parentBlock, stmtParamsAST, new PawsCodegenContext(blockAST, PawsVoid::TYPE, blockParams));
			}
		);

		// Define `exprdef` from paws type
		defineStmt2(rootBlock, "paws_exprdef<$I> $E $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* exprType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				ExprAST* exprAST = params[1];
				BlockExprAST* blockAST = (BlockExprAST*)params[2];

				setBlockExprASTParent(blockAST, parentBlock);

				// Collect parameters
				std::vector<ExprAST*> exprParams;
				collectParams(parentBlock, exprAST, exprAST, exprParams);

				// Get block parameter types
				std::vector<Variable> blockParams;
				getBlockParameterTypes(parentBlock, exprParams, blockParams);

				importBlock(blockAST, pawsDefScope);
				definePawsReturnStmt(pawsDefScope, PawsType<LLVMValueRef>::TYPE);

				defineExpr5(parentBlock, exprAST, new PawsBootstrapCodegenContext(blockAST, exprType, blockParams));
			}
		);
		defineStmt2(rootBlock, "paws_exprdef<$E> $E $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				raiseCompileError("dynamically typed paws expressions not implemented", params[0]);
			}
		);

		// Define `castdef` from paws type
		defineStmt2(rootBlock, "paws_castdef<$I> $E<PawsMetaType> $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				BaseType* fromType = ((PawsMetaType*)codegenExpr(params[1], parentBlock).value)->get();
				BlockExprAST* blockAST = (BlockExprAST*)params[2];

				// Get block parameter types
				std::vector<Variable> blockParams(1, Variable(PawsTpltType::get(PawsExprAST::TYPE, fromType), nullptr));

				importBlock(blockAST, pawsDefScope);
				definePawsReturnStmt(pawsDefScope, PawsType<LLVMValueRef>::TYPE);

				defineTypeCast3(parentBlock, fromType, toType, new PawsBootstrapCodegenContext(blockAST, toType, blockParams));
			}
		);

		// Define `inhtdef` from paws type
		defineStmt2(rootBlock, "paws_inhtdef<$I> $E<PawsMetaType> $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				BaseType* fromType = ((PawsMetaType*)codegenExpr(params[1], parentBlock).value)->get();
				BlockExprAST* blockAST = (BlockExprAST*)params[2];

				// Get block parameter types
				std::vector<Variable> blockParams(1, Variable(PawsTpltType::get(PawsExprAST::TYPE, fromType), nullptr));

				importBlock(blockAST, pawsDefScope);
				definePawsReturnStmt(pawsDefScope, PawsType<LLVMValueRef>::TYPE);

				defineInheritanceCast3(parentBlock, fromType, toType, new PawsBootstrapCodegenContext(blockAST, toType, blockParams));
			}
		);

		// Define `typedef` from paws type
		defineStmt2(rootBlock, "paws_typedef<$I> $I $B",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
				BuiltinType* metaType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
				const char* name = getIdExprASTName((IdExprAST*)params[1]);
				BlockExprAST* blockAST = (BlockExprAST*)params[2];

				if (metaType == getVoid().type || metaType == PawsVoid::TYPE)
					raiseCompileError("cannot define void type", params[0]);

				// Get block parameter types
				std::vector<Variable> blockParams;

				importBlock(blockAST, pawsDefScope);
				definePawsReturnStmt(pawsDefScope, metaType);

				// Execute typedef code block
				try
				{
					codegenExpr((ExprAST*)blockAST, parentBlock);
				}
				catch (ReturnException err)
				{
					BuiltinType* type = (BuiltinType*)err.result.value;

					defineType(name, type);
					defineType(parentBlock, name, metaType, type);
					return;
				}

				raiseCompileError("missing return statement in typedef block", (ExprAST*)blockAST);
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
				defineSymbol(parentBlock, funcName, func->type, func);
				defineOpaqueInheritanceCast(parentBlock, func->type, BuiltinTypes::BuiltinFunction);
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
				defineType(getIdExprASTName(nameAST), structType);
				defineType(parentBlock, getIdExprASTName(nameAST), BuiltinTypes::BuiltinClass, structType);
				defineOpaqueInheritanceCast(parentBlock, structType, BuiltinTypes::BuiltinInstance);

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
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
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
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
				ClassType* structType = (ClassType*)getType(structAST, parentBlock);
				std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

				auto variable = structType->variables.find(memberName);
				return variable == structType->variables.end() ? nullptr : variable->second.type;
			}
		);

		// Define struct member setter
		defineExpr3(rootBlock, "$E<BuiltinInstance>.$I = $E<BuiltinValue>",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
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

				assert(ExprASTIsCast(params[2]));
				ExprAST* varExpr = getDerivedExprAST(params[2]);
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
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
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
					FuncType* constructorType = constructor.func->type;
					if (constructorType->argTypes.size() - 1 != numArgs)
						continue;

					for (size_t i = 0; i < numArgs; ++i)
					{
						ExprAST* argExpr = argExprs[i];
						BaseType *expectedType = constructorType->argTypes[i + 1], *gotType = getType(argExpr, parentBlock);

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
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
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
				assert(ExprASTIsCast(params[0]));
				ExprAST* structAST = getDerivedExprAST(params[0]);
				ClassType* structType = (ClassType*)getType(structAST, parentBlock);
				std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

				auto member = structType->members.find(memberName);
				return member == structType->members.end() || !member->second.isMethod() ? nullptr : ((FuncType*)member->second.type)->resultType;
			}
		);*/

		// Define $E<ExprAST>.codegen($E<BlockExprAST>)
		defineExpr2(defScope, "$E<ExprAST>.codegen($E<BlockExprAST>)",
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
		defineExpr2(defScope, "$E.codegen($E, ...)",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);
				if (argExprs.size() != 1)
					raiseCompileError("invalid number of function arguments", params[1]);
				argExprs.insert(argExprs.begin(), params[0]);

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
				ExprAST* exprAST = getDerivedExprAST(params[0]);
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
				ExprAST* exprAST = getDerivedExprAST(params[0]);
				const TpltType* exprType = dynamic_cast<const TpltType*>((const BuiltinType*)getType(exprAST, parentBlock));
				assert(exprType != 0); //TODO: Non-template builtin types don't resolve to 0!
				return exprType->tpltType;
			}
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
				const Variable* typeVar = importSymbol(parentBlock, getIdExprASTName(typeAST));
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

		// Define variable lookup
		defineExpr3(rootBlock, "$I",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				const char* varName = getIdExprASTName((IdExprAST*)params[0]);
				const Variable* var = importSymbol(parentBlock, varName);
				if (var == nullptr)
					raiseCompileError(("`" + std::string(varName) + "` was not declared in this scope").c_str(), params[0]);
				if (!isInstance(parentBlock, var->type, BuiltinTypes::Base))
					return *var;

				XXXValue* varVal = (XXXValue*)var->value;
				if (!varVal) raiseCompileError(("invalid use of type `" + std::string(varName) + "` as expression").c_str(), params[0]);
				if (varVal->isConstant() || isInstance(parentBlock, var->type, BuiltinTypes::BuiltinFunction))
					return *var;

				LoadInst* loadVal = builder->CreateLoad(((XXXValue*)var->value)->val, varName);
				loadVal->setAlignment(4);
				return Variable(var->type, new XXXValue(loadVal));
			},
			[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
				const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
				return var != nullptr ? var->type : nullptr;
			}
		);

		defineExpr3(rootBlock, "$E<ExprListAST>[$E<long>]",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
				assert(ExprASTIsCast(params[0]));
				Variable listVar = codegenExpr(getDerivedExprAST(params[0]), parentBlock);
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
				return Variable(listType->tpltType, new XXXValue(builder->CreateBitCast(resultVal, unwrap(listType->tpltType->llvmtype))));
			},
			[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
				assert(ExprASTIsCast(params[0]));
				ExprAST* arrAST = getDerivedExprAST(params[0]);
				TpltType* listType = dynamic_cast<TpltType*>((BuiltinType*)getType(arrAST, parentBlock));
				assert(listType);
				return listType->tpltType;
			}
		);

// registerType<PawsType<BuiltinType*>>(rootBlock, "PawsBuiltinType");
// defineExpr(rootBlock, "_BuiltinType($E<PawsString>, $E<PawsLLVMTypeRef>, $E<PawsInt>)",
// 	+[](std::string name, LLVMTypeRef llvmtype, int align) -> BuiltinType* {
// 		return BuiltinType::get(name.c_str(), llvmtype, align, 0, 0);
// 	}
// );
defineExpr2(rootBlock, "_BuiltinType($E<PawsLLVMTypeRef>, $E<PawsInt>, $E<PawsInt>, $E<PawsInt>)",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
		PawsType<LLVMTypeRef>* llvmtype = (PawsType<LLVMTypeRef>*)codegenExpr(params[0], parentBlock).value;
		PawsInt* align = (PawsInt*)codegenExpr(params[1], parentBlock).value;
		PawsInt* encoding = (PawsInt*)codegenExpr(params[2], parentBlock).value;
		PawsInt* numbits = (PawsInt*)codegenExpr(params[3], parentBlock).value;
		return Variable(BuiltinTypes::Builtin, (BaseValue*)new BuiltinType(llvmtype->get(), align->get(), encoding->get(), numbits->get()));
	},
	BuiltinTypes::Builtin
);

defineSymbol(rootBlock, "PawsBlockExprAST", PawsMetaType::TYPE, new PawsMetaType(PawsBlockExprAST::TYPE)); //DELETE
defineStmt2(rootBlock, "$E<BuiltinType> $E<PawsBlockExprAST>::$I($E<BuiltinType>, ...)",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
		BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
		BlockExprAST* scope = ((PawsBlockExprAST*)codegenExpr(params[1], parentBlock).value)->get();
		const char* funcName = getIdExprASTName((IdExprAST*)params[2]);
		std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[3]);

		std::vector<BuiltinType*> argTypes; argTypes.reserve(argTypeExprs.size());
		for (ExprAST* argTypeExpr: argTypeExprs)
			argTypes.push_back((BuiltinType*)codegenExpr(argTypeExpr, parentBlock).value->getConstantValue());

		defineFunction(scope, funcName, returnType, argTypes.data(), argTypeExprs.size(), false);
	}
);

		defineImportRule(BuiltinScopes::File, BuiltinScopes::File, BuiltinTypes::BuiltinValue,
			[](Variable& symbol, BaseScopeType* fromScope, BaseScopeType* toScope) -> void {
				XXXValue* value = (XXXValue*)symbol.value;
				if (!value->isConstant())
				{
					assert(value->val->getName().size()); // Importing anonymus globals results in a seg fault!
					symbol.value = new XXXValue(new GlobalVariable(*currentModule, unwrap(((BuiltinType*)symbol.type)->llvmtype), false, GlobalValue::ExternalLinkage, nullptr, value->val->getName()));
				}
			}
		);
	}
} PAWS_BOOTSTRAP;