#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> std::string PawsString::Type::toString(MincObject* value) const
{
	return '"' + ((PawsString*)value)->get() + '"';
}

template<> std::string PawsStringMap::Type::toString(MincObject* value) const
{
	//TODO: Use stringstream instead
	std::string str = "{ ";
	for (const std::pair<const std::string, std::string>& pair: ((PawsStringMap*)value)->get())
		str += pair.first + ": " + pair.second + ", ";
	str[str.size() - 2] = ' ';
	str[str.size() - 1] = '}';
	return str;
}

MincPackage PAWS_STRING("paws.string", [](MincBlockExpr* pkgScope) {

	// >>> PawsString expressions

	// Define string concatenation
	defineExpr9_2(pkgScope, "$E<PawsString> += $E<PawsString>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			PawsString* self = (PawsString*)runtime.result.value;
			if (runExpr2(params[1], runtime))
				return true;
			self->get() += ((PawsString*)runtime.result.value)->get();
			runtime.result.value = self; // result.type is already PawsString::TYPE
			return false;
		},
		PawsString::TYPE
	);
	defineExpr(pkgScope, "$E<PawsString> + $E<PawsString>",
		+[](std::string a, std::string b) -> std::string {
			return a + b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> * $E<PawsString>",
		+[](int a, std::string b) -> std::string {
			std::string result;
			for (int i = 0; i < a; ++i)
				result += b;
			return result;
		}
	);
	defineExpr(pkgScope, "$E<PawsString> * $E<PawsInt>",
		+[](std::string a, int b) -> std::string {
			std::string result;
			for (int i = 0; i < b; ++i)
				result += a;
			return result;
		}
	);

	// Define string length getter
	defineExpr(pkgScope, "$E<PawsString>.length",
		+[](std::string a) -> int {
			return a.length();
		}
	);

	// Define substring
	defineExpr(pkgScope, "$E<PawsString>.substr($E<PawsInt>)",
		+[](std::string a, int b) -> std::string {
			return a.substr(b);
		}
	);
	defineExpr(pkgScope, "$E<PawsString>.substr($E<PawsInt>, $E<PawsInt>)",
		+[](std::string a, int b, int c) -> std::string {
			return a.substr(b, c);
		}
	);

	// Define substring finder
	defineExpr(pkgScope, "$E<PawsString>.find($E<PawsString>)",
		+[](std::string a, std::string b) -> int {
			return a.find(b);
		}
	);
	defineExpr(pkgScope, "$E<PawsString>.rfind($E<PawsString>)",
		+[](std::string a, std::string b) -> int {
			return a.rfind(b);
		}
	);

	// Define string parser
	defineExpr(pkgScope, "$E<PawsString>.parseInt()",
		+[](std::string a) -> int {
			return std::stoi(a);
		}
	);
	defineExpr(pkgScope, "$E<PawsString>.parseInt($E<PawsInt>)",
		+[](std::string a, int b) -> int {
			return std::stoi(a, 0, b);
		}
	);

	// Define string relations
	defineExpr(pkgScope, "$E<PawsString> == $E<PawsString>",
		+[](std::string a, std::string b) -> int {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsString> != $E<PawsString>",
		+[](std::string a, std::string b) -> int {
			return a != b;
		}
	);

	// Define string iterating for statement
	defineStmt6_2(pkgScope, "for ($I: $E<PawsString>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
			defineSymbol((MincBlockExpr*)params[2], getIdExprName((MincIdExpr*)params[0]), PawsString::TYPE, nullptr);
			buildExpr(params[2], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* iterExpr = (MincIdExpr*)params[0];
			if (runExpr2(params[1], runtime))
				return true;
			PawsString* str = (PawsString*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsString iter;
			defineSymbol(body, getIdExprName(iterExpr), PawsString::TYPE, &iter);
			for (char c: str->get())
			{
				iter.set(std::string(1, c));
				if (runExpr2((MincExpr*)body, runtime))
					return true;
			}
			return false;
		}
	);

	// >>> PawsStringMap expressions

	// Define string map constructor
	defineExpr9_2(pkgScope, "map($E<PawsString>: $E<PawsString>, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			for (MincExpr* key: getListExprExprs((MincListExpr*)params[0]))
				buildExpr(key, parentBlock);
			for (MincExpr* value: getListExprExprs((MincListExpr*)params[1]))
				buildExpr(value, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			std::vector<MincExpr*>& keys = getListExprExprs((MincListExpr*)params[0]);
			std::vector<MincExpr*>& values = getListExprExprs((MincListExpr*)params[1]);
			std::map<std::string, std::string> map;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				if (runExpr2(keys[i], runtime))
					return true;
				const std::string& key = ((PawsString*)runtime.result.value)->get();
				if (runExpr2(values[i], runtime))
					return true;
				map[key] = ((PawsString*)runtime.result.value)->get();
			}
			runtime.result = MincSymbol(PawsStringMap::TYPE, new PawsStringMap(map));
			return false;
		},
		PawsStringMap::TYPE
	);

	// Define string map element getter
	defineExpr(pkgScope, "$E<PawsStringMap>[$E<PawsString>]",
		+[](std::map<std::string, std::string> map, std::string key) -> std::string {
			auto pair = map.find(key);
			return pair == map.end() ? nullptr : pair->second;
		}
	);

	// Define string map element search
	defineExpr(pkgScope, "$E<PawsStringMap>.contains($E<PawsString>)",
		+[](std::map<std::string, std::string> map, std::string key) -> int {
			return map.find(key) != map.end();
		}
	);

	// Define string map iterating for statement
	defineStmt6_2(pkgScope, "for ($I, $I: $E<PawsStringMap>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			buildExpr(params[2], parentBlock);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, nullptr);
			defineSymbol(body, getIdExprName(valueExpr), PawsString::TYPE, nullptr);
			buildExpr((MincExpr*)body, parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			if (runExpr2(params[2], runtime))
				return true;
			PawsStringMap* map = (PawsStringMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsString key, value;
			defineSymbol(body, getIdExprName(keyExpr), PawsString::TYPE, &key);
			defineSymbol(body, getIdExprName(valueExpr), PawsString::TYPE, &value);
			for (std::pair<const std::string, std::string> pair: map->get())
			{
				key.set(pair.first);
				value.set(pair.second);
				if (runExpr2((MincExpr*)body, runtime))
					return true;
			}
			return false;
		}
	);
});