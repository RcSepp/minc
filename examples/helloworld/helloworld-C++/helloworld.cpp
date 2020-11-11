#include <string>
#include <cstring>
#include <iostream>
#include "minc_api.hpp"
#include "minc_pkgmgr.h"

//TODO: Implement package loading in other langauages (i.e. Python)

MincObject STRING_TYPE, META_TYPE;

struct String : public std::string, public MincObject
{
	String(const std::string val) : std::string(val) {}
};

MincPackage HELLOWORLD_CPP_PKG("helloworld-C++", [](MincBlockExpr* pkgScope) {
	pkgScope->defineSymbol("string", &META_TYPE, &STRING_TYPE);
	//TODO: Clarify that type names within templates are resolved using defined symbols.
	// Emphasize that this behaviour allows non-unique type names within a program and that it aids multithreading by making type lookups a local operation within the AST.
	// (This is the same reason why casts are defined on AST blocks, instead of globally.)

	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$L")[0],
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) -> MincSymbol {
			const std::string& value = ((MincLiteralExpr*)params[0])->value;

			if (value.back() == '"' || value.back() == '\'')
				return MincSymbol(&STRING_TYPE, new String(value.substr(1, value.size() - 2)));

			raiseCompileError("Non-string literals not implemented", params[0]);
			return MincSymbol(nullptr, nullptr); // LCOV_EXCL_LINE
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) -> MincObject* {
			const std::string& value = ((MincLiteralExpr*)params[0])->value;
			if (value.back() == '"' || value.back() == '\'')
				return &STRING_TYPE;
			return nullptr;
		}
	);

	pkgScope->defineStmt(MincBlockExpr::parseCTplt("print($E<string>)"),
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			params[0]->build(parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params) {
			String* const message = (String*)params[0]->run(parentBlock).value;
			std::cout << *message << " from C++!\n";
		}
	);
});