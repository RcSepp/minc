#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXCEPTION("paws.exception", [](MincBlockExpr* pkgScope) {
	registerType<PawsException>(pkgScope, "PawsException");

	// Define try statement
	defineStmt2(pkgScope, "try $S catch $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			try
			{
				codegenExpr(params[0], parentBlock);
			}
			catch (const MincException& err)
			{
				codegenExpr(params[1], parentBlock);
			}
		}
	);
	defineStmt2(pkgScope, "try $S catch ($E<PawsType> $I) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			try
			{
				codegenExpr(params[0], parentBlock);
			}
			catch (const MincException& err)
			{
				MincObject* const catchType = codegenExpr(params[1], parentBlock).value;
				if (isInstance(parentBlock, PawsException::TYPE, catchType))
				{
					defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), PawsException::TYPE, new PawsException(err));
					codegenExpr(params[3], parentBlock);
				}
				else
					throw;
			}
		}
	);

	// Define constructors
	defineExpr(pkgScope, "PawsException()",
		+[]() -> MincException {
			return MincException();
		}
	);
	defineExpr2(pkgScope, "PawsException($E<PawsString>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const std::string& msg = ((PawsString*)codegenExpr(params[0], parentBlock).value)->get();
			return MincSymbol(PawsException::TYPE, new PawsException(MincException(msg, getLocation(params[0]))));
		},
		PawsException::TYPE
	);


	// Define msg getter
	defineExpr(pkgScope, "$E<PawsException>.msg",
		+[](MincException err) -> std::string {
			return err.what();
		}
	);
});