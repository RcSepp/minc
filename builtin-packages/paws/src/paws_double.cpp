#include <cmath>
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
			const std::string& name = ((MincIdExpr*)params[0])->name;
			const MincStackSymbol* stackSymbol = buildtime.parentBlock->lookupStackSymbol(name);
			if (stackSymbol == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", name);
			return new OperatorKernel(cbk, stackSymbol);
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}
		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			MincObject* var = runtime.parentBlock->getStackSymbol(runtime, varId);
			runtime.result = new PawsDouble(cbk(((PawsDouble*)var)->get()));
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
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("++$I<PawsDouble>")[0],
		new OperatorKernel([](double& value) -> double {
			return ++value;
		})
	);

	// Define double prefix decrement
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("--$I<PawsDouble>")[0],
		new OperatorKernel([](double& value) -> double {
			return --value;
		})
	);

	// Define double postfix increment
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble>++")[0],
		new OperatorKernel([](double& value) -> double {
			return value++;
		})
	);

	// Define double postfix decrement
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble>--")[0],
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
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineAddKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)runtime.parentBlock->getStackSymbol(runtime, varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() + ((PawsDouble*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble!> += $E<PawsDouble>")[0], new InlineAddKernel());

	// Define in-place double subtraction
	class InlineSubKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineSubKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineSubKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)runtime.parentBlock->getStackSymbol(runtime, varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() - ((PawsDouble*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble!> -= $E<PawsDouble>")[0], new InlineSubKernel());

	// Define in-place double multiplication
	class InlineMulKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineMulKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineMulKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)runtime.parentBlock->getStackSymbol(runtime, varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() * ((PawsDouble*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble!> *= $E<PawsDouble>")[0], new InlineMulKernel());

	// Define in-place double division
	class InlineDivKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineDivKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineDivKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			PawsDouble* const val = (PawsDouble*)runtime.parentBlock->getStackSymbol(runtime, varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() / ((PawsDouble*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsDouble!> /= $E<PawsDouble>")[0], new InlineDivKernel());

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
	struct LogicalAndKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const auto a = ((PawsDouble*)runtime.result)->get();
			if (!a)
			{
				runtime.result = new PawsDouble(0);
				return false;
			}
			if (params[1]->run(runtime))
				return true;
			const auto b = ((PawsDouble*)runtime.result)->get();
			runtime.result = new PawsDouble(a && b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsDouble> && $E<PawsDouble>")[0], new LogicalAndKernel());
	struct LogicalOrKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const auto a = ((PawsDouble*)runtime.result)->get();
			if (a)
			{
				runtime.result = new PawsDouble(1);
				return false;
			}
			if (params[1]->run(runtime))
				return true;
			const auto b = ((PawsDouble*)runtime.result)->get();
			runtime.result = new PawsDouble(a || b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsDouble::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsDouble> || $E<PawsDouble>")[0], new LogicalOrKernel());
});