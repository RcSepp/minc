#ifndef __MINC_DBG_H
#define __MINC_DBG_H

#include <functional>

struct BaseValue;

typedef std::function<bool(const BaseValue*, std::string*)> GetValueStrFunc;
void registerValueSerializer(GetValueStrFunc serializer);

#endif