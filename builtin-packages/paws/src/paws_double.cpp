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
		const MincStackSymbol* varId;
		OperatorKernel(double (*cbk)(double& value), const MincStackSymbol* varId=nullptr) : cbk(cbk), varId(varId) {}
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const char* name = getIdExprName((MincIdExpr*)params[0]);
			const MincStackSymbol* stackSymbol = lookupStackSymbol(buildtime.parentBlock, name);
			if (stackSymbol == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", name);
			return new OperatorKernel(cbk, stackSymbol);
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincObject* var = getStackSymbol(runtime.parentBlock, runtime, varId);
			runtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(cbk(((PawsDouble*)var)->get())));
			return false;
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
	class InlineAddKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineAddKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			const MincStackSymbol* varId = lookupStackSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineAddKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)getStackSymbol(runtime.parentBlock, runtime, varId);
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() + ((PawsDouble*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsDouble!> += $E<PawsDouble>", new InlineAddKernel());

	// Define in-place double subtraction
	class InlineSubKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineSubKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			const MincStackSymbol* varId = lookupStackSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineSubKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)getStackSymbol(runtime.parentBlock, runtime, varId);
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() - ((PawsDouble*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsDouble!> -= $E<PawsDouble>", new InlineSubKernel());

	// Define in-place double multiplication
	class InlineMulKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineMulKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			const MincStackSymbol* varId = lookupStackSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineMulKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)getStackSymbol(runtime.parentBlock, runtime, varId);
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() * ((PawsDouble*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsDouble!> *= $E<PawsDouble>", new InlineMulKernel());

	// Define in-place double division
	class InlineDivKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineDivKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			buildExpr(params[1], buildtime);
			const MincStackSymbol* varId = lookupStackSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]));
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, getLocation(params[0]), "`%s` was not declared in this scope", getIdExprName((MincIdExpr*)params[0]));
			return new InlineDivKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)getStackSymbol(runtime.parentBlock, runtime, varId);
			if (runExpr(params[1], runtime))
				return true;
			val->set(val->get() / ((PawsDouble*)runtime.result.value)->get());
			runtime.result.value = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	defineExpr6(pkgScope, "$I<PawsDouble!> /= $E<PawsDouble>", new InlineDivKernel());

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
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const auto a = ((PawsDouble*)runtime.result.value)->get();
			if (!a)
			{
				runtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(0));
				return false;
			}
			if (runExpr(params[1], runtime))
				return true;
			const auto b = ((PawsDouble*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(a && b));
			return false;
		},
		PawsDouble::TYPE
	);
	defineExpr9(pkgScope, "$E<PawsDouble> || $E<PawsDouble>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
			buildExpr(params[1], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			const auto a = ((PawsDouble*)runtime.result.value)->get();
			if (a)
			{
				runtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(1));
				return false;
			}
			if (runExpr(params[1], runtime))
				return true;
			const auto b = ((PawsDouble*)runtime.result.value)->get();
			runtime.result = MincSymbol(PawsDouble::TYPE, new PawsDouble(a || b));
			return false;
		},
		PawsDouble::TYPE
	);
});