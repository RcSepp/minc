#ifndef __INCLUDE_H
#define __INCLUDE_H

#include "api.h"

namespace llvm {
	class Value;
	class Constant;
	class Function;
}

struct Func;
struct LLVMOpaqueType;

enum Visibility {
	PRIVATE, PUBLIC, PROTECTED
};

struct BuiltinType : public BaseType
{
private:
	static std::map<std::string, BuiltinType*> builtinTypes;
	BuiltinType* ptr;

protected:
	BuiltinType(LLVMOpaqueType* llvmtype, int32_t align, int32_t encoding, int64_t numbits)
		: ptr(nullptr), llvmtype(llvmtype), align(align), encoding(encoding), numbits(numbits) {}

public:
	LLVMOpaqueType* llvmtype;
	int32_t align, encoding;
	int64_t numbits;
	virtual ~BuiltinType() {};

	static BuiltinType* get(const char* name, LLVMOpaqueType* llvmtype, int32_t align, int32_t encoding, int64_t numbits);

	BuiltinType* Ptr();
};

struct FuncType : public BuiltinType
{
	BuiltinType* resultType;
	std::vector<BuiltinType*> argTypes;
	const char* name;

	FuncType(const char* name, BuiltinType* resultType, const std::vector<BuiltinType*>& argTypes, bool isVarArg);
	virtual ~FuncType() {};
};

struct ClassMethod
{
	Visibility visibility;
	Func* func;
};

struct ClassVariable
{
	Visibility visibility;
	BuiltinType* type;
	unsigned int index;
};

struct ClassType : public BuiltinType
{
	BuiltinType* resultType;
	std::multimap<std::string, ClassMethod> methods;
	std::map<std::string, ClassVariable> variables;
	std::vector<ClassMethod> constructors;

	ClassType();
	virtual ~ClassType() {};
};

struct TpltType : public BuiltinType
{
private:
	static std::map<std::string, TpltType*> tpltTypes;
	virtual ~TpltType() {};

protected:
	TpltType(BuiltinType* baseType, BuiltinType* tpltType)
		: BuiltinType(baseType->llvmtype, baseType->align, baseType->encoding, baseType->numbits), tpltType(tpltType) {}

public:
	BuiltinType* tpltType;

	static TpltType* get(std::string name, BuiltinType* baseType, BuiltinType* tpltType);
};

struct XXXValue : BaseValue
{
private:
	uint64_t constantValue;

public:
	llvm::Value* val;

	XXXValue(llvm::Value* val)
		: val(val), constantValue(0xFFFFFFFFFFFFFFFF) {}
	XXXValue(llvm::Type* type, uint64_t value)
		: val(type == nullptr ? nullptr : llvm::Constant::getIntegerValue(type, llvm::APInt(64, value))), constantValue(value) {}

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

	bool isConstant()
	{
		return constantValue != 0xFFFFFFFFFFFFFFFF;//llvm::isa<llvm::Constant>(val);
	}
};

struct Func : XXXValue
{
public:
	FuncType type;
	const char* symName;

	Func(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg, const char* symName = nullptr)
		: XXXValue(nullptr), type(name, resultType, argTypes, isVarArg), symName(symName ? symName : name) {}

	llvm::Function* getFunction(llvm::Module* module);

	bool isFunction()
	{
		return true;
	}	
};

#endif