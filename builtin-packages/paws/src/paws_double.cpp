#include <cmath>
#include "minc_api.h"
#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> std::string PawsDouble::Type::toString(MincObject* value) const
{
	return std::to_string(((PawsDouble*)value)->get());
}

MincPackage PAWS_DOUBLE("paws.double", [](MincBlockExpr* pkgScope) {

	struct OperatorKernel : public MincKernel
	{
		double (*cbk)(double& value);
		MincSymbolId varId;
		OperatorKernel(double (*cbk)(double& value), MincSymbolId varId=MincSymbolId::NONE) : cbk(cbk), varId(varId) {}
		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			return new OperatorKernel(cbk, lookupSymbolId(parentBlock, getIdExprName((MincIdExpr*)params[0])));
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}
		MincSymbol run(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(parentBlock, varId);
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(cbk(((PawsDouble*)var->value)->get())));
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};

	// >>> PawsDouble expressions

	// Define double negation
	defineExpr(pkgScope, "-$E<PawsDouble>",
		+[](double f) -> double {
			return -f;
		}
	);

	// Define double prefix increment
	defineExpr6(pkgScope, "++$I<PawsDouble>",
		new OperatorKernel([](double& value) -> double {
			return ++value;
		})
	);

	// Define double prefix decrement
	defineExpr6(pkgScope, "--$I<PawsDouble>",
		new OperatorKernel([](double& value) -> double {
			return --value;
		})
	);

	// Define double postfix increment
	defineExpr6(pkgScope, "$I<PawsDouble>++",
		new OperatorKernel([](double& value) -> double {
			return value++;
		})
	);

	// Define double postfix decrement
	defineExpr6(pkgScope, "$I<PawsDouble>--",
		new OperatorKernel([](double& value) -> double {
			return value--;
		})
	);

	// Define double addition
	defineExpr(pkgScope, "$E<PawsDouble> + $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a + b;
		}
	);

	// Define double subtraction
	defineExpr(pkgScope, "$E<PawsDouble> - $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a - b;
		}
	);

	// Define double multiplication
	defineExpr(pkgScope, "$E<PawsDouble> * $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a * b;
		}
	);

	// Define double division
	defineExpr(pkgScope, "$E<PawsDouble> / $E<PawsDouble>",
		+[](double a, double b) -> double {
			if (b == 0.0)
				throw MincException("Divide by zero exception");
			return a / b;
		}
	);

	// Define in-place double addition
	defineExpr9(pkgScope, "$I<PawsDouble!> += $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() + ((PawsDouble*)runExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double subtraction
	defineExpr9(pkgScope, "$I<PawsDouble!> -= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() - ((PawsDouble*)runExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double multiplication
	defineExpr9(pkgScope, "$I<PawsDouble!> *= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() * ((PawsDouble*)runExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double division
	defineExpr9(pkgScope, "$I<PawsDouble!> /= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() / ((PawsDouble*)runExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define double minimum
	defineExpr(pkgScope, "min($E<PawsDouble>, $E<PawsDouble>)",
		+[](double a, double b) -> double {
			return a < b ? a : b;
		}
	);

	// Define double maximum
	defineExpr(pkgScope, "max($E<PawsDouble>, $E<PawsDouble>)",
		+[](double a, double b) -> double {
			return a > b ? a : b;
		}
	);

	// Define double math functions
	defineExpr(pkgScope, "sqrt($E<PawsDouble>)",
		+[](double f) -> double {
			return sqrt(f);
		}
	);
	defineExpr(pkgScope, "sin($E<PawsDouble>)",
		+[](double f) -> double {
			return sin(f);
		}
	);
	defineExpr(pkgScope, "asin($E<PawsDouble>)",
		+[](double f) -> double {
			return asin(f);
		}
	);
	defineExpr(pkgScope, "cos($E<PawsDouble>)",
		+[](double f) -> double {
			return cos(f);
		}
	);
	defineExpr(pkgScope, "acos($E<PawsDouble>)",
		+[](double f) -> double {
			return acos(f);
		}
	);
	defineExpr(pkgScope, "tan($E<PawsDouble>)",
		+[](double f) -> double {
			return tan(f);
		}
	);
	defineExpr(pkgScope, "atan($E<PawsDouble>)",
		+[](double f) -> double {
			return atan(f);
		}
	);
	defineExpr(pkgScope, "atan2($E<PawsDouble>, $E<PawsDouble>)",
		+[](double y, double x) -> double {
			return atan2(y, x);
		}
	);

	// Define double relations
	defineExpr(pkgScope, "$E<PawsDouble> == $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a == b;
		}
	);
	defineExpr(pkgScope, "$E<PawsDouble> != $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a != b;
		}
	);
	defineExpr(pkgScope, "$E<PawsDouble> <= $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a <= b;
		}
	);
	defineExpr(pkgScope, "$E<PawsDouble> >= $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a >= b;
		}
	);
	defineExpr(pkgScope, "$E<PawsDouble> << $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a < b;
		}
	);
	defineExpr(pkgScope, "$E<PawsDouble> >> $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a > b;
		}
	);

	// Define logical operators
	defineExpr9(pkgScope, "$E<PawsDouble> && $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(
					((PawsDouble*)runExpr(params[0], parentBlock).value)->get() &&
					((PawsDouble*)runExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsDouble::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsDouble> || $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(
					((PawsDouble*)runExpr(params[0], parentBlock).value)->get() ||
					((PawsDouble*)runExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsDouble::TYPE
	);
});