#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_ASSERT("paws.assert", [](MincBlockExpr* pkgScope) {
	defineStmt2(pkgScope, "assert($E<PawsInt>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			int test = ((PawsInt*)codegenExpr(params[0], parentBlock).value)->get();
			if (!test)
				throw CompileError(parentBlock, getLocation(params[0]), "Assertion failed: %e", params[0]);
		}
	);
	defineStmt2(pkgScope, "assert($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			getType(params[0], parentBlock); // Raise expression errors if any
			raiseCompileError("Assertion expression is undefined", params[0]);
		}
	);
	for (auto tplt: {"assert($E) throws $E<PawsException>", "assert $B throws $E<PawsException>"})
		defineStmt2(pkgScope, tplt,
			[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
				const MincException& expected = ((PawsException*)codegenExpr(params[1], parentBlock).value)->get();
				const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
				try
				{
					codegenExpr(params[0], parentBlock);
				}
				catch (const MincException& got)
				{
					const std::string gotStr = got.what() == nullptr ? std::string() : '"' + std::string(got.what()) + '"';
					if (expectedStr != gotStr)
						throw CompileError(
							parentBlock, getLocation(params[0]),
							"Assertion failed: Expected exception MincException(%S), but got exception MincException(%S)",
							expectedStr, gotStr
						);
					else
						return;
				}
				throw CompileError(parentBlock, getLocation(params[0]), "Assertion failed: MincException(%S) not raised", expectedStr);
			}
		);
});