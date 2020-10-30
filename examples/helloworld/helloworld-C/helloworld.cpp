#include <string.h>
#include <stdio.h>
#include "minc_api.h"
#include "minc_pkgmgr.h"

//TODO: Implement package loading in other langauages (i.e. Python)

MincObject STRING_TYPE, META_TYPE;

struct String : public std::string, MincObject
{
	char* val;
	String(const char* val) : val(new char[strlen(val) + 1])
	{
		strcpy(this->val, val);
	}
	String(const char* val, size_t len) : val(new char[len + 1])
	{
		strncpy(this->val, val, len);
	}
	~String()
	{
		delete[] val;
	}
};

MincPackage HELLOWORLD_C_PKG("helloworld-C", [](MincBlockExpr* pkgScope) {
	defineSymbol(pkgScope, "string", &META_TYPE, &STRING_TYPE);
	//TODO: Clarify that type names within templates are resolved using defined symbols.
	// Emphasize that this behaviour allows non-unique type names within a program and that it aids multithreading by making type lookups a local operation within the AST.
	// (This is the same reason why casts are defined on AST blocks, instead of globally.)

	defineExpr3(pkgScope, "$L",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				return MincSymbol(&STRING_TYPE, new String(valueStart, valueEnd - valueStart));
			}

			raiseCompileError("Non-string literals not implemented", params[0]);
			return MincSymbol(nullptr, nullptr); // LCOV_EXCL_LINE
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;
			if (*valueEnd == '"' || *valueEnd == '\'')
				return &STRING_TYPE;
			return nullptr;
		}
	);

	defineStmt6(pkgScope, "print($E<string>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			String* const message = (String*)codegenExpr(params[0], parentBlock).value;
			std::cout << message->val << " from C!\n";
		}
	);
});