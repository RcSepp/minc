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

	// Define integer prefix increment
	defineExpr2(pkgScope, "++$I<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			++((PawsDouble*)var->value)->get();
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define integer prefix decrement
	defineExpr2(pkgScope, "--$I<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			--((PawsDouble*)var->value)->get();
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define integer postfix increment
	defineExpr2(pkgScope, "$I<PawsDouble>++",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(((PawsDouble*)var->value)->get()++));
		},
		PawsDouble::TYPE
	);

	// Define integer postfix decrement
	defineExpr2(pkgScope, "$I<PawsDouble>--",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsDouble::TYPE, new PawsDouble(((PawsDouble*)var->value)->get()--));
		},
		PawsDouble::TYPE
	);

	// Define integer addition
	defineExpr(pkgScope, "$E<PawsDouble> + $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a + b;
		}
	);

	// Define integer subtraction
	defineExpr(pkgScope, "$E<PawsDouble> - $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a - b;
		}
	);

	// Define integer multiplication
	defineExpr(pkgScope, "$E<PawsDouble> * $E<PawsDouble>",
		+[](double a, double b) -> double {
			return a * b;
		}
	);

	// Define integer division
	defineExpr(pkgScope, "$E<PawsDouble> / $E<PawsDouble>",
		+[](double a, double b) -> double {
			if (b == 0)
				throw MincException("Divide by zero exception");
			return a / b;
		}
	);

	// Define in-place integer addition
	defineExpr2(pkgScope, "$I<PawsDouble!> += $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() + ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place integer subtraction
	defineExpr2(pkgScope, "$I<PawsDouble!> -= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() - ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place integer multiplication
	defineExpr2(pkgScope, "$I<PawsDouble!> *= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() * ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define in-place integer division
	defineExpr2(pkgScope, "$I<PawsDouble!> /= $E<PawsDouble>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsDouble* const val = (PawsDouble*)var->value;
			val->set(val->get() / ((PawsDouble*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsDouble::TYPE
	);

	// Define integer minimum
	defineExpr(pkgScope, "min($E<PawsDouble>, $E<PawsDouble>)",
		+[](double a, double b) -> double {
			return a < b ? a : b;
		}
	);

	// Define integer maximum
	defineExpr(pkgScope, "max($E<PawsDouble>, $E<PawsDouble>)",
		+[](double a, double b) -> double {
			return a > b ? a : b;
		}
	);

	// Define double sqrt
	defineExpr(pkgScope, "sqrt($E<PawsDouble>)",
		+[](double f) -> double {
			return sqrt(f);
		}
	);

	// Define integer relations
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