#ifndef __MINC_PKGMGR_H
#define __MINC_PKGMGR_H

#include <string>
#include <map>
#include <mutex>

class BlockExprAST;

typedef void (*MincPackageFunc)(BlockExprAST* pkgScope);

class MincPackage
{
private:
	std::mutex loadMutex;
	std::string parentName;
	BlockExprAST* pkgScope;
	MincPackageFunc defineFunc;

protected:
	virtual void definePackage(BlockExprAST* pkgScope) { defineFunc(pkgScope); }

public:
	MincPackage(const char* name, MincPackageFunc defineFunc=nullptr);
	virtual ~MincPackage();
	BlockExprAST* load(BlockExprAST* importer);
	void import(BlockExprAST* scope);
};

class MincPackageManager : public MincPackage
{
private:
	std::map<std::string, MincPackage*> packages;
	void definePackage(BlockExprAST* pkgScope);

public:
	MincPackageManager();
	void registerPackage(std::string pkgName, MincPackage* package)
	{
		packages[pkgName] = package;
	}
	MincPackage* discoverPackage(std::string pkgName) const;
	BlockExprAST* loadPackage(std::string pkgName, BlockExprAST* importer) const;
	void importPackage(BlockExprAST* scope, std::string pkgName) const;
	bool tryImportPackage(BlockExprAST* scope, std::string pkgName) const;
};
MincPackageManager& MINC_PACKAGE_MANAGER();

#endif