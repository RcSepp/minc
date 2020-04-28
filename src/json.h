#ifndef __JSON_H
#define __JSON_H

#include <istream>
#include <map>
#include <string>
#include <vector>

namespace Json
{
	struct Value
	{
		std::string str;
		std::vector<Value> arr;
		std::map<std::string, Value> lst;
	};
	bool parse(std::istream& file, Value* out);
}

#endif