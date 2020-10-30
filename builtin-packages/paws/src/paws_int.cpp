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
		MincKernel* build(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			return new OperatorKernel(cbk, lookupSymbolId(parentBlock, getIdExprName((MincIdExpr*)params[0])));
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}
		MincSymbol codegen(MincBlockExpr* parentBlock, std::vector<MincExpr*>& params)
		{
			MincSymbol* var = getSymbol(parentBlock, varId);
			return MincSymbol(PawsInt::TYPE, new PawsInt(cbk(((PawsInt*)var->value)->get())));
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			val->set(val->get() + ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsInt::TYPE
	);

	// Define in-place integer subtraction
	defineExpr9(pkgScope, "$I<PawsInt!> -= $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			val->set(val->get() - ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsInt::TYPE
	);

	// Define in-place integer multiplication
	defineExpr9(pkgScope, "$I<PawsInt!> *= $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			val->set(val->get() * ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
		},
		PawsInt::TYPE
	);

	// Define in-place integer division
	defineExpr9(pkgScope, "$I<PawsInt!> /= $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			//TODO: import var at build time
			MincSymbol* var = importSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]));
			PawsInt* const val = (PawsInt*)var->value;
			val->set(val->get() / ((PawsInt*)codegenExpr(params[1], parentBlock).value)->get());
			return *var;
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
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			return MincSymbol(PawsInt::TYPE, new PawsInt(
					((PawsInt*)codegenExpr(params[0], parentBlock).value)->get() &&
					((PawsInt*)codegenExpr(params[1], parentBlock).value)->get()
			));
		},
		PawsInt::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsInt> || $E<PawsInt>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			buildExpr(params[0], parentBlock);
			buildExpr(params[1], parentBlock);
		},
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