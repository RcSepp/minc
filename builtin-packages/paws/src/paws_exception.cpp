#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> std::string PawsException::Type::toString(MincObject* value) const
{
	return "PawsException(\"" + std::string(((PawsException*)value)->get().what()) + "\")";
}

MincPackage PAWS_EXCEPTION("paws.exception", [](MincBlockExpr* pkgScope) {
	registerType<PawsException>(pkgScope, "PawsException");

	// Define try statement
	defineStmt6(pkgScope, "try $S catch $S",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			try
			{
				if (!runExpr(params[0], runtime))
					return false;
			}
			catch (const MincException&)
			{
				return runExpr(params[1], runtime);
			}
			catch (const MincSymbol& err)
			{
				if (err.type == &PAWS_RETURN_TYPE || err.type == &PAWS_AWAIT_TYPE)
					return true;
				return runExpr(params[1], runtime);
			}
			if (runtime.result.type == &PAWS_RETURN_TYPE || runtime.result.type == &PAWS_AWAIT_TYPE)
				return true;
			return runExpr(params[1], runtime);
		}
	);
	defineStmt6(pkgScope, "try $S catch ($E $I) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			MincObject* const catchType = buildExpr(params[1], buildtime).value;
			if (catchType == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[1]), "catch type must be build time constant");
			defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), catchType, nullptr);
			//TODO: Store symbolId
			buildExpr(params[3], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[1], runtime)) //TODO: Consider storing this in the built kernel
				return true;
			MincObject* const catchType = runtime.result.value;

			try
			{
				if (!runExpr(params[0], runtime))
					return false;
			}
			catch (const MincException& err)
			{
				if (isInstance(runtime.parentBlock, PawsException::TYPE, catchType))
				{
					defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), PawsException::TYPE, new PawsException(err));
					return runExpr(params[3], runtime);
				}
				else
					throw;
			}
			catch (const MincSymbol& err)
			{
				if (isInstance(runtime.parentBlock, err.type, catchType))
				{
					defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), err.type, err.value);
					return runExpr(params[3], runtime);
				}
				else
				{
					runtime.result = err;
					return true;
				}
			}
			const MincSymbol& err = runtime.result;
			if (isInstance(runtime.parentBlock, err.type, catchType))
			{
				defineSymbol((MincBlockExpr*)params[3], getIdExprName((MincIdExpr*)params[2]), err.type, err.value);
				return runExpr(params[3], runtime);
			}
			else
				return true;
		}
	);

	// Define throw statement
	defineStmt6(pkgScope, "throw $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			runExpr(params[0], runtime);
			return true;
		}
	);

	// Define constructors
	defineExpr(pkgScope, "PawsException()",
		+[]() -> MincException {
			return MincException();
		}
	);
	defineExpr9(pkgScope, "PawsException($E<PawsString>)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			if (buildtime.result.value != nullptr)
			{
				const std::string& msg = ((PawsString*)buildtime.result.value)->get();
				buildtime.result = MincSymbol(PawsException::TYPE, new PawsException(MincException(msg, getLocation(params[0]))));
			}
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const std::string& msg = ((PawsString*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsException::TYPE, new PawsException(MincException(msg, getLocation(params[0]))));
			return false;
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