#ifndef __MINC_DBG_H
#define __MINC_DBG_H

#include <functional>

struct MincBlockExpr;
struct MincSymbol;

typedef std::function<bool(const MincBlockExpr*, const MincSymbol&, std::string*)> GetValueStrFunc;

int launchDebugClient(char* path);
void registerValueSerializer(GetValueStrFunc serializer);

#endif