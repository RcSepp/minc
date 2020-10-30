#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "minc_api.hpp"
#include "minc_pkgmgr.h"
#include "json.h"

const std::string	PKG_PATH_ENV = "MINC_PATH";
const std::string	EXT_PATH_ENV = "MINC_EXT";
const char			PATH_ENV_SEPARATOR = ':';
const char			MincPackage::PKG_PATH_SEPARATOR = '.';

void defaultDefineFunc(MincBlockExpr* pkgScope)
{
	pkgScope->build(pkgScope->parent);
	pkgScope->codegen(pkgScope->parent);
}

MincPackage::MincPackage(const char* name, MincPkgFunc defineFunc, MincBlockExpr* defineBlock)
	: defineFunc(defineFunc != nullptr ? defineFunc : defaultDefineFunc), defineBlock(defineBlock != nullptr ? defineBlock : new MincBlockExpr({0}, {}))
{
	// Initialize pkgScope to nullptr
	// Note: If a binary is loaded multiple times, it is possible that a package is constructed multiple times at the same memory location.
	//		 In this case we skip initialization of pkgScope to avoid overwriting the existing package.
	if (name == nullptr || MINC_PACKAGE_MANAGER().registerPackage(name, this)) // If package is MincPackageManager or newly registered, ...
		pkgScope = nullptr;

	if (name) // Avoid registering MincPackageManager during class construction
	{
		this->defineBlock->name = name;
		const char* perentNameEnd = strrchr(name, PKG_PATH_SEPARATOR);
		if (perentNameEnd)
			parentName = std::string(name, perentNameEnd - name);
	}
}

MincPackage::~MincPackage()
{
	if (defineBlock != nullptr)
	{
	 	delete defineBlock;
		defineBlock = nullptr;
	}
}

MincBlockExpr* MincPackage::load(MincBlockExpr* importer)
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
			MincBlockExpr* parentPkg = MINC_PACKAGE_MANAGER().loadPackage(parentName, importer);
			if (parentPkg != nullptr)
				pkgScope->import(parentPkg);
			else
				throw CompileError("unknown package " + parentName, importer->loc);
		}
		this->definePackage(pkgScope);
		for (MincPackageManager::PostDefineHook hook: MINC_PACKAGE_MANAGER().postDefineHooks)
			hook(pkgScope);
	}
	return pkgScope;
}

void MincPackage::import(MincBlockExpr* scope)
{
	scope->import(load(scope));
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
		throw CompileError("environment variable " + PKG_PATH_ENV + " not set");
	std::stringstream packagePathsStream(packagePathList);
	std::string searchPath;
	while (std::getline(packagePathsStream, searchPath, PATH_ENV_SEPARATOR)) // Traverse package paths
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

	// Read extension search paths from EXT_PATH_ENV
	const char* extensionPathList = std::getenv(EXT_PATH_ENV.c_str());
	if (extensionPathList == nullptr)
		throw CompileError("environment variable " + EXT_PATH_ENV + " not set");
	std::stringstream extensionPathsStream(extensionPathList);
	while (std::getline(extensionPathsStream, searchPath, PATH_ENV_SEPARATOR)) // Traverse extension paths
	{
		// Skip empty extension paths
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
		extSearchPaths.push_back(searchPath);
	}
}

void MincPackageManager::definePackage(MincBlockExpr* pkgScope)
{
	pkgScope->name = "pkgmgr"; // Manually set MincPackageManager name

	// Define import statement
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("import $I. ..."),
		[this](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			std::vector<MincExpr*>& pkgPath = ((MincListExpr*)params[0])->exprs;
			std::string pkgName = ((MincIdExpr*)pkgPath[0])->name;
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + PKG_PATH_SEPARATOR + ((MincIdExpr*)pkgPath[i])->name;

			// Import package
			if (!this->tryImportPackage(parentBlock, pkgName))
				throw CompileError("unknown package " + pkgName, params[0]->loc);
		},
		nullptr
	);

	// Define import statement
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("import $L"),
		[this](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			std::string pkgName = ((MincLiteralExpr*)params[0])->value;

			// Trim '"'
			if (pkgName.back() == '"' || pkgName.back() == '\'')
				pkgName = pkgName.substr(1, pkgName.size() - 2);

			// Import package
			if (!this->tryImportPackage(parentBlock, pkgName))
				throw CompileError("unknown package " + pkgName, params[0]->loc);
		},
		nullptr
	);

	// Define export statement
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("export $I. ... $B"),
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			std::vector<MincExpr*>& pkgPath = ((MincListExpr*)params[0])->exprs;
			std::string pkgName = ((MincIdExpr*)pkgPath[0])->name;
			for (size_t i = 1; i < pkgPath.size(); ++i)
				pkgName = pkgName + PKG_PATH_SEPARATOR + ((MincIdExpr*)pkgPath[i])->name;
			MincBlockExpr* exportBlock = (MincBlockExpr*)params[1];

			exportBlock->parent = parentBlock;

			// Export package
			new MincPackage(pkgName.c_str(), nullptr, exportBlock);
		},
		nullptr
	);
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("export $I. ..."),
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			std::vector<MincExpr*>& pkgPath = ((MincListExpr*)params[0])->exprs;
			throw CompileError("Missing export block", pkgPath.empty() ? MincLocation{nullptr, 0, 0, 0, 0} : pkgPath.front()->loc);
		},
		nullptr
	);

	// Define export statement
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("export $L $B"),
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			std::string pkgName = ((MincLiteralExpr*)params[0])->value;
			MincBlockExpr* exportBlock = (MincBlockExpr*)params[1];

			// Trim '"'
			if (pkgName.back() == '"' || pkgName.back() == '\'')
				pkgName = pkgName.substr(1, pkgName.size() - 2);

			exportBlock->parent = parentBlock;

			// Export package
			new MincPackage(pkgName.c_str(), nullptr, exportBlock);
		},
		nullptr
	);
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("export $L"),
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			throw CompileError("Missing export block", params[0]->loc);
		},
		nullptr
	);

	// Load extensions
	for (std::string extSearchPath: extSearchPaths)
		for (const auto& entry: std::filesystem::directory_iterator(extSearchPath))
			if (entry.is_directory())
			{
				// Load extension configuration (minc_ext.json)
				const std::string& extJsonPath = (entry.path() / "minc_ext.json").string();
				if (!std::filesystem::exists(extJsonPath))
					throw CompileError(("extension " + entry.path().string() + " is missing minc_ext.json").c_str());
				Json::Value extJson;
				std::ifstream extJsonFile(extJsonPath.c_str());
				if (!extJsonFile)
					throw CompileError(("unable to load extension configuration " + extJsonPath).c_str());
				bool success = Json::parse(extJsonFile, &extJson);
				extJsonFile.close();
				if (!success)
					throw CompileError(("error parsing extension configuration " + extJsonPath).c_str());

				auto type = extJson.lst.find("type");
				if (type == extJson.lst.end())
					throw CompileError((extJsonPath + ": missing required parameter `type`").c_str());

				if (type->second.str == "importHook")
				{
					const std::string& extBinaryPath = (entry.path() / "minc_ext.so").string();
					char* error;
					auto pkgHandle = dlopen(extBinaryPath.c_str(), RTLD_LAZY);
					if (pkgHandle == nullptr)
					{
						error = dlerror();
						if (error != nullptr)
							throw CompileError(("error loading library " + extBinaryPath + ": " + error).c_str());
						else
							throw CompileError(("unable to load library " + extBinaryPath).c_str());
					}

					postDefineHooks.push_back((PostDefineHook)dlsym(pkgHandle, "postDefineHook"));
					error = dlerror();
					if (error != nullptr)
					{
						dlclose(pkgHandle);
						throw CompileError(("error loading postDefineHook from " + extBinaryPath + ": " + error).c_str());
					}
				}
				else
					throw CompileError((extJsonPath + ": invalid value for parameter `type`: " + type->second.str).c_str());
			}
}

bool MincPackageManager::registerPackage(std::string pkgName, MincPackage* package)
{
	return packages.insert(std::make_pair(pkgName, package)).second;
}

MincBlockExpr* MincPackageManager::loadPackage(std::string pkgName, MincBlockExpr* importer) const
{
	MincPackage* pkg = discoverPackage(pkgName);
	return pkg == nullptr ? nullptr : pkg->load(importer);
}

void MincPackageManager::importPackage(MincBlockExpr* scope, std::string pkgName) const
{
	MincPackage* pkg = discoverPackage(pkgName);
	if (pkg != nullptr)
		pkg->import(scope);
	else
		throw CompileError("unknown package " + pkgName, scope->loc);
}
bool MincPackageManager::tryImportPackage(MincBlockExpr* scope, std::string pkgName) const
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