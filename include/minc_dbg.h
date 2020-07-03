#ifndef __MINC_DBG_H
#define __MINC_DBG_H

#include <functional>

struct MincObject;

typedef std::function<bool(const MincSymbol&, std::string*)> GetValueStrFunc;

int launchDebugClient(MincBlockExpr* rootBlock);
void registerValueSerializer(GetValueStrFunc serializer);

#endif