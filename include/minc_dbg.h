#ifndef __MINC_DBG_H
#define __MINC_DBG_H

#include <functional>

struct MincObject;

typedef std::function<bool(const Variable&, std::string*)> GetValueStrFunc;

int launchDebugClient(BlockExprAST* rootBlock);
void registerValueSerializer(GetValueStrFunc serializer);

#endif