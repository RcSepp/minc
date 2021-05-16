// This package provides common data types for passing data between languages
// For example languageA_int --typeCast-> minc_i32 --typeCast-> languageB_int

#include "minc_api.hpp"
#include "minc_pkgmgr.h"

MincObject INTEROP_METATYPE;
MincObject minc_i32;

MincPackage CUCUMBER_PKG("interop", [](MincBlockExpr* pkgScope) {
	pkgScope->defineSymbol("minc_i32", &INTEROP_METATYPE, &minc_i32);
});