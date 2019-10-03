#include "ast.h"

void CastRegister::defineCast(BaseType* fromType, BaseType* toType, CodegenContext* context)
{
	casts[std::make_pair(fromType, toType)] = context;
}
CodegenContext* CastRegister::lookupCast(BaseType* fromType, BaseType* toType) const
{
	auto cast = casts.find(std::make_pair(fromType, toType));
	return cast == casts.end() ? nullptr : cast->second;
}
void CastRegister::listAllCasts(std::list<std::pair<BaseType*, BaseType*>>& casts) const
{
	for (const std::pair<std::pair<BaseType*, BaseType*>, CodegenContext*> cast: this->casts)
		casts.push_back(cast.first);
}