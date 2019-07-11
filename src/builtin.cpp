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
	Func* getIdExprASTName;
	Func* getLiteralExprASTValue;
	Func* getBlockExprASTParent;
	Func* setBlockExprASTParent;
	Func* getPointerToBuiltinType;

	Func* addToScope;
	Func* addToFileScope;
	Func* codegenExprValue;
	Func* codegenStmt;
	//Func* defineStmt;
	Func* getValueFunction;
	Func* createFuncType;
}

Variable lookupVariable(const BlockExprAST* parentBlock, const IdExprAST* id)
{
	bool isCaptured;
	const Variable* var = lookupSymbol(parentBlock, getIdExprASTName(id), isCaptured);
	if (var == nullptr)
		raiseCompileError(("`" + std::string(getIdExprASTName(id)) + "` was not declared in this scope").c_str(), (ExprAST*)id);
auto foo = getIdExprASTName(id);
	XXXValue* varVal = var->value;
	if (!varVal) raiseCompileError(("invalid use of type `" + std::string(getIdExprASTName(id)) + "` as expression").c_str(), (ExprAST*)id);
	if (varVal->isFunction() || isa<Constant>(varVal->val))
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

void initBuiltinSymbols()
{
	// >>> Create builtin types

	BuiltinTypes::Base = BuiltinType::get("BaseType", wrap(Types::BaseType->getPointerTo()), 8);
	BuiltinTypes::Builtin = BuiltinType::get("BuiltinType", wrap(Types::BuiltinType), 8);
	BuiltinTypes::BuiltinValue = BuiltinType::get("BuiltinValue", nullptr, 0);
	BuiltinTypes::Value = BuiltinType::get("Value", wrap(Types::Value->getPointerTo()), 8);

	// Primitive types
	BuiltinTypes::Void = BuiltinType::get("void", LLVMVoidType(), 0);
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
	BuiltinTypes::LiteralExprAST = BuiltinType::get("LiteralExprAST", wrap(Types::LiteralExprAST->getPointerTo()), 8);
	BuiltinTypes::IdExprAST = BuiltinType::get("IdExprAST", wrap(Types::IdExprAST->getPointerTo()), 8);
	BuiltinTypes::CastExprAST = BuiltinType::get("CastExprAST", wrap(Types::CastExprAST->getPointerTo()), 8);
	BuiltinTypes::BlockExprAST = BuiltinType::get("BlockExprAST", wrap(Types::BlockExprAST->getPointerTo()), 8);
	BuiltinTypes::StmtAST = BuiltinType::get("StmtAST", wrap(Types::StmtAST->getPointerTo()), 8);

	// Misc. types
	BuiltinTypes::Function = BuiltinType::get("func", nullptr, 8);

	// >>> Create LLVM-c extern functions

	create_llvm_c_functions(*context, llvm_c_functions);

	// >>> Create Minc extern functions

	MincFunctions::getIdExprASTName = new Func("getIdExprASTName", BuiltinTypes::Int8Ptr, { BuiltinTypes::IdExprAST }, false);
	MincFunctions::getLiteralExprASTValue = new Func("getLiteralExprASTValue", BuiltinTypes::Int8Ptr, { BuiltinTypes::LiteralExprAST }, false);
	MincFunctions::getBlockExprASTParent = new Func("getBlockExprASTParent", BuiltinTypes::BlockExprAST, { BuiltinTypes::BlockExprAST }, false);
	MincFunctions::setBlockExprASTParent = new Func("setBlockExprASTParent", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::getPointerToBuiltinType = new Func("getPointerToBuiltinType", BuiltinTypes::Builtin, { BuiltinTypes::Builtin }, false);

	MincFunctions::addToScope = new Func("AddToScope", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::IdExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
	MincFunctions::addToFileScope = new Func("AddToFileScope", BuiltinTypes::Void, { BuiltinTypes::ExprAST, BuiltinTypes::Base, BuiltinTypes::LLVMValueRef }, false);
	MincFunctions::codegenExprValue = new Func("codegenExprValue", BuiltinTypes::LLVMValueRef, { BuiltinTypes::ExprAST, BuiltinTypes::BlockExprAST }, false);
	MincFunctions::codegenStmt = new Func("codegenStmt", BuiltinTypes::Void, { BuiltinTypes::StmtAST, BuiltinTypes::BlockExprAST }, false);
	//MincFunctions::defineStmt = new Func("DefineStatement", BuiltinTypes::Void, { BuiltinTypes::BlockExprAST, BuiltinTypes::ExprAST->Ptr(), BuiltinTypes::Int32, TODO, BuiltinTypes::Int8Ptr }, false);
	MincFunctions::getValueFunction = new Func("getValueFunction", BuiltinTypes::LLVMValueRef, { BuiltinTypes::Value }, false);
	MincFunctions::createFuncType = new Func("createFuncType", BuiltinTypes::Base, { BuiltinTypes::Int8Ptr, BuiltinTypes::Int8, BuiltinTypes::Base, BuiltinTypes::Base->Ptr(), BuiltinTypes::Int32 }, false);
}

void defineBuiltinSymbols(BlockExprAST* rootBlock)
{
	// Define LLVM-c extern functions
	for (Func& func: llvm_c_functions)
		defineSymbol(rootBlock, func.type.name, &func.type, &func);
bool isCaptured; lookupSymbol(rootBlock, "printf", isCaptured)->value->getFunction(currentModule); //DELETE

	// Define Minc extern functions
	for (Func* func: {
		MincFunctions::getIdExprASTName,
		MincFunctions::getLiteralExprASTValue,
		MincFunctions::getBlockExprASTParent,
		MincFunctions::setBlockExprASTParent,
		MincFunctions::getPointerToBuiltinType
	})
	{
		defineSymbol(rootBlock, func->type.name, &func->type, func);
		defineCast2(rootBlock, &func->type, BuiltinTypes::LLVMValueRef,
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
				Value* funcVal = Constant::getIntegerValue(Types::Value->getPointerTo(), APInt(64, (uint64_t)codegenExpr(params[0], parentBlock).value, true));

				Function* func = MincFunctions::getValueFunction->getFunction(currentModule);
				Value* resultVal = builder->CreateCall(func, { funcVal });
				return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
			}
		);
	}

	BaseType* baseType = getBaseType();
	defineSymbol(rootBlock, "BaseType", baseType, new XXXValue(Types::BaseType->getPointerTo(), (uint64_t)baseType));

	XXXValue* builtinTypeVal = new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Builtin);
	defineSymbol(rootBlock, "BuiltinType", BuiltinTypes::Builtin, builtinTypeVal);

	/*
	XXXValue* intTypeVal = new XXXValue(Types::BuiltinType, (uint64_t)Int32);
	defineSymbol(rootBlock, "IntType", builtinTypeVal, intTypeVal);*/

	defineSymbol(rootBlock, "void", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Void));
	defineSymbol(rootBlock, "bool", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Int1));
	defineSymbol(rootBlock, "char", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Int8));
	defineSymbol(rootBlock, "int", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Int32));
	defineSymbol(rootBlock, "long", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Int64));
	defineSymbol(rootBlock, "double", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Double));
	defineSymbol(rootBlock, "doublePtr", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::DoublePtr));
	defineSymbol(rootBlock, "string", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Int8Ptr));
	defineSymbol(rootBlock, "func", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::Function));
	defineSymbol(rootBlock, "ExprAST", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::ExprAST));
	defineSymbol(rootBlock, "LiteralExprAST", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LiteralExprAST));
	defineSymbol(rootBlock, "IdExprAST", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::IdExprAST));
	defineSymbol(rootBlock, "CastExprAST", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::CastExprAST));
	defineSymbol(rootBlock, "BlockExprAST", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::BlockExprAST));
	defineSymbol(rootBlock, "LLVMValueRef", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMValueRef));
	defineSymbol(rootBlock, "LLVMValueRefPtr", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMValueRef->Ptr()));
	defineSymbol(rootBlock, "LLVMBasicBlockRef", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMBasicBlockRef));
	defineSymbol(rootBlock, "LLVMTypeRef", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMTypeRef));
	defineSymbol(rootBlock, "LLVMTypeRefPtr", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMTypeRef->Ptr()));
	defineSymbol(rootBlock, "LLVMMetadataRef", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMMetadataRef));
	defineSymbol(rootBlock, "LLVMMetadataRefPtr", BuiltinTypes::Builtin, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::LLVMMetadataRef->Ptr()));

	defineSymbol(rootBlock, "BuiltinValue", BuiltinTypes::BuiltinValue, new XXXValue(Types::BuiltinType, (uint64_t)BuiltinTypes::BuiltinValue));
	defineOpaqueCast(rootBlock, BuiltinTypes::Int1, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int8, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int32, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int64, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Double, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::DoublePtr, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Int8Ptr, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::Function, BuiltinTypes::BuiltinValue);
	defineOpaqueCast(rootBlock, BuiltinTypes::ExprAST, BuiltinTypes::BuiltinValue);
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

	// Define single-expr statement
	defineStmt2(rootBlock, "$E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			codegenExpr(params[0], parentBlock).value;
		}
	);

	// Define function call
	defineExpr3(rootBlock, "$I($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			ExprAST* funcAST = params[0];
			Variable funcVar = codegenExpr(funcAST, parentBlock);
			Function* func = funcVar.value->getFunction(currentModule);
			FuncType* funcType = (FuncType*)funcVar.type;
			if (funcType == nullptr)
				raiseCompileError(('`' + ExprASTToString(funcAST) + "` doesn't have a type").c_str(), funcAST);

			if ((func->isVarArg() && func->arg_size() > params.size() - 1) ||
				(!func->isVarArg() && func->arg_size() != params.size() - 1))
				raiseCompileError("invalid number of function arguments", funcAST);
			
			for (size_t i = 0; i < func->arg_size(); ++i)
			{
				BuiltinType *expectedType = funcType->argTypes[i], *gotType = (BuiltinType*)getType(params[i + 1], parentBlock);

				if (expectedType != gotType)
				{
//printf("implicit cast from %s to %s in %s:%i\n", ((BuiltinType*)expectedType)->name, ((BuiltinType*)gotType)->name, expr->loc.filename, expr->loc.begin_line);
					ExprAST* castExpr = lookupCast(parentBlock, params[i + 1], expectedType);
					if (castExpr == nullptr)
					{
						std::string expectedTypeStr = expectedType->name, gotTypeStr = gotType == nullptr ? "NULL" : gotType->name;
						std::string candidateReport = reportExprCandidates(parentBlock, params[i + 1]);
						raiseCompileError(
							("invalid function argument type: " + ExprASTToString(params[i + 1]) + "<" + gotTypeStr + ">, expected: <" + expectedTypeStr + ">\n" + candidateReport).c_str(),
							params[i + 1]
						);
					}
					params[i + 1] = castExpr;
				}
			}

			std::vector<Value*> argValues;
			for (auto arg = params.begin() + 1; arg != params.end(); ++arg)
				argValues.push_back(codegenExpr(*arg, parentBlock).value->val);
	
			return Variable(funcType->resultType, new XXXValue(builder->CreateCall(func, argValues)));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) -> BaseType* {
			FuncType* funcType = (FuncType*)getType(params[0], parentBlock);
			//TODO: Check errors
			return funcType == nullptr ? nullptr : funcType->resultType;
		}
	);

	// Define `import`
	defineStmt2(rootBlock, "import $L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			LiteralExprAST* pathAST = (LiteralExprAST*)params[0];
			std::string path(getLiteralExprASTValue(pathAST));
			path = path.substr(1, path.length() - 2);

			importModule(parentBlock, path.c_str(), (ExprAST*)pathAST);
		}
	);
	defineStmt2(rootBlock, "import <$I.$I>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			IdExprAST* filenameAST = (IdExprAST*)params[0];
			IdExprAST* fileextAST = (IdExprAST*)params[1];

			importModule(parentBlock, ("../lib/" + std::string(getIdExprASTName(filenameAST)) + '.' + std::string(getIdExprASTName(fileextAST))).c_str(), (ExprAST*)filenameAST);
		}
	);

	// Define `stmtdef`
	defineStmt2(rootBlock, "stmtdef $E ... $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			BlockExprAST* blockAST = (BlockExprAST*)params.back();
			params.pop_back(); //TODO: Consider switching to const params and using `params = std::vector<ExprAST*>(params.begin(), params.end() - 1);`

			std::vector<ExprAST*> stmtParams;
			for (int i = 0; i < params.size(); ++i)
				collectParams(parentBlock, params[i], params[i], stmtParams);

			// Generate JIT function name
			std::string jitFuncName = "";
			for (auto param: params)
			{
				if (param != params.front())
					jitFuncName += ' ';
				jitFuncName += ExprASTIsBlock(param) ? std::string("{}") : ExprASTToString(param);
			}

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::Void, stmtParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineStmt(parentBlock, params, jitFunc);
			removeJitFunction(jitFunc);
		}
	);

// 	// Define `stmtdef2` //TODO: Should be `$B.stmtdef ... { ... }`
// 	defineStmt2(rootBlock, "stmtdef2 $E $E ... $B",
// 		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
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
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			BaseType* exprType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			//TODO: Check for errors
			ExprAST* exprAST = params[1];
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			std::vector<ExprAST*> exprParams;
			collectParams(parentBlock, exprAST, exprAST, exprParams);

			// Generate JIT function name
			std::string jitFuncName = ExprASTIsBlock(exprAST) ? std::string("{}") : ExprASTToString(exprAST);

			/*const Variable* typeVar = parentBlock->lookupScope(getIdExprASTName(typeAST));
			if (!typeVar)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` was not declared in this scope").c_str(), (ExprAST*)typeAST);
			if (typeVar->value)
				raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` is not a type").c_str(), (ExprAST*)typeAST);*/

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, exprParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineExpr(parentBlock, exprAST, jitFunc, exprType);
			removeJitFunction(jitFunc);
		}
	);

	// Define `castdef`
	defineStmt2(rootBlock, "castdef<$I> $E $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			BaseType* toType = (BaseType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			//TODO: Check for errors
			BaseType* fromType = getType(params[1], parentBlock);
			//TODO: Check for errors
			BlockExprAST* blockAST = (BlockExprAST*)params[2];

			std::vector<ExprAST*> castParams(1, params[1]);

			// Generate JIT function name
			std::string jitFuncName("cast " + std::string(((BuiltinType*)fromType)->name) + " -> " + std::string(((BuiltinType*)toType)->name));

			JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinTypes::LLVMValueRef, castParams, jitFuncName);
			capturedScope.clear();
			codegenExpr((ExprAST*)blockAST, parentBlock);

			defineCast(parentBlock, fromType, toType, jitFunc);
			removeJitFunction(jitFunc);
		}
	);

	// // Define `typedef`
	// defineStmt2(rootBlock, "typedef<$I> $I $B",
	// 	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
	// 		IdExprAST* typeAST = (IdExprAST*)params.front();
	// 		IdExprAST* nameAST = (IdExprAST*)params[1];
	// 		BlockExprAST* blockAST = (BlockExprAST*)params.back();
 
	// 		// Define `return`
	// 		Type* returnType = nullptr;
	// 		defineStmt2(blockAST, "fake_return $E",
	// 			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
	// 				Type* returnStructType = StructType::get(Types::Int64, Types::LLVMOpaqueType->getPointerTo());
	// 				Value* resultVal = codegenExpr(params[0], parentBlock).value->val;
	// 				Value* resultTypeVal = Constant::getIntegerValue(Types::LLVMOpaqueType->getPointerTo(), APInt(64, (uint64_t)resultVal->getType()));
	// 				Value* returnStruct = builder->CreateInsertValue(UndefValue::get(returnStructType), builder->CreatePtrToInt(resultVal, Types::Int64), { 0 });
	// 				returnStruct = builder->CreateInsertValue(returnStruct, resultTypeVal, { 1 });
	// 				builder->CreateRet(returnStruct);
	// 			}
	// 		);

	// 		std::vector<ExprAST*> typeParams;
	// 		collectParams(parentBlock, (ExprAST*)nameAST, (ExprAST*)nameAST, typeParams);

	// 		// Generate JIT function name
	// 		std::string jitFuncName = getIdExprASTName(nameAST);

	// 		bool isCaptured;
	// 		const Variable* typeVar = lookupSymbol(parentBlock, getIdExprASTName(typeAST), isCaptured);
	// 		if (!typeVar)
	// 			raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` was not declared in this scope").c_str(), (ExprAST*)typeAST);
	// 		if (typeVar->value)
	// 			raiseCompileError(("`" + std::string(getIdExprASTName(typeAST)) + "` is not a type").c_str(), (ExprAST*)typeAST);
	// 		//Type* returnType = typeVar->value->type->getPointerTo();

	// 		Type* returnStructType = StructType::get(Types::Int64, Types::LLVMOpaqueType->getPointerTo());
	// 		struct ReturnStruct { uint64_t result; Type* resultType; };

	// 		JitFunction* jitFunc = createJitFunction(parentBlock, blockAST, BuiltinType::get("typedefReturnStructType", wrap(returnStructType), 8), typeParams, jitFuncName);
	// 		capturedScope.clear();
	// 		codegenExpr((ExprAST*)blockAST, parentBlock);

	// 		typedef ReturnStruct (*funcPtr)(LLVMBuilderRef, LLVMModuleRef, LLVMValueRef, BlockExprAST* parentBlock, ExprAST** params);
	// 		funcPtr jitFunctionPtr = reinterpret_cast<funcPtr>(compileJitFunction(jitFunc));
	// 		ReturnStruct type = jitFunctionPtr(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, {});
	// 		removeJitFunctionModule(jitFunc);
	// 		removeJitFunction(jitFunc);

	// 		Value* typeVal = Constant::getIntegerValue(type.resultType, APInt(64, type.result));
	// 		//Value* val = jitFunctionPtr(wrap(builder), wrap(currentModule), wrap(currentFunc), parentBlock, {});

	// 		/*AllocaInst* typeValPtr = builder->CreateAlloca(returnType, nullptr);
	// 		typeValPtr->setAlignment(8);
	// 		builder->CreateStore(typeVal, typeValPtr)->setAlignment(8);*/

	// 		defineSymbol(parentBlock, getIdExprASTName(nameAST), typeVar->type, new XXXValue(typeVal));
	// 	}
	// );

	// Define `do`
	defineStmt2(rootBlock, "$E.do $E ...",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			Value* parentBlockVal = codegenExpr(params[0], parentBlock).value->val;
			//Value* parentBlockVal = Constant::getIntegerValue(Types::BlockExprAST->getPointerTo(), APInt(64, (uint64_t)parentBlock, true));
			std::vector<ExprAST*> stmtParams(params.begin() + 1, params.end());

			StmtAST* stmt = lookupStmt(parentBlock, stmtParams);
			if (!stmt)
				raiseCompileError(("undefined statement " + StmtASTToString(stmt)).c_str(), stmtParams[0]); //TODO: loc should spam all params
			Value* stmtVal = Constant::getIntegerValue(Types::StmtAST->getPointerTo(), APInt(64, (uint64_t)stmt, true));

			Function* func = MincFunctions::codegenStmt->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(func, { stmtVal, parentBlockVal });
		}
	);

	// Define variable declaration
	defineStmt2(rootBlock, "int_ref $I",
			[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
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
	defineStmt2(rootBlock, "int_ref $I = $<int>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
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

			XXXValue* val = codegenExpr(valAST, parentBlock).value;
			builder->CreateStore(val->val, var)->setAlignment(4);

			defineSymbol(parentBlock, getIdExprASTName(varAST), BuiltinTypes::Int32, new XXXValue(var));
		}
	);

	// Define `auto` variable declaration with initialization //TODO: not working
	defineStmt2(rootBlock, "auto $I = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			IdExprAST* varAST = (IdExprAST*)params[0];
			ExprAST* valAST = (ExprAST*)params[1];
			XXXValue* val = codegenExpr(valAST, parentBlock).value;

			val->val->setName(getIdExprASTName(varAST));
			defineSymbol(parentBlock, getIdExprASTName(varAST), nullptr, val);
		}
	);

	// Define function definition
	defineStmt2(rootBlock, "$<BuiltinType> $I($<BuiltinType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
			BuiltinType* returnType = (BuiltinType*)codegenExpr(params[0], parentBlock).value->getConstantValue();
			const char* funcName = getIdExprASTName((IdExprAST*)params[1]);
			BlockExprAST* blockAST = (BlockExprAST*)params.back();

			size_t numArgs = (params.size() - 3) / 2;
			std::vector<BuiltinType*> argTypes; argTypes.reserve(numArgs);
			std::vector<const char*> argNames; argNames.reserve(numArgs);
			for (size_t i = 0; i < numArgs; ++i)
			{
				argTypes.push_back((BuiltinType*)codegenExpr(params[i * 2 + 2], parentBlock).value->getConstantValue());
				argNames.push_back(getIdExprASTName((IdExprAST*)params[i * 2 + 3]));
			}
			
			Func *func = new Func(funcName, returnType, argTypes, false);
			Function* parentFunc = currentFunc;
			currentFunc = func->getFunction(currentModule);
			currentFunc->setDSOLocal(true);

			// Create entry BB in currentFunc
			BasicBlock *parentBB = currentBB;
			builder->SetInsertPoint(currentBB = BasicBlock::Create(*context, "entry", currentFunc));

			if (dbuilder)
			{
				DIScope *FContext = dfile;
				DISubprogram *subprogram = dbuilder->createFunction(
					parentFunc->getSubprogram(), funcName, "", dfile, getExprLine(params[1]),
					dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray({})), //TODO: Replace {} with array of argument types
					getExprLine((ExprAST*)blockAST), DINode::FlagPrototyped, DISubprogram::SPFlagDefinition)
				;
				currentFunc->setSubprogram(subprogram);

				for (size_t i = 0; i < numArgs; ++i)
				{
					ExprAST* argNameAST = params[i * 2 + 3];
					DILocalVariable *D = dbuilder->createParameterVariable(
						currentFunc->getSubprogram(),
						argNames[i],
						i,
						dfile,
						getExprLine(argNameAST),
						intType, //TODO: Replace with argTypes[i].ditype
						true
					);
					dbuilder->insertDeclare(
						currentFunc->args().begin() + i, D, dbuilder->createExpression(),
						DebugLoc::get(getExprLine(argNameAST), getExprColumn(argNameAST), currentFunc->getSubprogram()),
						builder->GetInsertBlock()
					);
				}
			}

			// Define argument symbols in function scope
			for (size_t i = 0; i < numArgs; ++i)
			{
				AllocaInst* argPtr = builder->CreateAlloca(unwrap(argTypes[i]->Ptr()->llvmtype), nullptr, argNames[i]);
				argPtr->setAlignment(8);
				builder->CreateStore(currentFunc->args().begin() + i, argPtr)->setAlignment(8);
				defineSymbol(blockAST, argNames[i], argTypes[i], new XXXValue(argPtr));
			}

			// Codegen function body
			codegenExpr((ExprAST*)blockAST, parentBlock).value;

			if (currentFunc->getReturnType()->isVoidTy()) // If currentFunc is void function
				builder->CreateRetVoid(); // Add implicit `return;`

			// Close function
			if (dbuilder)
			{
				dbuilder->finalizeSubprogram(currentFunc->getSubprogram());
				builder->SetCurrentDebugLocation(DebugLoc());
			}
			bool haserr = verifyFunction(*currentFunc, &outs());
			builder->SetInsertPoint(currentBB = parentBB);
			currentFunc = parentFunc;
//			if (haserr) assert(0); //TODO: Raise exception

			// Define function symbol in parent scope
			defineSymbol(parentBlock, funcName, &func->type, func);
		}
	);

	// Define codegen($<ExprAST>, $E)
	defineExpr2(rootBlock, "codegen($<ExprAST>, $E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* exprVal = codegenExpr(params[0], parentBlock).value->val;
			Value* parentBlockVal = codegenExpr(params[1], parentBlock).value->val;

			Function* func = MincFunctions::codegenExprValue->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(func, { exprVal, parentBlockVal });
			return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
		},
		BuiltinTypes::LLVMValueRef
	);

	/*// Define codegen($<LiteralExprAST>, $E) //TODO: This should be implemented with casting LiteralExprAST -> ExprAST
	defineExpr2(rootBlock, "codegen($<LiteralExprAST>, $E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* exprVal = codegenExpr(params[0], parentBlock).value->val;
			Value* parentBlockVal = codegenExpr(params[1], parentBlock).value->val;

			exprVal = builder->CreateBitCast(exprVal, Types::ExprAST->getPointerTo());

			Function* func = MincFunctions::codegenExprValue->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(func, { exprVal, parentBlockVal });
			return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(resultVal));
		},
		BuiltinTypes::LLVMValueRef
	);*/

	// Define addToScope()
	defineExpr2(rootBlock, "addToScope($E, $E, $E, $E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* parentBlockVal = codegenExpr(params[0], parentBlock).value->val;
			Value* nameVal; //= codegenExpr(params[1], parentBlock).value->val;
			Value* typeVal = codegenExpr(params[2], parentBlock).value->val;
			Value* valVal = codegenExpr(params[3], parentBlock).value->val;

			if (ExprASTIsParam(params[1]))
				nameVal = codegenExpr(params[1], parentBlock).value->val;
			else if (ExprASTIsId(params[1]))
				nameVal = builder->CreateBitCast(Constant::getIntegerValue(Types::IdExprAST->getPointerTo(), APInt(64, (uint64_t)params[1], true)), Types::ExprAST->getPointerTo());
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
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			BaseType* type = getType(params[0], parentBlock);
BuiltinType* foo = (BuiltinType*)type;
printf("gettype(%s) == %s\n", ExprASTToString(params[0]).c_str(), type ? ((BuiltinType*)type)->name : "NULL");
			//return Variable(nullptr, new XXXValue(Constant::getIntegerValue(Types::BaseType->getPointerTo(), APInt(64, (uint64_t)type))));
return Variable(BuiltinTypes::Builtin, new XXXValue(Constant::getIntegerValue(Types::BuiltinType, APInt(64, (uint64_t)type))));
		},
		BuiltinTypes::Builtin
	);

	// Define symdef
	defineStmt2(rootBlock, "symdef<$E> $E = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) {
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
				nameVal = codegenExpr(params[1], parentBlock).value->val;
			else if (ExprASTIsId(params[1]))
				nameVal = builder->CreateBitCast(Constant::getIntegerValue(Types::IdExprAST->getPointerTo(), APInt(64, (uint64_t)params[1], true)), Types::ExprAST->getPointerTo());
			else
				assert(0);

			Value* valVal = codegenExpr(params[2], parentBlock).value->val;

			Function* addToScopeFunc = MincFunctions::addToFileScope->getFunction(currentModule);
			Value* resultVal = builder->CreateCall(addToScopeFunc, { nameVal, typeVal, valVal });
		}
	);

	/*// Define subscript
	defineExpr2(rootBlock, "$E[$E]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
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
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* var = codegenExpr(params[0], parentBlock).value->val;
			Value* idx = codegenExpr(params[1], parentBlock).value->val;

			Value* gep = builder->CreateInBoundsGEP(var, {
				idx
			}, "gep");

			Variable expr = codegenExpr(params[2], parentBlock);
			builder->CreateStore(expr.value->val, gep);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) -> BaseType* {
			return getType(params[2], parentBlock);
		}
	);*/

	// Define $<LiteralExprAST>.value_ref
	defineExpr2(rootBlock, "$<LiteralExprAST>.value_ref",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* varVal = codegenExpr(params[0], parentBlock).value->val;
			varVal = builder->CreateBitCast(varVal, Types::LiteralExprAST->getPointerTo());

			Function* getLiteralExprASTValueFunc = currentModule->getFunction("getLiteralExprASTValue");
			return Variable(BuiltinTypes::Int8Ptr, new XXXValue(builder->CreateCall(getLiteralExprASTValueFunc, { varVal })));
		},
		BuiltinTypes::Int8Ptr
	);

	// Define variable lookup
	defineExpr3(rootBlock, "$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Variable var = lookupVariable(parentBlock, (IdExprAST*)params[0]);
			if (var.value->isFunction() || isa<Constant>(var.value->val))
				return var;

			LoadInst* loadVal = builder->CreateLoad(var.value->val, getIdExprASTName((IdExprAST*)params[0]));
			loadVal->setAlignment(4);
			return Variable(var.type, new XXXValue(loadVal));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) -> BaseType* {
			bool isCaptured;
			const Variable* var = lookupSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]), isCaptured);
			return var != nullptr ? var->type : nullptr;
		}
	);

	// Define literal definition
	defineExpr3(rootBlock, "$L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);

			if (value[0] == '"' || value[0] == '\'')
			{
				//Constant *valueConstant = ConstantDataArray::getString(*context, std::string(value + 1, strlen(value) - 2));
				Constant *valueConstant = (Constant*)unwrap(LLVMConstString(value + 1, strlen(value) - 2, 0));
				GlobalVariable* valueGlobal = new GlobalVariable(*currentFunc->getParent(), valueConstant->getType(), true,
					GlobalValue::PrivateLinkage, valueConstant, "",
					nullptr, GlobalVariable::NotThreadLocal,
					0
				);
				valueGlobal->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
				valueGlobal->setAlignment(1);
					Constant *zero_32 = Constant::getNullValue(IntegerType::getInt32Ty(*context));
					std::vector<Value *>gep_params = {
						zero_32,
						zero_32
					};
				//return Variable(Type::getInt8PtrTy(*context), new XXXValue(builder->CreateGEP(valueConstant->getType(), valueGlobal, gep_params)));
				return Variable(BuiltinTypes::Int8Ptr, new XXXValue(builder->CreateGEP(valueConstant->getType(), valueGlobal, gep_params)));
			}

			if (strchr(value, '.'))
			{
				double doubleValue = std::stod(value);
				return Variable(BuiltinTypes::Int32, new XXXValue(unwrap(LLVMConstReal(LLVMDoubleType(), doubleValue))));
			}

			int intValue;
			if (value[0] == '0' && value[1] == 'x')
				intValue = std::stoi(value, 0, 16);
			else
				intValue = std::stoi(value, 0, 10);

			return Variable(BuiltinTypes::Int32, new XXXValue(unwrap(LLVMConstInt(LLVMInt32Type(), intValue, 1))));
			//return Variable(Type::getInt32Ty(*context), new XXXValue(ConstantInt::get(*context, APInt(32, value, true))));
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) -> BaseType* {
			const char* value = getLiteralExprASTValue((LiteralExprAST*)params[0]);
			if (value[0] == '"' || value[0] == '\'')
				return BuiltinTypes::Int8Ptr;
			return BuiltinTypes::Int32;
		}
	);

	// Define variable assignment
	defineExpr3(rootBlock, "$I = $E",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Variable var = lookupVariable(parentBlock, (IdExprAST*)params[0]);
			Variable expr = codegenExpr(params[1], parentBlock);

			builder->CreateStore(expr.value->val, var.value->val);
			return expr;
		},
		[](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params) -> BaseType* {
			return getType(params[1], parentBlock);
		}
	);

	defineExpr2(rootBlock, "FuncType($<string>, $<int>, $<BaseType>, $<BaseType>, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* nameVal = codegenExpr(params[0], parentBlock).value->val;
			Value* isVarArgVal = builder->CreateBitCast(codegenExpr(params[1], parentBlock).value->val, Types::Int8);
			Value* resultTypeVal = codegenExpr(params[2], parentBlock).value->val;

			size_t numArgTypes = params.size() - 3;
			AllocaInst* argTypes = builder->CreateAlloca(ArrayType::get(Types::BaseType->getPointerTo(), numArgTypes), nullptr, "argTypes");
			argTypes->setAlignment(8);
			for (size_t i = 0; i < numArgTypes; ++i)
				builder->CreateStore(codegenExpr(params[3 + i], parentBlock).value->val, builder->CreateConstInBoundsGEP2_64(argTypes, 0, i));
			Value* numArgTypesVal = Constant::getIntegerValue(Types::Int32, APInt(32, numArgTypes, true));
			Value* argTypesVal = builder->CreateConstInBoundsGEP2_64(argTypes, 0, 0);

			Function* func = MincFunctions::createFuncType->getFunction(currentModule);
			return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(builder->CreateCall(func, { nameVal, isVarArgVal, resultTypeVal, argTypesVal, numArgTypesVal })));
		},
		BuiltinTypes::LLVMValueRef
	);

	/*defineExpr2(rootBlock, "$<BaseType>.llvmtype",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
			Value* typeVal = codegenExpr(params[0], parentBlock).value->val;

			//TODO
		},
		BuiltinTypes::LLVMTypeRef
	);*/

defineExpr2(rootBlock, "getfunc($E)",
	[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params) -> Variable {
		Variable expr = codegenExpr(params[0], parentBlock);
		LLVMValueRef func = wrap(expr.value->getFunction(currentModule));

		return Variable(BuiltinTypes::LLVMValueRef, new XXXValue(Constant::getIntegerValue(Types::LLVMOpaqueValue->getPointerTo(), APInt(64, (uint64_t)func))));
	},
	BuiltinTypes::LLVMValueRef
);
}