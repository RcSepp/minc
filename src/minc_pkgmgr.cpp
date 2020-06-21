#include <cstring>
#include <sstream>
#include "minc_api.h"
#include "minc_pkgmgr.h"

const std::string	PKG_PATH_ENV = "MINC_PATH";
const char			PKG_PATH_ENV_SEPARATOR = ':';
const char			MincPackage::PKG_PATH_SEPARATOR = '.';

void defaultDefineFunc(BlockExprAST* pkgScope)
{
	codegenExpr((ExprAST*)pkgScope, getBlockExprASTParent(pkgScope));
}

MincPackage::MincPackage(const char* name, MincPackageFunc defineFunc, BlockExprAST* defineBlock)
	: defineFunc(defineFunc != nullptr ? defineFunc : defaultDefineFunc), defineBlock(defineBlock != nullptr ? defineBlock : createEmptyBlockExprAST())
{
	// Initialize pkgScope to nullptr
	// Note: If a binary is loaded multiple times, it is possible that a package is constructed multiple times at the same memory location.
	//		 In this case we skip initialization of pkgScope to avoid overwriting the existing package.
	if (name == nullptr || MINC_PACKAGE_MANAGER().registerPackage(name, this)) // If package is MincPackageManager or newly registered, ...
		pkgScope = nullptr;

	if (name) // Avoid registering MincPackageManager during class construction
	{
		setBlockExprASTName(this->defineBlock, name);
		const char* perentNameEnd = strrchr(name, PKG_PATH_SEPARATOR);
		if (perentNameEnd)
			parentName = std::string(name, perentNameEnd - name);
	}
}

MincPackage::~MincPackage()
{
	if (defineBlock != nullptr)
	{
	 	removeBlockExprAST(defineBlock);
		defineBlock = nullptr;
	}
}

BlockExprAST* MincPackage::load(BlockExprAST* importer)
{
	// Avoid circular import dead lock
	if (pkgScope == importer)
		return pkgScope;

	std::unique_lock<std::mutex> lock(loadMutex);
	if (pkgScope == nullptr)
	{
		pkgScope = defineBlock;
		if (parentName.size())
		{
			BlockExprAST* parentPkg = MINC_PACKAGE_MANAGER().loadPackage(parentName, importer);
			if (parentPkg != nullptr)
				importBlock(pkgScope, parentPkg);
			else
				raiseCompileError(("unknown package " + parentName).c_str(), (ExprAST*)importer);
		}
		this->definePackage(pkgScope);
	}
	return pkgScope;
}

void MincPackage::import(BlockExprAST* scope)
{
	importBlock(scope, load(scope));
}

MincPackageManager& MINC_PACKAGE_MANAGER()
{
	static MincPackageManager pkgmgr;
	return pkgmgr;
}

MincPackageManager::MincPackageManager() : MincPackage(nullptr)
{
	// Manually register MincPackageManager
	registerPackage("pkgmgr", this);

	// Read package search paths from PKG_PATH_ENV
	const char* packagePathList = std::getenv(PKG_PATH_ENV.c_str());
	if (packagePathList == nullptr)
		raiseCompileError(("environment variable " + PKG_PATH_ENV + " not set").c_str());
	std::stringstream packagePathsStream(packagePathList);
	std::string searchPath;
	while (std::getline(packagePathsStream, searchPath, PKG_PATH_ENV_SEPARATOR)) // Traverse package paths
	{
		// Skip empty package paths
		if (searchPath.empty())
			continue;

		// Append trailing path separator
		if (searchPath.back() != '/' && searchPath.back() != '\\')
#ifdef _WIN32
			searchPath.push_back('\\');
#else
			searchPath.push_back('/');
#endif

		// Add search path
		pkgSearchPaths.push_back(searchPath);
	}
}

void MincPackageManager::definePackage(BlockExprAST* pkgScope)
{
	setBlockExprASTName(pkgScope, "pkgmgr"); // Manually set MincPackageManager name

	// Define import statement
	defineStmt2(pkgScope, "import $I. ...",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			MincPackageManager* pkgMgr = (MincPackageManager*)stmtArgs;
			std::vector<ExprAST*>& pkgPath = getListExprASTExprs((ListExprAST*)params[0]);
			std::string pkgName = getIdExprASTName((IdExprAST*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + PKG_PATH_SEPARATOR + getIdExprASTName((IdExprAST*)pkgPath[i]);

			// Import package
			if (!pkgMgr->tryImportPackage(parentBlock, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
		}, this
	);

	// Define import statement
	defineStmt2(pkgScope, "import $L",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			MincPackageManager* pkgMgr = (MincPackageManager*)stmtArgs;
			std::string pkgName = getLiteralExprASTValue((LiteralExprAST*)params[0]);

			// Trim '"'
			if (pkgName.back() == '"' || pkgName.back() == '\'')
				pkgName = pkgName.substr(1, pkgName.size() - 2);

			// Import package
			if (!pkgMgr->tryImportPackage(parentBlock, pkgName))
				raiseCompileError(("unknown package " + pkgName).c_str(), params[0]);
		}, this
	);

	// Define export statement
	defineStmt2(pkgScope, "export $I. ... $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::vector<ExprAST*>& pkgPath = getListExprASTExprs((ListExprAST*)params[0]);
			std::string pkgName = getIdExprASTName((IdExprAST*)pkgPath[0]);
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + PKG_PATH_SEPARATOR + getIdExprASTName((IdExprAST*)pkgPath[i]);
			BlockExprAST* exportBlock = (BlockExprAST*)params[1];

			setBlockExprASTParent(exportBlock, parentBlock);

			// Export package
			new MincPackage(pkgName.c_str(), nullptr, exportBlock);
		}
	);
	defineStmt2(pkgScope, "export $I. ...",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::vector<ExprAST*>& pkgPath = getListExprASTExprs((ListExprAST*)params[0]);
			raiseCompileError("Missing export block", pkgPath.empty() ? nullptr : pkgPath.front());
		}
	);
}

bool MincPackageManager::registerPackage(std::string pkgName, MincPackage* package)
{
	return packages.insert(std::make_pair(pkgName, package)).second;
}

BlockExprAST* MincPackageManager::loadPackage(std::string pkgName, BlockExprAST* importer) const
{
	MincPackage* pkg = discoverPackage(pkgName);
	return pkg == nullptr ? nullptr : pkg->load(importer);
}

void MincPackageManager::importPackage(BlockExprAST* scope, std::string pkgName) const
{
	MincPackage* pkg = discoverPackage(pkgName);
	if (pkg != nullptr)
		pkg->import(scope);
	else
		raiseCompileError(("unknown package " + pkgName).c_str(), (ExprAST*)scope);
}
bool MincPackageManager::tryImportPackage(BlockExprAST* scope, std::string pkgName) const
{
	MincPackage* pkg = discoverPackage(pkgName);
	if (pkg != nullptr)
	{
		pkg->import(scope);
		return true;
	}
	else
		return false;
}