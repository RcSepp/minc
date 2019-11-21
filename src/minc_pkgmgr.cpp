#include <cstring>
#include "api.h"
#include "minc_pkgmgr.h"

MincPackage::MincPackage(const char* name, MincPackageFunc defineFunc)
	: pkgScope(nullptr), defineFunc(defineFunc)
{
	if (name) // Avoid registering MincPackageManager during class construction
	{
		MINC_PACKAGE_MANAGER().registerPackage(name, this);
		const char* perentNameEnd = strrchr(name, '.');
		if (perentNameEnd)
			parentName = std::string(name, perentNameEnd - name);
	}
}

MincPackage::~MincPackage()
{
	if (pkgScope)
		removeBlockExprAST(pkgScope);
}

BlockExprAST* MincPackage::load()
{
	if (pkgScope == nullptr)
	{
		pkgScope = createEmptyBlockExprAST();
		if (parentName.size())
			importBlock(pkgScope, MINC_PACKAGE_MANAGER().loadPackage(parentName));
		this->define(pkgScope);
	}
	return pkgScope;
}

void MincPackage::import(BlockExprAST* scope)
{
	importBlock(scope, load());
}

MincPackageManager& MINC_PACKAGE_MANAGER()
{
	static MincPackageManager pkgmgr;
	return pkgmgr;
}

MincPackageManager::MincPackageManager() : MincPackage(nullptr)
{
	registerPackage("pkgmgr", this); // Manually register MincPackageManager
}

void MincPackageManager::define(BlockExprAST* pkgScope)
{
	defineStmt2(pkgScope, "import $I. ...",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			MincPackageManager* pkgMgr = (MincPackageManager*)stmtArgs;
			std::vector<ExprAST*>& pkgPath = getExprListASTExpressions((ExprListAST*)params[0]);
			std::string pkgName = getIdExprASTName((IdExprAST*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + '.' + getIdExprASTName((IdExprAST*)pkgPath[i]);

			// Import package
			if (!pkgMgr->tryImportPackage(parentBlock, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
		}, this
	);
}

BlockExprAST* MincPackageManager::loadPackage(std::string pkgName) const
{
	auto pkg = packages.find(pkgName);
	return pkg == packages.end() ? nullptr : pkg->second->load();
}

void MincPackageManager::importPackage(BlockExprAST* scope, std::string pkgName) const
{
	auto pkg = packages.find(pkgName);
	if (pkg != packages.end())
		pkg->second->import(scope);
	else
		raiseCompileError(("unknown package " + pkgName).c_str(), (ExprAST*)scope);
}
bool MincPackageManager::tryImportPackage(BlockExprAST* scope, std::string pkgName) const
{
	auto pkg = packages.find(pkgName);
	if (pkg != packages.end())
	{
		pkg->second->import(scope);
		return true;
	}
	else
		return false;
}