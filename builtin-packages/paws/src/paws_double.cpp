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

	// >>> PawsDouble expressions

	// Define double negation
	defineExpr(pkgScope, "-$E<PawsDouble>",
		+[](double f) -> double {
			return -f;
		}
	);

	// Define double prefix increment
	defineExpr2(pkgScope, "++$I<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			++((PawsDouble*)var->value)->get();
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define double prefix decrement
	defineExpr2(pkgScope, "--$I<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			--((PawsDouble*)var->value)->get();
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define double postfix increment
	defineExpr2(pkgScope, "$I<PawsDouble>++",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(((PawsDouble*)var->value)->get()++));
		},
		PawsDouble::TYPE
	);

	// Define double postfix decrement
	defineExpr2(pkgScope, "$I<PawsDouble>--",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(((PawsDouble*)var->value)->get()--));
		},
		PawsDouble::TYPE
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
			if (b == 0)
				throw MincException("Divide by zero exception");
			return a / b;
		}
	);

	// Define in-place double addition
	defineExpr2(pkgScope, "$I<PawsDouble!> += $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() + ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double subtraction
	defineExpr2(pkgScope, "$I<PawsDouble!> -= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() - ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double multiplication
	defineExpr2(pkgScope, "$I<PawsDouble!> *= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() * ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place double division
	defineExpr2(pkgScope, "$I<PawsDouble!> /= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() / ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
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
	defineExpr(pkgScope, "sqrt($E<PawsDouble>)", +[](double f) -> double { return sqrt(f); } );
	defineExpr(pkgScope, "sin($E<PawsDouble>)", +[](double f) -> double { return sin(f); } );
	defineExpr(pkgScope, "cos($E<PawsDouble>)", +[](double f) -> double { return cos(f); } );
	defineExpr(pkgScope, "tan($E<PawsDouble>)", +[](double f) -> double { return tan(f); } );

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
	defineExpr2(pkgScope, "$E<PawsDouble> && $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(
					((PawsDouble*)codegenExpr(params[0], parentBlock).value)->get() &&
					((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsDouble::TYPE
	);
	defineExpr2(pkgScope, "$E<PawsDouble> || $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(
					((PawsDouble*)codegenExpr(params[0], parentBlock).value)->get() ||
					((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsDouble::TYPE
	);
});