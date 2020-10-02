#ifndef __MINC_PKGMGR_H
#define __MINC_PKGMGR_H

#include <string>
#include <list>
#include <map>
#include <mutex>
#include <vector>

class MincBlockExpr;

typedef void (*MincPkgFunc)(MincBlockExpr* pkgScope);

class MincPackage
{
private:
	std::mutex loadMutex;
	std::string parentName;
	const MincPkgFunc defineFunc;
	MincBlockExpr *pkgScope, *defineBlock;

protected:
	static const char PKG_PATH_SEPARATOR;
	virtual void definePackage(MincBlockExpr* pkgScope) { defineFunc(pkgScope); }

public:
	MincPackage(const char* name, MincPkgFunc defineFunc=nullptr, MincBlockExpr* defineBlock=nullptr);
	virtual ~MincPackage();
	MincBlockExpr* load(MincBlockExpr* importer);
	void import(MincBlockExpr* scope);
};

class MincPackageManager : public MincPackage
{
public:
	typedef void (*PostDefineHook)(MincBlockExpr* pkgScope);

private:
	std::map<std::string, MincPackage*> packages;
	std::vector<std::string> pkgSearchPaths, extSearchPaths;
	void definePackage(MincBlockExpr* pkgScope);
	MincPackage* discoverPackage(std::string pkgName) const;

public:
	std::list<PostDefineHook> postDefineHooks;

	MincPackageManager();
	bool registerPackage(std::string pkgName, MincPackage* package);
	MincBlockExpr* loadPackage(std::string pkgName, MincBlockExpr* importer) const;
	void importPackage(MincBlockExpr* scope, std::string pkgName) const;
	bool tryImportPackage(MincBlockExpr* scope, std::string pkgName) const;
};
MincPackageManager& MINC_PACKAGE_MANAGER();

#endif