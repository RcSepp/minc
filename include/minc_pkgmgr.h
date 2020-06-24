#ifndef __MINC_PKGMGR_H
#define __MINC_PKGMGR_H

#include <string>
#include <map>
#include <mutex>
#include <vector>

class BlockExprAST;

typedef void (*MincPackageFunc)(BlockExprAST* pkgScope);

class MincPackage
{
private:
	std::mutex loadMutex;
	std::string parentName;
	const MincPackageFunc defineFunc;
	BlockExprAST *pkgScope, *defineBlock;

protected:
	static const char PKG_PATH_SEPARATOR;
	virtual void definePackage(BlockExprAST* pkgScope) { defineFunc(pkgScope); }

public:
	MincPackage(const char* name, MincPackageFunc defineFunc=nullptr, BlockExprAST* defineBlock=nullptr);
	virtual ~MincPackage();
	BlockExprAST* load(BlockExprAST* importer);
	void import(BlockExprAST* scope);
};

class MincPackageManager : public MincPackage
{
private:
	std::map<std::string, MincPackage*> packages;
	std::vector<std::string> pkgSearchPaths;
	void definePackage(BlockExprAST* pkgScope);
	MincPackage* discoverPackage(std::string pkgName) const;

public:
	MincPackageManager();
	bool registerPackage(std::string pkgName, MincPackage* package);
	BlockExprAST* loadPackage(std::string pkgName, BlockExprAST* importer) const;
	void importPackage(BlockExprAST* scope, std::string pkgName) const;
	bool tryImportPackage(BlockExprAST* scope, std::string pkgName) const;
};
MincPackageManager& MINC_PACKAGE_MANAGER();

#endif