#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_ASSERT("paws.assert", [](MincBlockExpr* pkgScope) {
	struct AssertKernel1 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			int test = ((PawsInt*)runtime.result)->get();
			if (!test)
				throw CompileError(runtime.parentBlock, params[0]->loc, "Assertion failed: %e", params[0]);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("assert($E<PawsInt>)"), new AssertKernel1());

	struct AssertKernel2 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->getType(buildtime.parentBlock); // Raise expression errors if any
			throw CompileError(buildtime.parentBlock, params[0]->loc, "Assertion expression is undefined");
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("assert($E)"), new AssertKernel2());

	class AssertKernel3 : public MincKernel
	{
		bool thrownDuringBuild;
	public:
		AssertKernel3(bool thrownDuringBuild=false) : thrownDuringBuild(thrownDuringBuild) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			if (buildtime.result.value == nullptr)
				throw CompileError(buildtime.parentBlock, params[1]->loc, "assert exception must be build time constant");
			const MincException& expected = ((PawsException*)buildtime.result.value)->get();
			const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
			try
			{
				params[0]->build(buildtime);
			}
			catch (const MincException& got)
			{
				const std::string gotStr = got.what() == nullptr ? std::string() : '"' + std::string(got.what()) + '"';
				if (expectedStr != gotStr)
					throw CompileError(
						buildtime.parentBlock, params[0]->loc,
						"Assertion failed: Expected exception MincException(%S), but got exception MincException(%S)",
						expectedStr, gotStr
					);
				else
					return new AssertKernel3(true);
			}
			return new AssertKernel3(false);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (thrownDuringBuild)
				return false;

			if (params[1]->run(runtime))
				return true;
			const MincException& expected = ((PawsException*)runtime.result)->get();
			const std::string expectedStr = expected.what() == nullptr ? std::string() : '"' + std::string(expected.what()) + '"';
			try
			{
				if (params[0]->run(runtime))
					return true;
			}
			catch (const MincException& got)
			{
				const std::string gotStr = got.what() == nullptr ? std::string() : '"' + std::string(got.what()) + '"';
				if (expectedStr != gotStr)
					throw CompileError(
						runtime.parentBlock, params[0]->loc,
						"Assertion failed: Expected exception MincException(%S), but got exception MincException(%S)",
						expectedStr, gotStr
					);
				else
					return false;
			}
			throw CompileError(runtime.parentBlock, params[0]->loc, "Assertion failed: MincException(%S) not raised", expectedStr);
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const { return getVoid().type; }
	};
	for (auto tplt: {"assert($E) throws $E<PawsException>", "assert $B throws $E<PawsException>"})
		pkgScope->defineStmt(MincBlockExpr::parseCTplt(tplt), new AssertKernel3());
});