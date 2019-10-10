#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"

PawsPackage::PawsPackage(const char* name, PawsPackageFunc defineFunc)
	: pkgScope(nullptr), defineFunc(defineFunc)
{
	if (name) // Avoid registering PawsPackageManager during class construction
		PAWS_PACKAGE_MANAGER().registerPackage(name, this);
}

PawsPackage::~PawsPackage()
{
	if (pkgScope)
		removeBlockExprAST(pkgScope);
}

void PawsPackage::import(BlockExprAST* scope)
{
	if (pkgScope == nullptr)
	{
		pkgScope = createEmptyBlockExprAST();
		this->define(pkgScope);
	}
	importBlock(scope, pkgScope);
}

PawsPackageManager& PAWS_PACKAGE_MANAGER()
{
	static PawsPackageManager pkgmgr;
	return pkgmgr;
}

PawsPackageManager::PawsPackageManager() : PawsPackage(nullptr)
{
	registerPackage("pkgmgr", this); // Manually register PawsPackageManager
}

void PawsPackageManager::define(BlockExprAST* pkgScope)
{
	defineStmt2(pkgScope, "import paws.$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsPackageManager* pkgMgr = (PawsPackageManager*)stmtArgs;
			std::string pkgName = getIdExprASTName((IdExprAST*)params[0]);
			auto pck = pkgMgr->packages.find(pkgName);
			if (pck == pkgMgr->packages.end())
				raiseCompileError(("unknown paws package " + pkgName).c_str(), params[0]);
			pck->second->import(parentBlock);
		}, this
	);

	defineExpr2(pkgScope, "$E<PawsBlockExprAST>.import(paws.$I)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			BlockExprAST* block = ((PawsBlockExprAST*)codegenExpr(params[0], parentBlock).value)->val;
			PawsPackageManager* pkgMgr = (PawsPackageManager*)exprArgs;
			std::string pkgName = getIdExprASTName((IdExprAST*)params[1]);
			auto pck = pkgMgr->packages.find(pkgName);
			if (pck == pkgMgr->packages.end())
				raiseCompileError(("unknown paws package " + pkgName).c_str(), params[1]);
			pck->second->import(block);
			return Variable(PawsVoid::TYPE, nullptr);
		}, PawsVoid::TYPE, this
	);
}