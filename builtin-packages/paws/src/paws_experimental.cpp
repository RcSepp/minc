#include "minc_api.hpp"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "paws_struct.h"
#include "minc_pkgmgr.h"

void constructor()
{
}
void constructor2(int foo)
{
	int abc = 0;
}

void test()
{
}

class Foo
{
public:
	int bla;
	Foo() : bla(456) {}
	// void constructor(int f)
	// {
	// 	bla = f;
	// }
	static Foo* constructor(int f)
	{
		Foo* foo = new Foo();
		foo->bla = f;
		return foo;
	}
	void test()
	{
		int abc = bla;
	}
} FOO;

MincPackage PAWS_EXPERIMENTAL("paws.frame.experimental", [](MincBlockExpr* pkgScope) {
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.struct");

	Struct* qMainWindow = new Struct();
	qMainWindow->methods.insert(std::make_pair("test", new PawsExternFunc(&Foo::test)));
	qMainWindow->constructors.push_back(new PawsExternFunc(Foo::constructor));
//	qMainWindow->constructors.push_back(new PawsExternFunc(constructor2));
	defineStruct(pkgScope, "QMainWindow", qMainWindow);
});