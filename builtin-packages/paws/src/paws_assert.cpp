#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_ASSERT("paws.assert", [](MincBlockExpr* pkgScope) {
	defineStmt6_2(pkgScope, "assert($E<PawsInt>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr2(params[0], runtime))
				return true;
			int test = ((PawsInt*)runtime.result.value)->get();
			if (!test)
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "Assertion failed: %e", params[0]);
			return false;
		}
	);
	defineStmt5(pkgScope, "assert($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			getType(params[0], parentBlock); // Raise expression errors if any
			raiseCompileError("Assertion expression is undefined", params[0]);
		}
	);
	for (auto tplt: {"assert($E) throws $E<PawsException>", "assert $B throws $E<PawsException>"})
	{
		class AssertKernel : public MincKernel
		{
			bool thrownDuringBuild;
		public:
			AssertKernel(bool thrownDuringBuild=false) : thrownDuringBuild(thrownDuringBuild) {}

			MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
			{
				buildExpr(params[1], parentBlock);
				const MincException& expected = ((PawsException*)runExpr(params[1], parentBlock).value)->get();
				const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
				try
				{
					buildExpr(params[0], parentBlock);
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
						return new AssertKernel(true);
				}
				return new AssertKernel(false);
			}
			void dispose(MincKernel* kernel)
			{
				delete kernel;
			}

			bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
			{
				if (thrownDuringBuild)
					return false;

				if (runExpr2(params[1], runtime))
					return true;
				const MincException& expected = ((PawsException*)runtime.result.value)->get();
				const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
				try
				{
					if (runExpr2(params[0], runtime))
						return true;
				}
				catch (const MincException& got)
				{
					const std::string gotStr = got.what() == nullptr ? std::string() : '"' + std::string(got.what()) + '"';
					if (expectedStr != gotStr)
						throw CompileError(
							runtime.parentBlock, getLocation(params[0]),
							"Assertion failed: Expected exception MincException(%S), but got exception MincException(%S)",
							expectedStr, gotStr
						);
					else
						return false;
				}
				throw CompileError(runtime.parentBlock, getLocation(params[0]), "Assertion failed: MincException(%S) not raised", expectedStr);
			}
			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
		};
		defineStmt4(pkgScope, tplt, new AssertKernel());
	}
});