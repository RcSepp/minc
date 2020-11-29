#ifndef __PAWS_STRUCT_H
#define __PAWS_STRUCT_H

#include "paws_subroutine.h"
#include <map>
#include <vector>

struct Struct : public PawsType
{
	typedef PawsType CType;
	struct MincSymbol
	{
		PawsType* type;
		MincExpr* initExpr;
	};

	static PawsMetaType* const TYPE;
	std::map<std::string, MincSymbol> variables;
	std::multimap<std::string, PawsFunc*> methods;
	std::vector<PawsFunc*> constructors;
	MincBlockExpr* body;
	Struct* base;

	MincObject* copy(MincObject* value);
	std::string toString(MincObject* value) const;
	void inherit(const Struct* base);
	MincSymbol* getVariable(const std::string& name, Struct** subStruct=nullptr);
	PawsFunc* getMethod(const std::string& name, Struct** subStruct=nullptr);
};
inline PawsMetaType* const Struct::TYPE = new PawsMetaType(sizeof(Struct));

void defineStruct(MincBlockExpr* scope, const char* name, Struct* strct);

struct StructInstance
{
	MincBlockExpr* body;
};
typedef PawsValue<StructInstance*> PawsStructInstance;

void defineStructInstance(MincBlockExpr* scope, const char* name, Struct* strct, StructInstance* instance);

#endif