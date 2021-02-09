#include "minc_api.hpp"
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
	struct StringConcatenationKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			PawsString* self = (PawsString*)runtime.result.value;
			if (params[1]->run(runtime))
				return true;
			self->get() += ((PawsString*)runtime.result.value)->get();
			runtime.result.value = self; // result.type is already PawsString::TYPE
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsString::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsString> += $E<PawsString>")[0], new StringConcatenationKernel());
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
	class StringIterationKernel : public MincKernel
	{
		const MincStackSymbol* const iterId;
	public:
		StringIterationKernel(const MincStackSymbol* iterId=nullptr)
			: iterId(iterId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* valueExpr = (MincIdExpr*)params[0];
			params[1]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			const MincStackSymbol* iterId = body->allocStackSymbol(valueExpr->name, PawsString::TYPE, PawsString::TYPE->size);
			body->build(buildtime);
			return new StringIterationKernel(iterId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[1]->run(runtime))
				return true;
			PawsString* str = (PawsString*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[2];
			PawsString* iter = (PawsString*)body->getStackSymbolOfNextStackFrame(runtime, iterId);
			PawsString::TYPE->allocTo(iter);
			for (char c: str->get())
			{
				iter->set(std::string(1, c));
				if (body->run(runtime))
					return true;
			}
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I: $E<PawsString>) $B"), new StringIterationKernel());

	// >>> PawsStringMap expressions

	// Define string map constructor
	struct StringMapDefinitionKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			for (MincExpr* key: ((MincListExpr*)params[0])->exprs)
				key->build(buildtime);
			for (MincExpr* value: ((MincListExpr*)params[1])->exprs)
				value->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			std::vector<MincExpr*>& keys = ((MincListExpr*)params[0])->exprs;
			std::vector<MincExpr*>& values = ((MincListExpr*)params[1])->exprs;
			std::map<std::string, std::string> map;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				if (keys[i]->run(runtime))
					return true;
				const std::string& key = ((PawsString*)runtime.result.value)->get();
				if (values[i]->run(runtime))
					return true;
				map[key] = ((PawsString*)runtime.result.value)->get();
			}
			runtime.result = MincSymbol(PawsStringMap::TYPE, new PawsStringMap(map));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsStringMap::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("map($E<PawsString>: $E<PawsString>, ...)")[0], new StringMapDefinitionKernel());

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
	class StringMapIterationKernel : public MincKernel
	{
		const MincStackSymbol* const keyId;
		const MincStackSymbol* const valueId;
	public:
		StringMapIterationKernel(const MincStackSymbol* keyId=nullptr, const MincStackSymbol* valueId=nullptr)
			: keyId(keyId), valueId(valueId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			MincIdExpr* keyExpr = (MincIdExpr*)params[0];
			MincIdExpr* valueExpr = (MincIdExpr*)params[1];
			params[2]->build(buildtime);
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			const MincStackSymbol* keyId = body->allocStackSymbol(keyExpr->name, PawsString::TYPE, PawsString::TYPE->size);
			const MincStackSymbol* valueId = body->allocStackSymbol(valueExpr->name, PawsString::TYPE, PawsString::TYPE->size);
			body->build(buildtime);
			return new StringMapIterationKernel(keyId, valueId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[2]->run(runtime))
				return true;
			PawsStringMap* map = (PawsStringMap*)runtime.result.value;
			MincBlockExpr* body = (MincBlockExpr*)params[3];
			PawsString* key = (PawsString*)body->getStackSymbolOfNextStackFrame(runtime, keyId);
			PawsString* value = (PawsString*)body->getStackSymbolOfNextStackFrame(runtime, valueId);
			PawsString::TYPE->allocTo(key);
			PawsString::TYPE->allocTo(value);
			for (std::pair<const std::string, std::string> pair: map->get())
			{
				key->set(pair.first);
				value->set(pair.second);
				if (body->run(runtime))
					return true;
			}
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("for ($I, $I: $E<PawsStringMap>) $B"), new StringMapIterationKernel());
});