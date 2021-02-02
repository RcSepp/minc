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
		const MincStackSymbol* symbol;
		MincExpr* initExpr;
	};

	static PawsMetaType* const TYPE;
	std::map<std::string, MincSymbol> variables;
	std::multimap<std::string, PawsFunc*> methods;
	std::vector<PawsFunc*> constructors;
	const MincStackSymbol* thisSymbol;
	MincBlockExpr* body;
	Struct* base;

	MincObject* copy(MincObject* value);
	void copyTo(MincObject* src, MincObject* dest);
	void copyToNew(MincObject* src, MincObject* dest);
	MincObject* alloc();
	MincObject* allocTo(MincObject* memory);
	std::string toString(MincObject* value) const;
	void inherit(const Struct* base);
	MincSymbol* getVariable(const std::string& name, Struct** subStruct=nullptr);
	PawsFunc* getMethod(const std::string& name, Struct** subStruct=nullptr);
};
inline PawsMetaType* const Struct::TYPE = new PawsMetaType(sizeof(Struct));

void defineStruct(MincBlockExpr* scope, const char* name, Struct* strct);

struct StructInstance
{
	StructInstance* base;
	MincStackFrame heapFrame;
	StructInstance() : base(nullptr) {}
};
typedef PawsValue<StructInstance*> PawsStructInstance;

void defineStructInstance(MincBlockExpr* scope, const char* name, Struct* strct, StructInstance* instance);

#endif