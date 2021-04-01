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
		const MincStackSymbol* varId;
		OperatorKernel(int (*cbk)(int& value), const MincStackSymbol* varId=nullptr) : cbk(cbk), varId(varId) {}
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
		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			MincObject* var = runtime.getStackSymbol(varId);
			runtime.result = new PawsInt(cbk(((PawsInt*)var)->get()));
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};

	// >>> PawsInt expressions

	// Define integer prefix increment
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("++$I<PawsInt>")[0],
		new OperatorKernel([](int& value) -> int {
			return ++value;
		})
	);

	// Define integer prefix decrement
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("--$I<PawsInt>")[0],
		new OperatorKernel([](int& value) -> int {
			return --value;
		})
	);

	// Define integer postfix increment
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt>++")[0],
		new OperatorKernel([](int& value) -> int {
			return value++;
		})
	);

	// Define integer postfix decrement
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt>--")[0],
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

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() + ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> += $E<PawsInt>")[0], new InlineAddKernel());

	// Define in-place integer subtraction
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

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() - ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> -= $E<PawsInt>")[0], new InlineSubKernel());

	// Define in-place integer multiplication
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

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() * ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> *= $E<PawsInt>")[0], new InlineMulKernel());

	// Define in-place integer division
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

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() / ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> /= $E<PawsInt>")[0], new InlineDivKernel());

	// Define in-place bitwise integer AND
	class InlineBitwiseAndKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineBitwiseAndKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineBitwiseAndKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() & ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> &= $E<PawsInt>")[0], new InlineBitwiseAndKernel());

	// Define in-place bitwise integer XOR
	class InlineBitwiseXorKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		InlineBitwiseXorKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[1]->build(buildtime);
			const MincStackSymbol* varId = buildtime.parentBlock->lookupStackSymbol(((MincIdExpr*)params[0])->name);
			if (varId == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", ((MincIdExpr*)params[0])->name);
			return new InlineBitwiseXorKernel(varId);
		}

		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			PawsInt* const val = (PawsInt*)runtime.getStackSymbol(varId);
			if (params[1]->run(runtime))
				return true;
			val->set(val->get() ^ ((PawsInt*)runtime.result)->get());
			runtime.result = val;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$I<PawsInt!> ^= $E<PawsInt>")[0], new InlineBitwiseXorKernel());

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

	// Define random integer generator
	defineExpr(pkgScope, "rand($E<PawsInt>, $E<PawsInt>)",
		+[](int a, int b) -> int {
			return rand() % (b - a) + a;
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
	struct LogicalAndKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const auto a = ((PawsInt*)runtime.result)->get();
			if (!a)
			{
				runtime.result = new PawsInt(0);
				return false;
			}
			if (params[1]->run(runtime))
				return true;
			const auto b = ((PawsInt*)runtime.result)->get();
			runtime.result = new PawsInt(a && b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsInt> && $E<PawsInt>")[0], new LogicalAndKernel());
	struct LogicalOrKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const auto a = ((PawsInt*)runtime.result)->get();
			if (a)
			{
				runtime.result = new PawsInt(1);
				return false;
			}
			if (params[1]->run(runtime))
				return true;
			const auto b = ((PawsInt*)runtime.result)->get();
			runtime.result = new PawsInt(a || b);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsInt::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsInt> || $E<PawsInt>")[0], new LogicalOrKernel());

	// Define boolean negation
	defineExpr(pkgScope, "!$E<PawsInt>",
		+[](int a) -> int {
			return !a;
		}
	);
});