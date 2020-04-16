#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <dirent.h>
#include <filesystem>
#include <sstream>
#include <string>
#include "minc_api.h"
#include "minc_pkgmgr.h"

const std::string	PACKAGE_PATH_ENV = "MINC_PATH";
const char			PACKAGE_PATH_ENV_SEPARATOR = ':';

bool hasSubdir(const char* path, const char* subdir)
{
	DIR* dir = opendir(path);
	for (dirent* entry = readdir(dir); entry != nullptr; entry = readdir(dir))
		if(entry->d_type == DT_DIR && strcmp(entry->d_name, subdir) == 0)
		{
			closedir(dir);
			return true;
		}
	closedir(dir);
	return false;
}

MincPackage* MincPackageManager::discoverPackage(std::string pkgName) const
{
	// Search registered packages
	auto pkg = packages.find(pkgName);
	if (pkg != packages.end())
		return pkg->second; // Package found

	// Split package name into list of sub-packages
	std::stringstream pkgNameStream(pkgName);
	std::string subpkgName;
	std::vector<std::string> subpkgList;
	while (std::getline(pkgNameStream, subpkgName, '.'))
		subpkgList.push_back(subpkgName);
	
	// Search PACKAGE_PATH_ENV
	const char* packagePaths = std::getenv(PACKAGE_PATH_ENV.c_str());
	if (packagePaths == nullptr)
		raiseCompileError(("environment variable " + PACKAGE_PATH_ENV + " not set").c_str());
	std::stringstream packagePathsStream(packagePaths);
	std::string pkgDir;
	while (std::getline(packagePathsStream, pkgDir, PACKAGE_PATH_ENV_SEPARATOR)) // Traverse package paths
	{
		// Skip empty package paths
		if (pkgDir.empty())
			continue;

		// Append trailing path separator
		if (pkgDir.back() != '/' && pkgDir.back() != '\\')
#ifdef _WIN32
			pkgDir.push_back('\\');
#else
			pkgDir.push_back('/');
#endif

		for (std::string subpkgName: subpkgList) // Traverse sub-packages of pkgName
			if(hasSubdir(pkgDir.c_str(), subpkgName.c_str())) // If a sub-directory named subpkgName exists in pkgDir
			{
				// Append subpkgName to pkgDir
				pkgDir += subpkgName;
#ifdef _WIN32
				pkgDir.push_back('\\');
#else
				pkgDir.push_back('/');
#endif

				std::string pkgPath = pkgDir + subpkgName + ".so";
				if (std::filesystem::exists(pkgPath)) // If a package library exists for this sub-package, ...
				{
					// Load package library
					// Packages will be registed with the package manager during library initialization
					auto pkgHandle = dlopen(pkgPath.c_str(), RTLD_NOW);
					if (pkgHandle == nullptr)
						raiseCompileError(("unable to read package " + pkgPath).c_str());

					// Search registered packages again to discover newly added packages
					pkg = packages.find(pkgName);
					if (pkg != packages.end())
						return pkg->second; // Package found

					//TODO: Raise warning that subpkgName was not defined in pkgPath
				}
			}
	}

	return nullptr; // Package not found
}
