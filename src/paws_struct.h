#ifndef __PAWS_STRUCT_H
#define __PAWS_STRUCT_H

#include "paws_subroutine.h"
#include <map>
#include <vector>

struct Struct : public PawsType
{
	struct Variable
	{
		PawsType* type;
		ExprAST* initExpr;
	};
	struct Method : public PawsRegularFunc
	{
	};

	std::map<std::string, Variable> variables;
	std::multimap<std::string, Method> methods;
	std::vector<PawsType*> constructors;
};
typedef PawsValue<Struct*> PawsStruct;

void defineStruct(BlockExprAST* scope, const char* name, Struct* strct);

struct StructInstance
{
	std::map<std::string, BaseValue*> variables;
};
typedef PawsValue<StructInstance*> PawsStructInstance;

void defineStructInstance(BlockExprAST* scope, const char* name, Struct* strct, StructInstance* instance);

#endif