
#include <cstring>
#include <map>
#include <vector>

#include <llvm-c/Core.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>

#include "types.h"

using namespace llvm;

extern LLVMContext* context;

std::mutex BuiltinType::mutex;
std::map<std::string, BuiltinType*> BuiltinType::builtinTypes;
std::mutex TpltType::mutex;
std::map<std::string, TpltType*> TpltType::tpltTypes;

BuiltinType* BuiltinType::get(const char* name, LLVMOpaqueType* llvmtype, int32_t align, int32_t encoding, int64_t numbits)
{
	std::unique_lock<std::mutex> lock(mutex);
	std::string hash = std::string(name);
	auto t = builtinTypes.find(hash);
	if (t == builtinTypes.end())
	{
		t = builtinTypes.insert({ name, new BuiltinType(llvmtype, align, encoding, numbits) }).first;
		defineType(name, t->second);
	}
	return t->second;
}

PtrType* BuiltinType::Ptr()
{
	if (ptr == nullptr)
	{
		const std::string& name = getTypeName(this);
		char* ptrName = new char[name.size() + 3];
		strcpy(ptrName, name.c_str());
		strcpy(ptrName + name.size(), "Ptr");
		ptr = new PtrType(this);
		defineType(ptrName, ptr);
	}
	return ptr;
}

PtrType::PtrType(BuiltinType* pointeeType)
	: BuiltinType(pointeeType == nullptr ? nullptr : LLVMPointerType(pointeeType->llvmtype, 0), 8, dwarf::DW_ATE_address, 64), pointeeType(pointeeType) {}

FuncType::FuncType(const char* funcName, BuiltinType* resultType, const std::vector<BuiltinType*>& argTypes, bool isVarArg)
	: BuiltinType(nullptr, 8, dwarf::DW_ATE_address, 64), resultType(resultType), argTypes(argTypes)
{
	std::string name = funcName;
	name += '(';
	size_t i;
	for (size_t i = 0; i < argTypes.size(); ++i)
	{
		if (i) name += ", ";
		name += getTypeName(argTypes[i]);
	}
	name += ')';
	defineType(name.c_str(), this);

	std::vector<llvm::Type*> argLlvmTypes;
	for (BuiltinType* argType: argTypes)
		argLlvmTypes.push_back(unwrap(argType->llvmtype));
	llvmtype = wrap(FunctionType::get(unwrap(resultType->llvmtype), argLlvmTypes, isVarArg));
}

ClassType::ClassType()
	: BuiltinType(wrap(StructType::create(*context)), 4, 0, 0) {}

TpltType* TpltType::get(std::string name, BuiltinType* baseType, BuiltinType* tpltType)
{
	std::unique_lock<std::mutex> lock(mutex);
	auto t = tpltTypes.find(name);
	if (t == tpltTypes.end())
	{
		t = tpltTypes.insert({ name, new TpltType(baseType, tpltType) }).first;
		defineType(name.c_str(), t->second);
		defineOpaqueInheritanceCast(getRootScope(), t->second, baseType);
	}
	return t->second;
}

llvm::Function* Func::getFunction(llvm::Module* module)
{
	/*if (!val)
		val = Function::Create((llvm::FunctionType*)unwrap(type.llvmtype), GlobalValue::ExternalLinkage, symName, module);
	return val;*/
	val = module->getFunction(symName);
	if (val == nullptr)
		val = Function::Create((llvm::FunctionType*)unwrap(type->llvmtype), GlobalValue::ExternalLinkage, symName, module);
	return val;
}