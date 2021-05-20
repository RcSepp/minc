#include <stack>
#include <threads.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

extern llvm::LLVMContext context;
extern llvm::IRBuilder<> builder;

namespace llvm
{
	class ExecutionEngine;
	namespace legacy
	{
		class FunctionPassManager;
		class PassManager;
	}
}

class Module : public llvm::Module
{
	std::string name;
	llvm::legacy::FunctionPassManager* funcPassMgr;
	llvm::legacy::PassManager* modulePassMgr;
	std::vector<llvm::Function*> functions;
	std::set<::Module*> dependencies;
	llvm::ExecutionEngine* executionEngine;

public:
	explicit Module(llvm::StringRef ModuleID, llvm::LLVMContext& C);
	~Module();
	llvm::Function* createFunction(llvm::FunctionType *Ty, llvm::GlobalValue::LinkageTypes Linkage, const llvm::Twine &N = "");
	void finalize();
	void dump();
	uint64_t getFunctionAddress(const std::string& name);
	void addDependency(::Module* module);
};

extern class LlvmRunner : public MincRunner
{
private:
	typedef void (*MainFunction)(MincInteropData&);
	MainFunction mincMain;

public:
	::Module* module;
	llvm::GlobalVariable* mincInteropDataValue;

	llvm::StructType* mincRunnerType;
	llvm::StructType* mincExprType;
	llvm::StructType* mincBlockExprType;
	llvm::StructType* mincObjectType;
	llvm::StructType* mincRuntimeType;
	llvm::StructType* mincKernelType;
	llvm::StructType* mincStackSymbolType;
	llvm::StructType* mincEnteredBlockExprType;
	llvm::StructType* mincInteropDataType;

	llvm::Function* handoverFunc;
	llvm::Function* runExprFunc;
	llvm::Function* getStackSymbolFunc;
	llvm::Function* enterBlockExprFunc;
	llvm::Function* exitBlockExprFunc;

	class ExprSwitch
	{
	private:
		llvm::SwitchInst* switchInst;
		llvm::BasicBlock* switchBlock;
		llvm::BasicBlock* defaultBlock;
		llvm::BasicBlock* prevBlock;
		llvm::GlobalVariable* mincInteropDataValue;

		void beginExprSwitch();
		void endExprSwitch();

	public:
		ExprSwitch(llvm::GlobalVariable* mincInteropDataValue);
		~ExprSwitch();
		void beginCast(MincExpr* expr);
		void endCast();
	};
	std::stack<ExprSwitch> exprSwitchStack;

	LlvmRunner();

	void buildBegin(MincBuildtime& buildtime);
	void buildEnd(MincBuildtime& buildtime);
	void buildBeginFile(MincBuildtime& buildtime, const char* path);
	void buildEndFile(MincBuildtime& buildtime, const char* path);
	void buildStmt(MincBuildtime& buildtime, MincStmt* stmt);
	void buildSuspendStmt(MincBuildtime& buildtime, MincStmt* stmt);
	void buildResumeStmt(MincBuildtime& buildtime, MincStmt* stmt);
	void buildSuspendExpr(MincBuildtime& buildtime, MincExpr* expr);
	void buildResumeExpr(MincBuildtime& buildtime, MincExpr* expr);
	void buildNestedExpr(MincBuildtime& buildtime, MincExpr* expr, MincRunner& next);
	int run(MincExpr* expr, MincInteropData& interopData);
} LLVM_RUNNER;