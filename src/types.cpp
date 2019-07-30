
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
		t = builtinTypes.insert({ name, new BuiltinType(llvmtype, align) }).first;
	return t->second;
}

BuiltinType* BuiltinType::Ptr()
{
	if (ptr == nullptr)
	{
		const std::string& name = getTypeName(this);
		char* ptrName = new char[name.size() + 3];
		strcpy(ptrName, name.c_str());
		strcpy(ptrName + name.size(), "Ptr");
		ptr = new BuiltinType(LLVMPointerType(llvmtype, 0), 8);
		defineType(nullptr, ptrName, this, new XXXValue(unwrap(ptr->llvmtype), (uint64_t)ptr));
	}
	return ptr;
}

FuncType::FuncType(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg)
		: BuiltinType(nullptr, 8), resultType(resultType), argTypes(argTypes), name(name)
{
	std::vector<llvm::Type*> argLlvmTypes;
	for (BuiltinType* argType: argTypes)
		argLlvmTypes.push_back(unwrap(argType->llvmtype));
	llvmtype = wrap(FunctionType::get(unwrap(resultType->llvmtype), argLlvmTypes, isVarArg));
}

TpltType* TpltType::get(std::string name, BuiltinType* baseType, BaseType* tpltType)
{
	auto t = tpltTypes.find(name);
	if (t == tpltTypes.end())
	{
		t = tpltTypes.insert({ name, new TpltType(baseType, tpltType) }).first;
		defineOpaqueCast(getRootScope(), t->second, baseType);
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