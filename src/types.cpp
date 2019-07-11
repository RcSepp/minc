
#include <cstring>
#include <map>
#include <vector>

#include <llvm-c/Core.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>

#include "api.h"
#include "types.h"

using namespace llvm;

std::map<std::string, BuiltinType*> BuiltinType::builtinTypes;
std::map<std::string, TpltType*> TpltType::tpltTypes;

BuiltinType* BuiltinType::get(const char* name, LLVMOpaqueType* llvmtype, int32_t align)
{
	std::string hash = std::string(name);
	auto t = builtinTypes.find(hash);
	if (t == builtinTypes.end())
		t = builtinTypes.insert({ hash, new BuiltinType(name, llvmtype, align) }).first;
	return t->second;
}

BuiltinType* BuiltinType::Ptr()
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

FuncType::FuncType(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg)
		: BuiltinType(name, nullptr, 8), resultType(resultType), argTypes(argTypes)
{
	std::vector<llvm::Type*> argLlvmTypes;
	for (BuiltinType* argType: argTypes)
		argLlvmTypes.push_back(unwrap(argType->llvmtype));
	llvmtype = wrap(FunctionType::get(unwrap(resultType->llvmtype), argLlvmTypes, isVarArg));
}

TpltType* TpltType::get(const char* name, LLVMOpaqueType* llvmtype, int32_t align, BuiltinType* tpltType)
{
	std::string hash = std::string(name) + '<' + std::string(tpltType->name) + '>';
	auto t = tpltTypes.find(hash);
	if (t == tpltTypes.end())
	{
		char* tpltName = new char[hash.size() + 1];
		strcpy(tpltName, hash.c_str());
		t = tpltTypes.insert({ hash, new TpltType(tpltName, llvmtype, align, tpltType) }).first;
		defineOpaqueCast(getRootScope(), t->second, BuiltinType::get(name, llvmtype, align));
	}
	return t->second;
}

llvm::Function* Func::getFunction(llvm::Module* module)
{
	/*if (!val)
		val = Function::Create((llvm::FunctionType*)unwrap(type.llvmtype), GlobalValue::ExternalLinkage, symName, module);
	return (llvm::Function*)val;*/
	val = module->getFunction(symName);
	if (val == nullptr)
		val = Function::Create((llvm::FunctionType*)unwrap(type.llvmtype), GlobalValue::ExternalLinkage, symName, module);
	return (llvm::Function*)val;
}