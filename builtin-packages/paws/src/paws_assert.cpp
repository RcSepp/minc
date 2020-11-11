#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_ASSERT("paws.assert", [](MincBlockExpr* pkgScope) {
	defineStmt6(pkgScope, "assert($E<PawsInt>)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			int test = ((PawsInt*)runExpr(params[0], parentBlock).value)->get();
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

			MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
			{
				if (thrownDuringBuild)
					return getVoid();

				const MincException& expected = ((PawsException*)runExpr(params[1], parentBlock).value)->get();
				const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
				try
				{
					runExpr(params[0], parentBlock);
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
						return getVoid();
				}
				throw CompileError(parentBlock, getLocation(params[0]), "Assertion failed: MincException(%S) not raised", expectedStr);

				return getVoid();
			}
			MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
		};
		defineStmt4(pkgScope, tplt, new AssertKernel());
	}
});