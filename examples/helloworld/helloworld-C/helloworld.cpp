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

	defineExpr3_2(pkgScope, "$L",
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;

			if (*valueEnd == '"' || *valueEnd == '\'')
			{
				const char* valueStart = strchr(value, *valueEnd) + 1;
				runtime.result = MincSymbol(&STRING_TYPE, new String(valueStart, valueEnd - valueStart));
			}
			else
				raiseCompileError("Non-string literals not implemented", params[0]);
			return false;
		},
		[](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			const char* value = getLiteralExprValue((MincLiteralExpr*)params[0]);
			const char* valueEnd = value + strlen(value) - 1;
			if (*valueEnd == '"' || *valueEnd == '\'')
				return &STRING_TYPE;
			else
				return nullptr;
		}
	);

	defineStmt6_2(pkgScope, "print($E<string>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			String* const message = (String*)runtime.result.value;
			std::cout << message->val << " from C!\n";
			return false;
		}
	);
});