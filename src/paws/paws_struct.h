#ifndef __PAWS_STRUCT_H
#define __PAWS_STRUCT_H

#include "paws_subroutine.h"
#include <map>
#include <vector>

struct Struct : public PawsType
{
	struct MincSymbol
	{
		PawsType* type;
		MincExpr* initExpr;
	};
	struct Method : public PawsRegularFunc
	{
	};

	static PawsType* const TYPE;
	std::map<std::string, MincSymbol> variables;
	std::multimap<std::string, Method> methods;
	std::vector<PawsType*> constructors;
};

void defineStruct(MincBlockExpr* scope, const char* name, Struct* strct);

struct StructInstance
{
	std::map<std::string, MincObject*> variables;
};
typedef PawsValue<StructInstance*> PawsStructInstance;

void defineStructInstance(MincBlockExpr* scope, const char* name, Struct* strct, StructInstance* instance);

#endif