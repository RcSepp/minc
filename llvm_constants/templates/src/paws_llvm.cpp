#include <map>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include "llvm-c/ExecutionEngine.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

using namespace llvm;

extern LLVMContext* context;
extern IRBuilder<>* builder;
extern Module* currentModule;
extern BasicBlock* currentBB;
extern DIBuilder* dbuilder;

extern "C"
{
@	MODIFIED_LLVM_EXTERN_FUNC_DECL@
	void LLVMEXPositionBuilder(LLVMBasicBlockRef Block);
	LLVMValueRef LLVMEXBuildInBoundsGEP1(LLVMValueRef Pointer, LLVMValueRef Idx0, const char *Name);
	LLVMValueRef LLVMEXBuildInBoundsGEP2(LLVMValueRef Pointer, LLVMValueRef Idx0, LLVMValueRef Idx1, const char *Name);
	LLVMValueRef LLVMEXConstInBoundsGEP1(LLVMValueRef ConstantVal, LLVMValueRef Idx0);
	LLVMValueRef LLVMEXConstInBoundsGEP2(LLVMValueRef ConstantVal, LLVMValueRef Idx0, LLVMValueRef Idx1);
	LLVMMetadataRef LLVMEXDIBuilderCreateExpression();
	LLVMMetadataRef LLVMEXDIBuilderCreateDebugLocation(unsigned Line, unsigned Column, LLVMMetadataRef Scope);
}

typedef PawsValue<void*> PawsVoidPtr;
typedef PawsValue<int*> PawsIntArray;
typedef PawsValue<std::string*> PawsStringArray;

typedef PawsValue<LLVMPassRegistryRef> PawsLLVMPassRegistryRef;
typedef PawsValue<LLVMContextRef> PawsLLVMContextRef;
typedef PawsValue<LLVMDiagnosticInfoRef> PawsLLVMDiagnosticInfoRef;
typedef PawsValue<LLVMAttributeRef> PawsLLVMAttributeRef;
typedef PawsValue<LLVMAttributeRef*> PawsLLVMAttributeRefArray;
typedef PawsValue<LLVMAttributeRef**> PawsLLVMAttributeRefArrayArray;
typedef PawsValue<LLVMModuleRef> PawsLLVMModuleRef;
typedef PawsValue<LLVMValueMetadataEntry*> PawsLLVMValueMetadataEntryRef;
typedef PawsValue<LLVMMetadataRef> PawsLLVMMetadataRef;
typedef PawsValue<LLVMMetadataRef*> PawsLLVMMetadataRefArray;
typedef PawsValue<LLVMValueRef> PawsLLVMValueRef;
typedef PawsValue<LLVMValueRef*> PawsLLVMValueRefArray;
typedef PawsValue<LLVMTypeRef> PawsLLVMTypeRef;
typedef PawsValue<LLVMTypeRef*> PawsLLVMTypeRefArray;
typedef PawsValue<LLVMModuleFlagEntry*> PawsLLVMModuleFlagEntryRef;
typedef PawsValue<LLVMUseRef> PawsLLVMUseRef;
typedef PawsValue<LLVMNamedMDNodeRef> PawsLLVMNamedMDNodeRef;
typedef PawsValue<LLVMBasicBlockRef> PawsLLVMBasicBlockRef;
typedef PawsValue<LLVMBasicBlockRef*> PawsLLVMBasicBlockRefArray;
typedef PawsValue<LLVMBuilderRef> PawsLLVMBuilderRef;
typedef PawsValue<LLVMModuleProviderRef> PawsLLVMModuleProviderRef;
typedef PawsValue<LLVMMemoryBufferRef> PawsLLVMMemoryBufferRef;
typedef PawsValue<LLVMMemoryBufferRef*> PawsLLVMMemoryBufferRefArray;
typedef PawsValue<LLVMPassManagerRef> PawsLLVMPassManagerRef;
typedef PawsValue<LLVMDIBuilderRef> PawsLLVMDIBuilderRef;
typedef PawsValue<LLVMExecutionEngineRef> PawsLLVMExecutionEngineRef;
typedef PawsValue<LLVMExecutionEngineRef*> PawsLLVMExecutionEngineRefArray;
typedef PawsValue<LLVMGenericValueRef> PawsLLVMGenericValueRef;
typedef PawsValue<LLVMTargetDataRef> PawsLLVMTargetDataRef;
typedef PawsValue<LLVMTargetMachineRef> PawsLLVMTargetMachineRef;
typedef PawsValue<LLVMJITEventListenerRef> PawsLLVMJITEventListenerRef;

template<> struct PawsValue<char*> : BaseValue
{
private:
	std::string val;

public:
	typedef char* CType;
	static inline PawsType* TYPE = PawsValue<std::string>::TYPE;
	PawsValue() {}
	PawsValue(char* val) : val(val) {}
	char* get() { return const_cast<char*>(val.c_str()); } //TODO: Remove const_cast
	void set(char* val) { this->val = std::string(val); }
};
template<> struct PawsValue<const char*> : BaseValue
{
private:
	std::string val;

public:
	typedef const char* CType;
	static inline PawsType* TYPE = PawsValue<std::string>::TYPE;
	PawsValue() {}
	PawsValue(const char* val) : val(val) {}
	const char* get() { return val.c_str(); }
	void set(const char* val) { this->val = std::string(val); }
};

template<> struct PawsValue<uint32_t> : BaseValue
{
private:
	int val;

public:
	typedef uint32_t CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const uint32_t val) : val(val) {}
	uint32_t get() { return (uint32_t)val; }
	void set(uint32_t val) { this->val = (int)val; }
};
template<> struct PawsValue<uint64_t> : BaseValue
{
private:
	int val;

public:
	typedef uint64_t CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const uint64_t val) : val(val) {}
	uint64_t get() { return (uint64_t)val; }
	void set(uint64_t val) { this->val = (int)val; }
};
template<> struct PawsValue<unsigned long long> : BaseValue
{
private:
	int val;

public:
	typedef unsigned long long CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const unsigned long long val) : val(val) {}
	unsigned long long get() { return (unsigned long long)val; }
	void set(unsigned long long val) { this->val = (int)val; }
};
template<> struct PawsValue<long long> : BaseValue
{
private:
	int val;

public:
	typedef long long CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const long long val) : val(val) {}
	long long get() { return (long long)val; }
	void set(long long val) { this->val = (int)val; }
};

template<> struct PawsValue<LLVMOpcode> : BaseValue
{
private:
	int val;

public:
	typedef LLVMOpcode CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMOpcode val) : val(val) {}
	LLVMOpcode get() { return (LLVMOpcode)val; }
	void set(LLVMOpcode val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMTypeKind> : BaseValue
{
private:
	int val;

public:
	typedef LLVMTypeKind CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMTypeKind val) : val(val) {}
	LLVMTypeKind get() { return (LLVMTypeKind)val; }
	void set(LLVMTypeKind val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMLinkage> : BaseValue
{
private:
	int val;

public:
	typedef LLVMLinkage CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMLinkage val) : val(val) {}
	LLVMLinkage get() { return (LLVMLinkage)val; }
	void set(LLVMLinkage val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMVisibility> : BaseValue
{
private:
	int val;

public:
	typedef LLVMVisibility CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMVisibility val) : val(val) {}
	LLVMVisibility get() { return (LLVMVisibility)val; }
	void set(LLVMVisibility val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMUnnamedAddr> : BaseValue
{
private:
	int val;

public:
	typedef LLVMUnnamedAddr CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMUnnamedAddr val) : val(val) {}
	LLVMUnnamedAddr get() { return (LLVMUnnamedAddr)val; }
	void set(LLVMUnnamedAddr val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMDLLStorageClass> : BaseValue
{
private:
	int val;

public:
	typedef LLVMDLLStorageClass CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMDLLStorageClass val) : val(val) {}
	LLVMDLLStorageClass get() { return (LLVMDLLStorageClass)val; }
	void set(LLVMDLLStorageClass val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMCallConv> : BaseValue
{
private:
	int val;

public:
	typedef LLVMCallConv CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMCallConv val) : val(val) {}
	LLVMCallConv get() { return (LLVMCallConv)val; }
	void set(LLVMCallConv val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMValueKind> : BaseValue
{
private:
	int val;

public:
	typedef LLVMValueKind CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMValueKind val) : val(val) {}
	LLVMValueKind get() { return (LLVMValueKind)val; }
	void set(LLVMValueKind val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMIntPredicate> : BaseValue
{
private:
	int val;

public:
	typedef LLVMIntPredicate CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMIntPredicate val) : val(val) {}
	LLVMIntPredicate get() { return (LLVMIntPredicate)val; }
	void set(LLVMIntPredicate val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMRealPredicate> : BaseValue
{
private:
	int val;

public:
	typedef LLVMRealPredicate CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMRealPredicate val) : val(val) {}
	LLVMRealPredicate get() { return (LLVMRealPredicate)val; }
	void set(LLVMRealPredicate val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMLandingPadClauseTy> : BaseValue
{
private:
	int val;

public:
	typedef LLVMLandingPadClauseTy CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMLandingPadClauseTy val) : val(val) {}
	LLVMLandingPadClauseTy get() { return (LLVMLandingPadClauseTy)val; }
	void set(LLVMLandingPadClauseTy val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMThreadLocalMode> : BaseValue
{
private:
	int val;

public:
	typedef LLVMThreadLocalMode CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMThreadLocalMode val) : val(val) {}
	LLVMThreadLocalMode get() { return (LLVMThreadLocalMode)val; }
	void set(LLVMThreadLocalMode val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMAtomicOrdering> : BaseValue
{
private:
	int val;

public:
	typedef LLVMAtomicOrdering CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMAtomicOrdering val) : val(val) {}
	LLVMAtomicOrdering get() { return (LLVMAtomicOrdering)val; }
	void set(LLVMAtomicOrdering val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMAtomicRMWBinOp> : BaseValue
{
private:
	int val;

public:
	typedef LLVMAtomicRMWBinOp CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMAtomicRMWBinOp val) : val(val) {}
	LLVMAtomicRMWBinOp get() { return (LLVMAtomicRMWBinOp)val; }
	void set(LLVMAtomicRMWBinOp val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMDiagnosticSeverity> : BaseValue
{
private:
	int val;

public:
	typedef LLVMDiagnosticSeverity CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMDiagnosticSeverity val) : val(val) {}
	LLVMDiagnosticSeverity get() { return (LLVMDiagnosticSeverity)val; }
	void set(LLVMDiagnosticSeverity val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMInlineAsmDialect> : BaseValue
{
private:
	int val;

public:
	typedef LLVMInlineAsmDialect CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMInlineAsmDialect val) : val(val) {}
	LLVMInlineAsmDialect get() { return (LLVMInlineAsmDialect)val; }
	void set(LLVMInlineAsmDialect val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMModuleFlagBehavior> : BaseValue
{
private:
	int val;

public:
	typedef LLVMModuleFlagBehavior CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMModuleFlagBehavior val) : val(val) {}
	LLVMModuleFlagBehavior get() { return (LLVMModuleFlagBehavior)val; }
	void set(LLVMModuleFlagBehavior val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMDIFlags> : BaseValue
{
private:
	int val;

public:
	typedef LLVMDIFlags CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMDIFlags val) : val(val) {}
	LLVMDIFlags get() { return (LLVMDIFlags)val; }
	void set(LLVMDIFlags val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMDWARFSourceLanguage> : BaseValue
{
private:
	int val;

public:
	typedef LLVMDWARFSourceLanguage CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMDWARFSourceLanguage val) : val(val) {}
	LLVMDWARFSourceLanguage get() { return (LLVMDWARFSourceLanguage)val; }
	void set(LLVMDWARFSourceLanguage val) { this->val = (int)val; }
};
template<> struct PawsValue<LLVMDWARFEmissionKind> : BaseValue
{
private:
	int val;

public:
	typedef LLVMDWARFEmissionKind CType;
	static inline PawsType* TYPE = PawsValue<int>::TYPE;
	PawsValue() {}
	PawsValue(const LLVMDWARFEmissionKind val) : val(val) {}
	LLVMDWARFEmissionKind get() { return (LLVMDWARFEmissionKind)val; }
	void set(LLVMDWARFEmissionKind val) { this->val = (int)val; }
};


MincPackage PAWS_LLVM("paws.llvm", [](BlockExprAST* pkgScope) {
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");

	registerType<PawsVoidPtr>(pkgScope, "PawsVoidPtr");
	registerType<PawsIntArray>(pkgScope, "PawsIntArray");
	registerType<PawsStringArray>(pkgScope, "PawsStringArray");

registerType<PawsValue<unsigned*>>(pkgScope, "PawsIntPtr");
registerType<PawsValue<const unsigned*>>(pkgScope, "ConstPawsIntPtr");

	registerType<PawsLLVMPassRegistryRef>(pkgScope, "PawsLLVMPassRegistryRef");
	registerType<PawsLLVMContextRef>(pkgScope, "PawsLLVMContextRef");
	registerType<PawsLLVMDiagnosticInfoRef>(pkgScope, "PawsLLVMDiagnosticInfoRef");
	registerType<PawsLLVMAttributeRef>(pkgScope, "PawsLLVMAttributeRef");
	registerType<PawsLLVMAttributeRefArray>(pkgScope, "PawsLLVMAttributeRefArray");
	registerType<PawsLLVMAttributeRefArrayArray>(pkgScope, "PawsLLVMAttributeRefArrayArray");
	registerType<PawsLLVMModuleRef>(pkgScope, "PawsLLVMModuleRef");
	registerType<PawsLLVMValueMetadataEntryRef>(pkgScope, "PawsLLVMValueMetadataEntryRef");
	registerType<PawsLLVMMetadataRef>(pkgScope, "PawsLLVMMetadataRef");
	registerType<PawsLLVMMetadataRefArray>(pkgScope, "PawsLLVMMetadataRefArray");
	registerType<PawsLLVMValueRef>(pkgScope, "PawsLLVMValueRef");
	registerType<PawsLLVMValueRefArray>(pkgScope, "PawsLLVMValueRefArray");
	registerType<PawsLLVMTypeRef>(pkgScope, "PawsLLVMTypeRef");
	registerType<PawsLLVMTypeRefArray>(pkgScope, "PawsLLVMTypeRefArray");
	registerType<PawsLLVMModuleFlagEntryRef>(pkgScope, "PawsLLVMModuleFlagEntryRef");
	registerType<PawsLLVMUseRef>(pkgScope, "PawsLLVMUseRef");
	registerType<PawsLLVMNamedMDNodeRef>(pkgScope, "PawsLLVMNamedMDNodeRef");
	registerType<PawsLLVMBasicBlockRef>(pkgScope, "PawsLLVMBasicBlockRef");
	registerType<PawsLLVMBasicBlockRefArray>(pkgScope, "PawsLLVMBasicBlockRefArray");
	registerType<PawsLLVMBuilderRef>(pkgScope, "PawsLLVMBuilderRef");
	registerType<PawsLLVMModuleProviderRef>(pkgScope, "PawsLLVMModuleProviderRef");
	registerType<PawsLLVMMemoryBufferRef>(pkgScope, "PawsLLVMMemoryBufferRef");
	registerType<PawsLLVMMemoryBufferRefArray>(pkgScope, "PawsLLVMMemoryBufferRefArray");
	registerType<PawsLLVMPassManagerRef>(pkgScope, "PawsLLVMPassManagerRef");
	registerType<PawsLLVMDIBuilderRef>(pkgScope, "PawsLLVMDIBuilderRef");
	registerType<PawsLLVMExecutionEngineRef>(pkgScope, "PawsLLVMExecutionEngineRef");
	registerType<PawsLLVMExecutionEngineRefArray>(pkgScope, "PawsLLVMExecutionEngineRefArray");
	registerType<PawsLLVMGenericValueRef>(pkgScope, "PawsLLVMGenericValueRef");
	registerType<PawsLLVMTargetDataRef>(pkgScope, "PawsLLVMTargetDataRef");
	registerType<PawsLLVMTargetMachineRef>(pkgScope, "PawsLLVMTargetMachineRef");
	registerType<PawsLLVMJITEventListenerRef>(pkgScope, "PawsLLVMJITEventListenerRef");

	defineExpr2(pkgScope, "[ $E<PawsLLVMTypeRef>, ... ]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& elements = getExprListASTExpressions((ExprListAST*)params[0]);
			LLVMTypeRef* arr = new LLVMTypeRef[elements.size()];
			for (size_t i = 0; i < elements.size(); ++i)
				arr[i] = ((PawsValue<LLVMTypeRef>*)codegenExpr(elements[i], parentBlock).value)->get();
			return Variable(PawsValue<LLVMTypeRef*>::TYPE, new PawsValue<LLVMTypeRef*>(arr));
		},
		PawsValue<LLVMTypeRef*>::TYPE
	);

	defineExpr2(pkgScope, "[ $E<PawsLLVMMetadataRef>, ... ]",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			std::vector<ExprAST*>& elements = getExprListASTExpressions((ExprListAST*)params[0]);
			LLVMMetadataRef* arr = new LLVMMetadataRef[elements.size()];
			for (size_t i = 0; i < elements.size(); ++i)
				arr[i] = ((PawsValue<LLVMMetadataRef>*)codegenExpr(elements[i], parentBlock).value)->get();
			return Variable(PawsValue<LLVMMetadataRef*>::TYPE, new PawsValue<LLVMMetadataRef*>(arr));
		},
		PawsValue<LLVMMetadataRef*>::TYPE
	);

	std::map<std::string, PawsRegularFunc> llvmFunctions;
@	PAWS_LLVM_EXTERN_FUNC_DEF@
	defineExternFunction(pkgScope, "LLVMEXPositionBuilder", LLVMEXPositionBuilder);
	defineExternFunction(pkgScope, "LLVMEXBuildInBoundsGEP1", LLVMEXBuildInBoundsGEP1);
	defineExternFunction(pkgScope, "LLVMEXBuildInBoundsGEP2", LLVMEXBuildInBoundsGEP2);
	defineExternFunction(pkgScope, "LLVMEXConstInBoundsGEP1", LLVMEXConstInBoundsGEP1);
	defineExternFunction(pkgScope, "LLVMEXConstInBoundsGEP2", LLVMEXConstInBoundsGEP2);
	defineExternFunction(pkgScope, "LLVMEXDIBuilderCreateExpression", LLVMEXDIBuilderCreateExpression);
	defineExternFunction(pkgScope, "LLVMEXDIBuilderCreateDebugLocation", LLVMEXDIBuilderCreateDebugLocation);
});