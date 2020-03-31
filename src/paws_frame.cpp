#include "paws_frame_eventloop.h"
#include <cassert>
#include <vector>
#include <list>
#include <limits> // For NaN
#include <cmath> // For isnan()
#include "ast.h" // Including "ast.h" instead of "minc_api.h" for CompileError
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

struct Awaitable : public PawsType
{
private:
	static std::set<Awaitable> awaitableTypes;

protected:
	Awaitable(PawsType* returnType)
		: returnType(returnType) {}

public:
	PawsType* returnType;
	static Awaitable* get(PawsType* returnType);
	Awaitable() = default;
};
std::set<Awaitable> Awaitable::awaitableTypes;
bool operator<(const Awaitable& lhs, const Awaitable& rhs)
{
	return lhs.returnType < rhs.returnType;
}
typedef PawsValue<Awaitable*> PawsAwaitable;

struct Frame : public Awaitable
{
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	BlockExprAST* body;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, BlockExprAST* body)
		: Awaitable(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};
typedef PawsValue<Frame*> PawsFrame;

struct AwaitableInstance
{
private:
	std::list<AwaitableInstance*>* blocked;
	std::mutex mutex;

public:
	Variable result;
	double delay; //TODO: Replace delays with timestamps

	AwaitableInstance(double delay = 0.0) : blocked(new std::list<AwaitableInstance*>()), delay(delay), result(nullptr, nullptr)
	{
		if (std::isnan(delay))
			throw std::invalid_argument("Trying to create awaitable with NaN delay");
	}
	~AwaitableInstance()
	{
		delete blocked;
	}

	bool isDone() { return std::isnan(delay); }
	void wakeup()
	{
		Variable result;
		if (resume(&result))
		{
			mutex.lock();
			this->result = result;
			this->delay = std::numeric_limits<double>::quiet_NaN();
			std::list<AwaitableInstance*>* blocked = this->blocked;
			this->blocked = new std::list<AwaitableInstance*>();
			mutex.unlock();
			for (AwaitableInstance* awaitable: *blocked)
				awaitable->wakeup();
			delete blocked;
		}
	}
	bool awaitResult(AwaitableInstance* awaitable, Variable* result)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (isDone())
		{
			*result = this->result;
			return true;
		}
		else
		{
			blocked->push_back(awaitable);
			return false;
		}
	}

protected:
	virtual bool resume(Variable* result) = 0;
};
typedef PawsValue<AwaitableInstance*> PawsAwaitableInstance;

struct TopLevelInstance : public AwaitableInstance
{
	EventPool* const eventPool;
	TopLevelInstance(EventPool* eventPool) : eventPool(eventPool) {}
protected:
	bool resume(Variable* result)
	{
		eventPool->close();
		*result = Variable(PawsVoid::TYPE, nullptr);
		return true;
	}
};

struct FrameInstance : public AwaitableInstance
{
private:
	const Frame* frame;
	BlockExprAST* instance;

public:
	std::map<std::string, BaseValue*> variables;

	FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs);
	~FrameInstance() { removeBlockExprAST(instance); }

protected:
	bool resume(Variable* result);
};
typedef PawsValue<FrameInstance*> PawsFrameInstance;

struct SleepInstance : public AwaitableInstance
{
	SleepInstance(double duration) : AwaitableInstance(duration) {}
protected:
	bool resume(Variable* result)
	{
		*result = Variable(PawsVoid::TYPE, nullptr);
		return true;
	}
};

struct AwaitException
{
	AwaitableInstance* blocker;
	AwaitException(AwaitableInstance* blocker) : blocker(blocker) {}
};

Awaitable* Awaitable::get(PawsType* returnType)
{
	std::set<Awaitable>::iterator iter = awaitableTypes.find(Awaitable(returnType));
	if (iter == awaitableTypes.end())
	{
		iter = awaitableTypes.insert(Awaitable(returnType)).first;
		Awaitable* t = const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
		defineType(("Awaitable<" + getTypeName(returnType) + '>').c_str(), t);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsOpaqueValue<0>::TYPE);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsAwaitableInstance::TYPE);
	}
	return const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
}

FrameInstance::FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs)
	: frame(frame), instance(cloneBlockExprAST(frame->body))
{
	instance->parent = frame->body;

	// Define arguments in frame instance
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(instance, frame->argNames[i].c_str(), frame->argTypes[i], codegenExpr(argExprs[i], callerScope).value);

	// Define await statement in frame instance scope
	defineExpr3(instance, "await $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			Variable blockerResult;
			if (blocker->awaitResult((FrameInstance*)exprArgs, &blockerResult))
				return blockerResult;
			else
				throw AwaitException(blocker);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			return event->returnType;
		}, this
	);
}

bool FrameInstance::resume(Variable* result)
{
	try
	{
		codegenExpr((ExprAST*)instance, getBlockExprASTParent(instance));
	}
	catch (ReturnException err)
	{
		*result = err.result;
		return true;
	}
	catch (AwaitException err)
	{
		return false;
	}

	if (frame->returnType != getVoid().type && frame->returnType != PawsVoid::TYPE)
		raiseCompileError("missing return statement in frame body", (ExprAST*)instance);
	*result = Variable(PawsVoid::TYPE, nullptr);
	return true;
}

class PawsFramePackage : public MincPackage
{
private:
	EventPool* eventPool;
	void define(BlockExprAST* pkgScope);
public:
	PawsFramePackage() : MincPackage("paws.frame"), eventPool(nullptr) {}
	~PawsFramePackage()
	{
		if (eventPool != nullptr)
			delete eventPool;
	}
} PAWS_FRAME;

void PawsFramePackage::define(BlockExprAST* pkgScope)
{
	eventPool = new EventPool(1);

	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");

	// >>> Type hierarchy
	//
	// Frame instance:		frame -> PawsFrameInstance -\
	//													|--> PawsAwaitableInstance
	// Awaitable instance:	PawsAwaitable<returnType> --/
	//
	// Frame class:			PawsFrame<frame> -> PawsFrame -> PawsAwaitable -> PawsMetaType
	//

	registerType<PawsAwaitable>(pkgScope, "PawsAwaitable");
	registerType<PawsFrame>(pkgScope, "PawsFrame");
	defineOpaqueInheritanceCast(pkgScope, PawsFrame::TYPE, PawsAwaitable::TYPE);
	defineOpaqueInheritanceCast(pkgScope, PawsAwaitable::TYPE, PawsMetaType::TYPE);

	registerType<PawsAwaitableInstance>(pkgScope, "PawsAwaitableInstance");
	registerType<PawsFrameInstance>(pkgScope, "PawsFrameInstance");
	defineOpaqueInheritanceCast(pkgScope, PawsFrameInstance::TYPE, PawsAwaitableInstance::TYPE);

	// Define sleep function
	defineConstantFunction(pkgScope, "sleep", Awaitable::get(PawsVoid::TYPE), { PawsDouble::TYPE }, { "duration" },
		[](BlockExprAST* callerScope, const std::vector<ExprAST*>& args, void* funcArgs) -> Variable
		{
			double duration = ((PawsDouble*)codegenExpr(args[0], callerScope).value)->get();
			SleepInstance* instance = new SleepInstance(duration);
			((EventPool*)funcArgs)->post(bind(&SleepInstance::wakeup, instance), duration);
			return Variable(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(instance));
		}, eventPool
	);

	// Define frame definition
	defineStmt2(pkgScope, "$E<PawsMetaType> frame $I($E<PawsMetaType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			const char* frameName = getIdExprASTName((IdExprAST*)params[1]);
			const std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			const std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);
			BlockExprAST* block = (BlockExprAST*)params[4];

			// Set frame parent to frame definition scope
			setBlockExprASTParent(block, parentBlock);

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType);

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->get());
			frame->argNames.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				frame->argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
			frame->body = block;

			defineType(frameName, frame);
			defineOpaqueInheritanceCast(parentBlock, frame, PawsFrameInstance::TYPE);
			defineOpaqueInheritanceCast(parentBlock, PawsTpltType::get(PawsFrame::TYPE, frame), PawsMetaType::TYPE);
			defineSymbol(parentBlock, frameName, PawsTpltType::get(PawsFrame::TYPE, frame), new PawsFrame(frame));
		}
	);

	// Define frame call
	defineExpr3(pkgScope, "$E<PawsFrame>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Frame* frame = ((PawsFrame*)codegenExpr(params[0], parentBlock).value)->get();
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of frame arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = frame->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						raiseCompileError(
							("invalid frame argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExpr
						);
					}
					argExprs[i] = castExpr;
				}
			}

			// Call frame
			FrameInstance* instance = new FrameInstance(frame, parentBlock, argExprs);
			((EventPool*)exprArgs)->post(bind(&FrameInstance::wakeup, instance), 0.0f);

			return Variable(frame, new PawsFrameInstance(instance));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			Frame* frame = (Frame*)((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
			return frame;
		}, eventPool
	);

	// Define top-level await statement
	defineExpr3(pkgScope, "await $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			EventPool* eventPool = (EventPool*)exprArgs;
			Variable result;

			blocker->awaitResult(new TopLevelInstance(eventPool), &blocker->result);
			eventPool->run();
			return blocker->result;
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			return event->returnType;
		}, eventPool
	);
}