#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

template<> std::string PawsException::Type::toString(MincObject* value) const
{
	return "PawsException(\"" + std::string(((PawsException*)value)->get().what()) + "\")";
}

MincPackage PAWS_EXCEPTION("paws.exception", [](MincBlockExpr* pkgScope) {
	registerType<PawsException>(pkgScope, "PawsException");

	// Define try statement
	struct TryCatchKernel1 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			try
			{
				if (!params[0]->run(runtime))
					return false;
			}
			catch (const MincException&)
			{
				return params[1]->run(runtime);
			}
			catch (const MincSymbol& err)
			{
				if (err.type == &PAWS_RETURN_TYPE || err.type == &PAWS_AWAIT_TYPE)
					return true;
				return params[1]->run(runtime);
			}
			if (runtime.exceptionType == &PAWS_RETURN_TYPE || runtime.exceptionType == &PAWS_AWAIT_TYPE)
				return true;
			return params[1]->run(runtime);
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("try $S catch $S"), new TryCatchKernel1());

	class TryCatchKernel2 : public MincKernel
	{
		const MincStackSymbol* const stackSymbol;
	public:
		TryCatchKernel2(const MincStackSymbol* stackSymbol=nullptr) : stackSymbol(stackSymbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			PawsType* const catchType = (PawsType*)params[1]->build(buildtime).value;
			if (catchType == nullptr)
				throw CompileError(buildtime.parentBlock, params[1]->loc, "catch type must be build time constant");
			const MincStackSymbol* stackSymbol = ((MincBlockExpr*)params[3])->allocStackSymbol(((MincIdExpr*)params[2])->name, catchType, catchType->size);
			params[3]->build(buildtime);
			return new TryCatchKernel2(stackSymbol);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[1]->run(runtime)) //TODO: Consider storing this in the built kernel
				return true;
			MincObject* const catchType = runtime.result;

			try
			{
				if (!params[0]->run(runtime))
					return false;
			}
			catch (const MincException& err)
			{
				if (runtime.parentBlock->isInstance(PawsException::TYPE, catchType))
				{
					MincObject* errValue = runtime.getStackSymbolOfNextStackFrame(stackSymbol);
					new(errValue) MincException(err);
					return params[3]->run(runtime);
				}
				else
					throw;
			}
			catch (const MincSymbol& err)
			{
				if (runtime.parentBlock->isInstance(err.type, catchType))
				{
					MincObject* errValue = runtime.getStackSymbolOfNextStackFrame(stackSymbol);
					((PawsType*)stackSymbol->type)->copyToNew(err.value, errValue);
					return params[3]->run(runtime);
				}
				else
				{
					runtime.exceptionType = err.type;
					runtime.result = err.value;
					return true;
				}
			}
			if (runtime.parentBlock->isInstance(runtime.exceptionType, catchType))
			{
				MincObject* errValue = runtime.getStackSymbolOfNextStackFrame(stackSymbol);
				((PawsType*)stackSymbol->type)->copyToNew(runtime.result, errValue);
				return params[3]->run(runtime);
			}
			else
				return true;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("try $S catch ($E<PawsType> $I) $B"), new TryCatchKernel2());

	// Define throw statement
	class ThrowKernel : public MincKernel
	{
		MincObject* const exceptionType;
	public:
		ThrowKernel(MincObject* exceptionType=nullptr) : exceptionType(exceptionType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return new ThrowKernel(params[0]->getType(buildtime.parentBlock));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			params[0]->run(runtime);
			runtime.exceptionType = exceptionType;
			return true;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("throw $E"), new ThrowKernel());

	// Define constructors
	defineExpr(pkgScope, "PawsException()",
		+[]() -> MincException {
			return MincException();
		}
	);
	struct ExceptionConstructorKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			if (buildtime.result.value != nullptr)
			{
				const std::string& msg = ((PawsString*)buildtime.result.value)->get();
				buildtime.result.value = new PawsException(MincException(msg, params[0]->loc));
			}
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const std::string& msg = ((PawsString*)runtime.result)->get();
			runtime.result = new PawsException(MincException(msg, params[0]->loc));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsException::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("PawsException($E<PawsString>)")[0], new ExceptionConstructorKernel());

	// Define msg getter
	defineExpr(pkgScope, "$E<PawsException>.msg",
		+[](MincException err) -> std::string {
			const char* msg = err.what();
			return msg != nullptr ? msg : "";
		}
	);
});