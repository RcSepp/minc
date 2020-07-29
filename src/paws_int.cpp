#include "minc_api.h"
#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> const std::string PawsInt::toString() const
{
	return std::to_string(val);
}

MincPackage PAWS_INT("paws.int", [](MincBlockExpr* pkgScope) {

	// >>> PawsInt expressions

	// Define integer prefix increment
	defineExpr2(pkgScope, "++$I<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			++((PawsInt*)var->value)->get();
			return *var;
		},
		PawsInt::TYPE
	);

	// Define integer prefix decrement
	defineExpr2(pkgScope, "--$I<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			--((PawsInt*)var->value)->get();
			return *var;
		},
		PawsInt::TYPE
	);

	// Define integer postfix increment
	defineExpr2(pkgScope, "$I<PawsInt>++",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsInt::TYPE, new PawsInt(((PawsInt*)var->value)->get()++));
		},
		PawsInt::TYPE
	);

	// Define integer postfix decrement
	defineExpr2(pkgScope, "$I<PawsInt>--",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			return MincSymbol(PawsInt::TYPE, new PawsInt(((PawsInt*)var->value)->get()--));
		},
		PawsInt::TYPE
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

	// Define integer addition
	defineExpr(pkgScope, "$E<PawsInt> * $E<PawsInt>",
		+[](int a, int b) -> int {
			return a * b;
		}
	);

	// Define integer subtraction
	defineExpr(pkgScope, "$E<PawsInt> / $E<PawsInt>",
		+[](int a, int b) -> int {
			if (b == 0)
				throw MincException("Divide by zero exception");
			return a / b;
		}
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
	defineExpr2(pkgScope, "$E<PawsInt> && $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() &&
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsInt::TYPE
	);
	defineExpr2(pkgScope, "$E<PawsInt> || $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() ||
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->get()
			));
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