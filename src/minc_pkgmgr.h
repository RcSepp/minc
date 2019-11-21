#ifndef __MINC_PKGMGR_H
#define __MINC_PKGMGR_H

#include <string>
#include <map>

class BlockExprAST;

typedef void (*MincPackageFunc)(BlockExprAST* pkgScope);

class MincPackage
{
private:
	std::string parentName;
	BlockExprAST* pkgScope;
	MincPackageFunc defineFunc;
	virtual void define(BlockExprAST* pkgScope) { defineFunc(pkgScope); }

public:
	MincPackage(const char* name, MincPackageFunc defineFunc=nullptr);
	virtual ~MincPackage();
	BlockExprAST* load();
	void import(BlockExprAST* scope);
};

class MincPackageManager : public MincPackage
{
private:
	std::map<std::string, MincPackage*> packages;
	void define(BlockExprAST* pkgScope);

public:
	MincPackageManager();
	void registerPackage(std::string pkgName, MincPackage* package)
	{
		packages[pkgName] = package;
	}
	BlockExprAST* loadPackage(std::string pkgName) const;
	void importPackage(BlockExprAST* scope, std::string pkgName) const;
	bool tryImportPackage(BlockExprAST* scope, std::string pkgName) const;
};
MincPackageManager& MINC_PACKAGE_MANAGER();

#endif