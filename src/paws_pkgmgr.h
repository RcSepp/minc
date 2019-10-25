#ifndef __PAWS_PKGMGR_H
#define __PAWS_PKGMGR_H

#include <string>
#include <map>

class BlockExprAST;

typedef void (*PawsPackageFunc)(BlockExprAST* pkgScope);

class PawsPackage
{
private:
	BlockExprAST* pkgScope;
	PawsPackageFunc defineFunc;
	virtual void define(BlockExprAST* pkgScope) { defineFunc(pkgScope); }

public:
	PawsPackage(const char* name, PawsPackageFunc defineFunc=nullptr);
	virtual ~PawsPackage();
	void import(BlockExprAST* scope);
};

class PawsPackageManager : public PawsPackage
{
private:
	std::map<std::string, PawsPackage*> packages;
	void define(BlockExprAST* pkgScope);

public:
	PawsPackageManager();
	void registerPackage(std::string pkgName, PawsPackage* package)
	{
		packages[pkgName] = package;
	}
	bool importPackage(BlockExprAST* scope, std::string pkgName) const;
};
PawsPackageManager& PAWS_PACKAGE_MANAGER();

#endif