#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_EXCEPTION("paws.exception", [](MincBlockExpr* pkgScope) {
	registerType<PawsException>(pkgScope, "PawsException");

	// Define try statement
	defineStmt6(pkgScope, "try $S catch $S",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			try
			{
				runExpr(params[0], parentBlock);
			}
			catch (const MincException&)
			{
				runExpr(params[1], parentBlock);
			}
			catch (const MincSymbol&)
			{
				runExpr(params[1], parentBlock);
			}
		}
	);
	defineStmt6(pkgScope, "try $S catch ($E $I) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
			MincObject* const catchType = runExpr(params[1], parentBlock).value;
			if (catchType == nullptr)
				throw CompileError(parentBlock, getLocation(params[1]), "catch type must be build time constant");
			defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), catchType, nullptr);
			//TODO: Store symbolId
			buildExpr(params[3], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			try
			{
				runExpr(params[0], parentBlock);
			}
			catch (const MincException& err)
			{
				MincObject* const catchType = runExpr(params[1], parentBlock).value;
				if (isInstance(parentBlock, PawsException::TYPE, catchType))
				{
					defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), PawsException::TYPE, new PawsException(err));
					runExpr(params[3], parentBlock);
				}
				else
					throw;
			}
			catch (const MincSymbol& err)
			{
				MincObject* const catchType = runExpr(params[1], parentBlock).value;
				if (isInstance(parentBlock, err.type, catchType))
				{
					defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), err.type, err.value);
					runExpr(params[3], parentBlock);
				}
				else
					throw;
			}
		}
	);

	// Define throw statement
	defineStmt6(pkgScope, "throw $E",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			throw runExpr(params[0], parentBlock);
		}
	);

	// Define constructors
	defineExpr(pkgScope, "PawsException()",
		+[]() -> MincException {
			return MincException();
		}
	);
	defineExpr9(pkgScope, "PawsException($E<PawsString>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const std::string& msg = ((PawsString*)runExpr(params[0], parentBlock).value)->get();
			return MincSymbol(PawsException::TYPE, new PawsException(MincException(msg, getLocation(params[0]))));
		},
		PawsException::TYPE
	);

	// Define msg getter
	defineExpr(pkgScope, "$E<PawsException>.msg",
		+[](MincException err) -> std::string {
			const char* msg = err.what();
			return msg != nullptr ? msg : "";
		}
	);
});