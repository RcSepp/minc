#include "minc_api.h"
#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> std::string PawsInt::Type::toString(MincObject* value) const
{
	return std::to_string(((PawsInt*)value)->get());
}

MincPackage PAWS_INT("paws.int", [](MincBlockExpr* pkgScope) {

	struct OperatorKernel : public MincKernel
	{
		int (*cbk)(int& value);
		MincSymbolId varId;
		OperatorKernel(int (*cbk)(int& value), MincSymbolId varId=MincSymbolId::NONE) : cbk(cbk), varId(varId) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			return new OperatorKernel(cbk, lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0])));
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(runtime.parentBlock, varId);
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(cbk(((PawsInt*)var->value)->get())));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};

	// >>> PawsInt expressions

	// Define integer prefix increment
	defineExpr6(pkgScope, "++$I<PawsInt>",
		new OperatorKernel([](int& value) -> int {
			return ++value;
		})
	);

	// Define integer prefix decrement
	defineExpr6(pkgScope, "--$I<PawsInt>",
		new OperatorKernel([](int& value) -> int {
			return --value;
		})
	);

	// Define integer postfix increment
	defineExpr6(pkgScope, "$I<PawsInt>++",
		new OperatorKernel([](int& value) -> int {
			return value++;
		})
	);

	// Define integer postfix decrement
	defineExpr6(pkgScope, "$I<PawsInt>--",
		new OperatorKernel([](int& value) -> int {
			return value--;
		})
	);

	// Define integer addition
	defineExpr(pkgScope, "$E<PawsInt> + $E<PawsInt>",
		+[](int a, int b) -> int {
			return a + b;
		}
	);

	// Define integer subtraction
	defineExpr(pkgScope, "$E<PawsInt> - $E<PawsInt>",
		+[](int a, int b) -> int {
			return a - b;
		}
	);

	// Define integer multiplication
	defineExpr(pkgScope, "$E<PawsInt> * $E<PawsInt>",
		+[](int a, int b) -> int {
			return a * b;
		}
	);

	// Define integer division
	defineExpr(pkgScope, "$E<PawsInt> / $E<PawsInt>",
		+[](int a, int b) -> int {
			if (b == 0)
				throw MincException("Divide by zero exception");
			return a / b;
		}
	);

	// Define in-place integer addition
	class InlineAddKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		InlineAddKernel(MincSymbolId varId=MincSymbolId::NONE) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == MincSymbolId::NONE)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineAddKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(runtime.parentBlock, varId);
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() + ((PawsInt*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsInt!> += $E<PawsInt>", new InlineAddKernel());

	// Define in-place integer subtraction
	class InlineSubKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		InlineSubKernel(MincSymbolId varId=MincSymbolId::NONE) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == MincSymbolId::NONE)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineSubKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(runtime.parentBlock, varId);
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() - ((PawsInt*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsInt!> -= $E<PawsInt>", new InlineSubKernel());

	// Define in-place integer multiplication
	class InlineMulKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		InlineMulKernel(MincSymbolId varId=MincSymbolId::NONE) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == MincSymbolId::NONE)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineMulKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(runtime.parentBlock, varId);
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() * ((PawsInt*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsInt!> *= $E<PawsInt>", new InlineMulKernel());

	// Define in-place integer division
	class InlineDivKernel : public MincKernel
	{
		const MincSymbolId varId;
	public:
		InlineDivKernel(MincSymbolId varId=MincSymbolId::NONE) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			MincSymbolId varId = lookupSymbolId(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == MincSymbolId::NONE)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineDivKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(runtime.parentBlock, varId);
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() / ((PawsInt*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsInt!> /= $E<PawsInt>", new InlineDivKernel());

	// Define integer minimum
	defineExpr(pkgScope, "min($E<PawsInt>, $E<PawsInt>)",
		+[](int a, int b) -> int {
			return a < b ? a : b;
		}
	);

	// Define integer maximum
	defineExpr(pkgScope, "max($E<PawsInt>, $E<PawsInt>)",
		+[](int a, int b) -> int {
			return a > b ? a : b;
		}
	);

	// Define integer relations
	defineExpr(pkgScope, "$E<PawsInt> == $E<PawsInt>",
		+[](int a, int b) -> int {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> != $E<PawsInt>",
		+[](int a, int b) -> int {
			return a != b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> <= $E<PawsInt>",
		+[](int a, int b) -> int {
			return a <= b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> >= $E<PawsInt>",
		+[](int a, int b) -> int {
			return a >= b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> << $E<PawsInt>",
		+[](int a, int b) -> int {
			return a < b;
		}
	);
	defineExpr(pkgScope, "$E<PawsInt> >> $E<PawsInt>",
		+[](int a, int b) -> int {
			return a > b;
		}
	);

	// Define logical operators
	defineExpr9(pkgScope, "$E<PawsInt> && $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const auto a = ((PawsInt*)runtime.result.value)->get();
			if (!a)
			{
				runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(0));
				return false;
			}
			if (runExpr(params[1], runtime))
				return true;
			const auto b = ((PawsInt*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a && b));
			return false;
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsInt> || $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const auto a = ((PawsInt*)runtime.result.value)->get();
			if (a)
			{
				runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(1));
				return false;
			}
			if (runExpr(params[1], runtime))
				return true;
			const auto b = ((PawsInt*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsInt::TYPE, new PawsInt(a || b));
			return false;
		},
		PawsInt::TYPE
	);

	// Define boolean negation
	defineExpr(pkgScope, "!$E<PawsInt>",
		+[](int a) -> int {
			return !a;
		}
	);
});