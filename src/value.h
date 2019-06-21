struct XXXValue
{
private:
	const char* funcname;
	uint64_t constantValue;

public:
	llvm::Type* type;
	union {
		llvm::Value* val;
		llvm::Function* func;
	};

	XXXValue(llvm::Value* val, llvm::Type* type = nullptr)
		: type(type ? type : val->getType()), val(val), funcname(nullptr), constantValue(0) {}
	XXXValue(llvm::Function* func)
		: type(func->getFunctionType()), func(func), funcname(nullptr), constantValue(0) {}
	XXXValue(llvm::FunctionType* functype, const char* funcname)
		: type(functype), func(nullptr), funcname(funcname), constantValue(0) {}
	XXXValue(llvm::Type* type, uint64_t value)
		: type(type), val(llvm::Constant::getIntegerValue(Types::BuiltinType, llvm::APInt(64, value))), funcname(nullptr), constantValue(value) {}

	uint64_t getConstantValue()
	{
		return constantValue;
	}

	llvm::Function* getFunction(llvm::Module* module)
	{
		if (func)
			return func;
		return func = Function::Create((llvm::FunctionType*)type, GlobalValue::ExternalLinkage, funcname, module);
	}
};