namespace llvm {
	class Value;
	class Function;
}

struct LLVMOpaqueType;

struct BaseType {};

struct BuiltinType : public BaseType
{
private:
	static std::map<std::string, BuiltinType*> builtinTypes;
	BuiltinType* ptr;

protected:
	BuiltinType(LLVMOpaqueType* llvmtype, int32_t align)
		: ptr(nullptr), llvmtype(llvmtype), align(align) {}

public:
	LLVMOpaqueType* llvmtype;
	int32_t align;
	virtual ~BuiltinType() {};

	static BuiltinType* get(const char* name, LLVMOpaqueType* llvmtype, int32_t align);

	BuiltinType* Ptr();
};

struct FuncType : public BuiltinType
{
	BuiltinType* resultType;
	std::vector<BuiltinType*> argTypes;
	const char* name;

	FuncType(const char* name, BuiltinType* resultType, std::vector<BuiltinType*> argTypes, bool isVarArg);
	virtual ~FuncType() {};
};

struct TpltType : public BuiltinType
{
private:
	static std::map<std::string, TpltType*> tpltTypes;
	virtual ~TpltType() {};

protected:
	TpltType(BuiltinType* baseType, BaseType* tpltType)
		: BuiltinType(baseType->llvmtype, baseType->align), tpltType(tpltType) {}

public:
	BaseType* tpltType;

	static TpltType* get(std::string name, BuiltinType* baseType, BaseType* tpltType);
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

	llvm::Function* getFunction(llvm::Module* module);

	bool isFunction()
	{
		return true;
	}	
};