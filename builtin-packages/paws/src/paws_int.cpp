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
	defineExpr9(pkgScope, "$I<PawsInt!> += $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(runtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() + ((PawsInt*)runtime.result.value)->get());
			runtime.result = *var;
			return false;
		},
		PawsInt::TYPE
	);

	// Define in-place integer subtraction
	defineExpr9(pkgScope, "$I<PawsInt!> -= $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(runtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() - ((PawsInt*)runtime.result.value)->get());
			runtime.result = *var;
			return false;
		},
		PawsInt::TYPE
	);

	// Define in-place integer multiplication
	defineExpr9(pkgScope, "$I<PawsInt!> *= $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(runtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() * ((PawsInt*)runtime.result.value)->get());
			runtime.result = *var;
			return false;
		},
		PawsInt::TYPE
	);

	// Define in-place integer division
	defineExpr9(pkgScope, "$I<PawsInt!> /= $E<PawsInt>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(runtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() / ((PawsInt*)runtime.result.value)->get());
			runtime.result = *var;
			return false;
		},
		PawsInt::TYPE
	);

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