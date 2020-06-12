#include "paws_frame_eventloop.h"
#include <cassert>
#include <vector>
#include <list>
#include <queue>
#include <limits> // For NaN
#include <cmath> // For isnan()
#include "ast.h" // Including "ast.h" instead of "minc_api.h" for CompileError
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

struct Awaitable : public PawsType
{
private:
	static std::mutex mutex;
	static std::set<Awaitable> awaitableTypes;

protected:
	Awaitable(PawsType* returnType)
		: returnType(returnType) {}

public:
	PawsType* returnType;
	static Awaitable* get(PawsType* returnType);
	Awaitable() = default;
};
std::mutex Awaitable::mutex;
std::set<Awaitable> Awaitable::awaitableTypes;
bool operator<(const Awaitable& lhs, const Awaitable& rhs)
{
	return lhs.returnType < rhs.returnType;
}
typedef PawsValue<Awaitable*> PawsAwaitable;

struct Event : public PawsType
{
private:
	static std::mutex mutex;
	static std::set<Event> eventTypes;

protected:
	Event(PawsType* msgType)
		: msgType(msgType) {}

public:
	PawsType* msgType;
	static Event* get(PawsType* msgType);
	Event() = default;
};
std::mutex Event::mutex;
std::set<Event> Event::eventTypes;
bool operator<(const Event& lhs, const Event& rhs)
{
	return lhs.msgType < rhs.msgType;
}
typedef PawsValue<Event*> PawsEvent;

struct Frame : public Awaitable
{
	struct Variable
	{
		PawsType* type;
		ExprAST* initExpr;
	};

	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	std::map<std::string, Variable> variables;
	BlockExprAST* body;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, BlockExprAST* body)
		: Awaitable(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};
typedef PawsValue<Frame*> PawsFrame;

struct SingleshotAwaitableInstance;
struct AwaitableInstance
{
	virtual bool awaitResult(SingleshotAwaitableInstance* awaitable, Variable* result) = 0;
	virtual bool getResult(Variable* result) = 0;
};
typedef PawsValue<AwaitableInstance*> PawsAwaitableInstance;

struct SingleshotAwaitableInstance : public AwaitableInstance
{
private:
	std::list<SingleshotAwaitableInstance*>* blocked;
	std::mutex mutex;

public:
	Variable result;
	double delay; //TODO: Replace delays with timestamps

	SingleshotAwaitableInstance(double delay = 0.0) : blocked(new std::list<SingleshotAwaitableInstance*>()), delay(delay), result(nullptr, nullptr)
	{
		if (std::isnan(delay))
			throw std::invalid_argument("Trying to create awaitable with NaN delay");
	}
	~SingleshotAwaitableInstance()
	{
		delete blocked;
	}

	bool isDone() { return std::isnan(delay); }
	void wakeup(Variable result)
	{
		if (resume(&result))
		{
			mutex.lock();
			this->result = result;
			this->delay = std::numeric_limits<double>::quiet_NaN();
			std::list<SingleshotAwaitableInstance*>* blocked = this->blocked;
			this->blocked = new std::list<SingleshotAwaitableInstance*>();
			mutex.unlock();
			for (SingleshotAwaitableInstance* awaitable: *blocked)
				awaitable->wakeup(result);
			delete blocked;
		}
	}
	bool awaitResult(SingleshotAwaitableInstance* awaitable, Variable* result)
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
	bool getResult(Variable* result)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (isDone())
		{
			*result = this->result;
			return true;
		}
		else
			return false;
	}

protected:
	virtual bool resume(Variable* result) { return true; }
};

struct TopLevelInstance : public SingleshotAwaitableInstance
{
private:
	EventPool* const eventPool;

public:
	TopLevelInstance(EventPool* eventPool) : eventPool(eventPool) {}

protected:
	bool resume(Variable* result)
	{
		eventPool->close();
		return true;
	}
};

struct FrameInstance : public SingleshotAwaitableInstance
{
private:
	const Frame* frame;
	BlockExprAST* instance;
	EventPool* const eventPool;

public:
	std::map<std::string, BaseValue*> variables;

	FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs, EventPool* eventPool);
	~FrameInstance() { removeBlockExprAST(instance); }

protected:
	bool resume(Variable* result);
};
typedef PawsValue<FrameInstance*> PawsFrameInstance;

struct AnyAwaitableInstance : public SingleshotAwaitableInstance
{
protected:
	AwaitableInstance *const a, *const b;

public:
	AnyAwaitableInstance(AwaitableInstance* a, AwaitableInstance* b) : a(a), b(b)
	{
		Variable result;
		if (a->awaitResult(this, &result) || b->awaitResult(this, &result))
			wakeup(result);
	}

protected:
	bool resume(Variable* result)
	{
		*result = Variable(PawsVoid::TYPE, nullptr);
		return true;
	}
};

struct AllAwaitableInstance : public SingleshotAwaitableInstance
{
protected:
	AwaitableInstance *const a, *const b;

public:
	AllAwaitableInstance(AwaitableInstance* a, AwaitableInstance* b) : a(a), b(b)
	{
		Variable result;
		if (a->awaitResult(this, &result) && b->awaitResult(this, &result))
			wakeup(result);
	}

protected:
	bool resume(Variable* result)
	{
		*result = Variable(PawsVoid::TYPE, nullptr);
		return true;
	}
};

struct EventInstance : public AwaitableInstance
{
private:
	struct Message
	{
		BaseValue* value;
		SingleshotAwaitableInstance* invokeInstance;
	};
	PawsType* const type;
	std::mutex mutex; // Mutex protecting blockedAwaitable, blockedAwaitableResult and msgQueue
	SingleshotAwaitableInstance* blockedAwaitable;
	Variable blockedAwaitableResult;
	std::queue<Message> msgQueue;
	EventPool* const eventPool;

public:
	EventInstance(PawsType* type, EventPool* eventPool) : type(type), eventPool(eventPool), blockedAwaitable(nullptr) {}
	void invoke(SingleshotAwaitableInstance* invokeInstance, BaseValue* value)
	{
		mutex.lock();
		msgQueue.push(Message{value, invokeInstance}); // Queue message
		if (blockedAwaitable) // If this event is being awaited, ...
		{
			mutex.unlock();
			blockedAwaitable->wakeup(blockedAwaitableResult = Variable(type, value)); // Wake up waiting awaitable
		}
		else
			mutex.unlock();
	}
	bool awaitResult(SingleshotAwaitableInstance* awaitable, Variable* result)
	{
		mutex.lock();

		if (blockedAwaitable == nullptr) // If this event is not currently awaited
			blockedAwaitable = awaitable; // Remember calling awaitable
		else if (blockedAwaitable != awaitable) // If this event is already being awaited by another awaitable, ...
		{
			mutex.unlock();
			raiseCompileError("Event awaited more than once", {}); //TODO: Raise runtime exception instead
		}

		if (msgQueue.empty()) // If there are no messages in the queue, ...
		{
			mutex.unlock();
			return false;
		}
		else // If there are messages in the queue, ...
		{
			Message msg = msgQueue.front();
			msgQueue.pop();
			mutex.unlock();
			*result = Variable(type, msg.value); // Return first queued message
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, Variable(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
			return true;
		}
	}
	bool getResult(Variable* result)
	{
		mutex.lock();

		if (msgQueue.empty()) // If there are no messages in the queue, ...
		{
			mutex.unlock();
			return false;
		}
		else // If there are messages in the queue, ...
		{
			const Message& msg = msgQueue.front();
			msgQueue.pop();
			mutex.unlock();
			*result = Variable(type, msg.value); // Return first queued message
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, Variable(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
			return true;
		}
	}
};
typedef PawsValue<EventInstance*> PawsEventInstance;

struct AwaitException {};

Awaitable* Awaitable::get(PawsType* returnType)
{
	std::unique_lock<std::mutex> lock(mutex);
	std::set<Awaitable>::iterator iter = awaitableTypes.find(Awaitable(returnType));
	if (iter == awaitableTypes.end())
	{
		iter = awaitableTypes.insert(Awaitable(returnType)).first;
		Awaitable* t = const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
		defineType(("Awaitable<" + getTypeName(returnType) + '>').c_str(), t);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsAwaitableInstance::TYPE);
	}
	return const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
}

Event* Event::get(PawsType* msgType)
{
	std::unique_lock<std::mutex> lock(mutex);
	std::set<Event>::iterator iter = eventTypes.find(Event(msgType));
	if (iter == eventTypes.end())
	{
		iter = eventTypes.insert(Event(msgType)).first;
		Event* t = const_cast<Event*>(&*iter); //TODO: Find a way to avoid const_cast
		defineType(("Event<" + getTypeName(msgType) + '>').c_str(), t);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(getRootScope(), t, PawsEventInstance::TYPE);
defineOpaqueInheritanceCast(getRootScope(), PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE); //TODO: This shouldn't be necessary
	}
	return const_cast<Event*>(&*iter); //TODO: Find a way to avoid const_cast
}

FrameInstance::FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs, EventPool* eventPool)
	: frame(frame), instance(cloneBlockExprAST(frame->body)), eventPool(eventPool)
{
	instance->parent = frame->body;

	// Define arguments in frame instance
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(instance, frame->argNames[i].c_str(), frame->argTypes[i], ((PawsBase*)codegenExpr(argExprs[i], callerScope).value)->copy());

	// Initialize and define frame variables in frame instance
	for (const std::pair<const std::string, Frame::Variable>& pair: frame->variables)
	{
		BaseValue* const value = codegenExpr(pair.second.initExpr, frame->body).value;
		defineSymbol(instance, pair.first.c_str(), pair.second.type, value);
		variables[pair.first] = value;
	}

	// Define await statement in frame instance scope
	defineExpr3(instance, "await $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			Variable blockerResult;
			if (blocker->awaitResult((FrameInstance*)exprArgs, &blockerResult))
				return blockerResult;
			else
				throw AwaitException();
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			return event->returnType;
		}, this
	);
}

bool FrameInstance::resume(Variable* result)
{
	// Avoid executing instance while it's being executed by another thread
	// This happens when a frame is resumed by a new thread while the old thread is still busy unrolling the AwaitException
	if (isBlockExprASTBusy(instance))
	{
		eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, this, *result), 0.0f); // Re-post self
		return false; // Not done yet
	}

	try
	{
		codegenExpr((ExprAST*)instance, getBlockExprASTParent(instance));
	}
	catch (ReturnException err)
	{
		*result = err.result;
		return true;
	}
	catch (AwaitException)
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
	void definePackage(BlockExprAST* pkgScope);
public:
	PawsFramePackage() : MincPackage("paws.frame"), eventPool(nullptr) {}
	~PawsFramePackage()
	{
		if (eventPool != nullptr)
			delete eventPool;
	}
} PAWS_FRAME;

void PawsFramePackage::definePackage(BlockExprAST* pkgScope)
{
	eventPool = new EventPool(1);

	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");

	// >>> Type hierarchy
	//
	// Frame instance:		frame -> PawsFrameInstance ---------------\
	//																  |
	// Event instance:		PawsEvent<msgType> -> PawsEventInstance --|--> PawsAwaitableInstance
	//																  |
	// Awaitable instance:	PawsAwaitable<returnType> ----------------/
	//
	// Frame class:			PawsFrame<frame> -> PawsFrame -> PawsAwaitable -> PawsMetaType
	//

	registerType<PawsAwaitable>(pkgScope, "PawsAwaitable");
	registerType<PawsEvent>(pkgScope, "PawsEvent");
	registerType<PawsFrame>(pkgScope, "PawsFrame");
	defineOpaqueInheritanceCast(pkgScope, PawsFrame::TYPE, PawsAwaitable::TYPE);
	defineOpaqueInheritanceCast(pkgScope, PawsAwaitable::TYPE, PawsMetaType::TYPE);

	registerType<PawsAwaitableInstance>(pkgScope, "PawsAwaitableInstance");
	registerType<PawsEventInstance>(pkgScope, "PawsEventInstance");
	defineOpaqueInheritanceCast(pkgScope, PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE);
	registerType<PawsFrameInstance>(pkgScope, "PawsFrameInstance");
	defineOpaqueInheritanceCast(pkgScope, PawsFrameInstance::TYPE, PawsAwaitableInstance::TYPE);

	// Define sleep function
	defineConstantFunction(pkgScope, "sleep", Awaitable::get(PawsVoid::TYPE), { PawsDouble::TYPE }, { "duration" },
		[](BlockExprAST* callerScope, const std::vector<ExprAST*>& args, void* funcArgs) -> Variable
		{
			double duration = ((PawsDouble*)codegenExpr(args[0], callerScope).value)->get();
			SingleshotAwaitableInstance* sleepInstance = new SingleshotAwaitableInstance();
			((EventPool*)funcArgs)->post(std::bind(&SingleshotAwaitableInstance::wakeup, sleepInstance, Variable(PawsVoid::TYPE, nullptr)), duration);
			return Variable(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(sleepInstance));
		}, eventPool
	);

	// Define event definition
	defineExpr3(pkgScope, "event<$E<PawsMetaType>>()",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			return Variable(Event::get(PawsString::TYPE), new PawsEventInstance(new EventInstance(PawsString::TYPE, (EventPool*)exprArgs)));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			PawsType* returnType = (PawsType*)getType(params[0], parentBlock);
			//TODO: getType() returns PawsMetaType, not the the passed type (e.g. PawsString)
			//TODO	How can returnType be retrieved in a constant context?
			return Event::get(PawsString::TYPE);
		}, eventPool
	);

	// Define frame definition
	defineStmt2(pkgScope, "$E<PawsMetaType> frame $I($E<PawsMetaType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			const char* frameName = getIdExprASTName((IdExprAST*)params[1]);
			const std::vector<ExprAST*>& argTypeExprs = getListExprASTExprs((ListExprAST*)params[2]);
			const std::vector<ExprAST*>& argNameExprs = getListExprASTExprs((ListExprAST*)params[3]);
			BlockExprAST* block = (BlockExprAST*)params[4];

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->get());
			frame->argNames.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				frame->argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
			frame->body = block;

			// Define types of arguments in frame body
			for (size_t i = 0; i < frame->argTypes.size(); ++i)
				defineSymbol(block, frame->argNames[i].c_str(), frame->argTypes[i], nullptr);

			// Define frame variable assignment
			defineStmt2(block, "public $I = $E<PawsBase>",
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					ExprAST* exprAST = params[1];
					if (ExprASTIsCast(exprAST))
						exprAST = getCastExprASTSource((CastExprAST*)exprAST);

					ExprAST* varAST = params[0];
					if (ExprASTIsCast(varAST))
						varAST = getCastExprASTSource((CastExprAST*)varAST);

					Frame* frame = (Frame*)stmtArgs;
					frame->variables[getIdExprASTName((IdExprAST*)varAST)] = Frame::Variable{(PawsType*)getType(exprAST, parentBlock), exprAST};
				}, frame
			);

			defineAntiStmt2(block,
				[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
					throw ReturnException(Variable(PawsVoid::TYPE, nullptr));
				}
			);

			try
			{
				codegenExpr((ExprAST*)block, parentBlock);
			}
			catch(ReturnException) {}

			defineAntiStmt2(block, nullptr);

			// Name frame block
			std::string frameFullName(frameName);
			frameFullName += '(';
			if (frame->argTypes.size())
			{
				frameFullName += getTypeName(frame->argTypes[0]);
				for (size_t i = 1; i != frame->argTypes.size(); ++i)
					frameFullName += getTypeName(frame->argTypes[i]) + ", ";
			}
			frameFullName += ')';
			setBlockExprASTName(block, frameFullName);

			// Set frame parent to frame definition scope
			setBlockExprASTParent(block, parentBlock);

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType);

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
			std::vector<ExprAST*>& argExprs = getListExprASTExprs((ListExprAST*)params[1]);

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
			EventPool* const eventPool = (EventPool*)exprArgs;
			FrameInstance* instance = new FrameInstance(frame, parentBlock, argExprs, eventPool);
			eventPool->post(std::bind(&FrameInstance::wakeup, instance, Variable(PawsVoid::TYPE, nullptr)), 0.0f);

			return Variable(frame, new PawsFrameInstance(instance));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			Frame* frame = (Frame*)((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
			return frame;
		}, eventPool
	);

	// Define frame member getter
	defineExpr3(pkgScope, "$E<PawsFrameInstance>.$I",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Variable& var = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			Frame* strct = (Frame*)var.type;
			FrameInstance* instance = ((PawsFrameInstance*)var.value)->get();
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = strct->variables.find(memberName);
			if (variable == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + getTypeName(strct) + "'").c_str(), params[1]);

			return Variable(variable->second.type, instance->variables[memberName]);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			Frame* frame = (Frame*)(getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock));
			std::string memberName = getIdExprASTName((IdExprAST*)params[1]);

			auto variable = frame->variables.find(memberName);
			return variable == frame->variables.end() ? nullptr : variable->second.type;
		}
	);

	// Define boolean operators on awaitables
	defineExpr2(pkgScope, "$E<PawsAwaitableInstance> || $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* a = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			AwaitableInstance* b = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[1]), parentBlock).value)->get();
			return Variable(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AnyAwaitableInstance(a, b)));
		}, Awaitable::get(PawsVoid::TYPE)
	);
	defineExpr2(pkgScope, "$E<PawsAwaitableInstance> && $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* a = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			AwaitableInstance* b = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[1]), parentBlock).value)->get();
			return Variable(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AllAwaitableInstance(a, b)));
		}, Awaitable::get(PawsVoid::TYPE)
	);

	// Define top-level await statement
	defineExpr3(pkgScope, "await $E<PawsAwaitableInstance>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			EventPool* eventPool = (EventPool*)exprArgs;
			Variable result = Variable(PawsVoid::TYPE, nullptr);

			if (blocker->awaitResult(new TopLevelInstance(eventPool), &result))
				return result;
			eventPool->run();
			blocker->getResult(&result);
			return result;
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			return event->returnType;
		}, eventPool
	);

	// Define event call
	defineExpr3(pkgScope, "$E<PawsEventInstance>($E)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Variable eventVar = codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			EventInstance* const event = ((PawsEventInstance*)eventVar.value)->get();
			ExprAST* argExpr = params[1];

			BaseType* const argType = getType(argExpr, parentBlock);
			PawsType* const msgType = ((Event*)eventVar.type)->msgType;
			if (msgType != argType)
			{
				ExprAST* castExpr = lookupCast(parentBlock, argExpr, msgType);
				if (castExpr == nullptr)
				{
					std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
					raiseCompileError(
						("invalid event type: " + ExprASTToString(argExpr) + "<" + getTypeName(argType) + ">, expected: <" + getTypeName(msgType) + ">\n" + candidateReport).c_str(),
						argExpr
					);
				}
				argExpr = castExpr;
			}

			// Invoke event
			SingleshotAwaitableInstance* invokeInstance = new SingleshotAwaitableInstance();
			((EventPool*)exprArgs)->post(std::bind(&EventInstance::invoke, event, invokeInstance, codegenExpr(argExpr, parentBlock).value), 0.0f);

			return Variable(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(invokeInstance));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			return Awaitable::get(PawsVoid::TYPE);
		}, eventPool
	);
}