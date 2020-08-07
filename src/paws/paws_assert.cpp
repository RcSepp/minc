#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_ASSERT("paws.assert", [](MincBlockExpr* pkgScope) {
	defineStmt2(pkgScope, "assert $E<PawsInt> ",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			int test = ((PawsInt*)codegenExpr(params[0], parentBlock).value)->get();
			if (!test)
				throw CompileError(parentBlock, getLocation(params[0]), "Assertion failed: %e", params[0]);
		}
	);
	defineStmt2(pkgScope, "assert $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			raiseCompileError("Assertion expression is undefined", params[0]);
		}
	);
});