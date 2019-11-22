#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

MincPackage PAWS_INT("paws.int", [](BlockExprAST* pkgScope) {

	// >>> PawsInt expressions

	// Define integer prefix increment
	defineExpr2(pkgScope, "++$I<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			++((PawsInt*)var->value)->get();
			return *var;
		},
		PawsInt::TYPE
	);

	// Define integer prefix decrement
	defineExpr2(pkgScope, "--$I<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			--((PawsInt*)var->value)->get();
			return *var;
		},
		PawsInt::TYPE
	);

	// Define integer postfix increment
	defineExpr2(pkgScope, "$I<PawsInt>++",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			return Variable(PawsInt::TYPE, new PawsInt(((PawsInt*)var->value)->get()++));
		},
		PawsInt::TYPE
	);

	// Define integer postfix decrement
	defineExpr2(pkgScope, "$I<PawsInt>--",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable* var = importSymbol(parentBlock, getIdExprASTName((IdExprAST*)params[0]));
			return Variable(PawsInt::TYPE, new PawsInt(((PawsInt*)var->value)->get()--));
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
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() &&
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsInt::TYPE
	);
	defineExpr2(pkgScope, "$E<PawsInt> || $E<PawsInt>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			return Variable(PawsInt::TYPE, new PawsInt(
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