#include "paws_frame_eventloop.h"
#include <cassert>
#include <vector>
#include <list>
#include <queue>
#include <limits> // For NaN
#include <cmath> // For isnan()
#include "minc_api.h"
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"

MincBlockExpr* importScope = nullptr; //TODO: This will not work if this package is imported more than once

static struct {} FRAME_INSTANCE_ID;

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

struct Frame : public Awaitable
{
	struct MincSymbol
	{
		PawsType* type;
		MincExpr* initExpr;
	};

	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	std::map<std::string, MincSymbol> variables;
	MincBlockExpr* body;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
		: Awaitable(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};

struct SingleshotAwaitableInstance;
struct AwaitableInstance
{
	virtual bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol* result) = 0;
	virtual bool getResult(MincSymbol* result) = 0;
};
typedef PawsValue<AwaitableInstance*> PawsAwaitableInstance;

struct SingleshotAwaitableInstance : public AwaitableInstance
{
private:
	std::list<SingleshotAwaitableInstance*>* blocked;
	std::mutex mutex;

public:
	MincSymbol result;
	double delay; //TODO: Replace delays with timestamps

	SingleshotAwaitableInstance(double delay = 0.0) : blocked(new std::list<SingleshotAwaitableInstance*>()), result(nullptr, nullptr), delay(delay)
	{
		if (std::isnan(delay))
			throw std::invalid_argument("Trying to create awaitable with NaN delay");
	}
	~SingleshotAwaitableInstance()
	{
		delete blocked;
	}

	bool isDone() { return std::isnan(delay); }
	void wakeup(MincSymbol result)
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
	bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol* result)
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
	bool getResult(MincSymbol* result)
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
	virtual bool resume(MincSymbol* result) { return true; }
};

struct TopLevelInstance : public SingleshotAwaitableInstance
{
private:
	EventPool* const eventPool;

public:
	TopLevelInstance(EventPool* eventPool) : eventPool(eventPool) {}

protected:
	bool resume(MincSymbol* result)
	{
		eventPool->close();
		return true;
	}
};

struct FrameInstance : public SingleshotAwaitableInstance
{
private:
	const Frame* frame;
	MincBlockExpr* instance;
	EventPool* const eventPool;

public:
	std::map<std::string, MincObject*> variables;

	FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool);
	~FrameInstance() { removeBlockExpr(instance); }

protected:
	bool resume(MincSymbol* result);
};
typedef PawsValue<FrameInstance*> PawsFrameInstance;

struct AnyAwaitableInstance : public SingleshotAwaitableInstance
{
protected:
	AwaitableInstance *const a, *const b;

public:
	AnyAwaitableInstance(AwaitableInstance* a, AwaitableInstance* b) : a(a), b(b)
	{
		MincSymbol result;
		if (a->awaitResult(this, &result) || b->awaitResult(this, &result))
			wakeup(result);
	}

protected:
	bool resume(MincSymbol* result)
	{
		*result = MincSymbol(PawsVoid::TYPE, nullptr);
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
		MincSymbol result;
		bool aDone = a->awaitResult(this, &result), bDone = b->awaitResult(this, &result);
		if (aDone && bDone)
			wakeup(result);
	}

protected:
	bool resume(MincSymbol* result)
	{
		return a->awaitResult(this, result) && b->awaitResult(this, result);
	}
};

struct EventInstance : public AwaitableInstance
{
private:
	struct Message
	{
		MincObject* value;
		SingleshotAwaitableInstance* invokeInstance;
	};
	PawsType* const type;
	std::mutex mutex; // Mutex protecting blockedAwaitable, blockedAwaitableResult and msgQueue
	SingleshotAwaitableInstance* blockedAwaitable;
	MincSymbol blockedAwaitableResult;
	std::queue<Message> msgQueue;
	EventPool* const eventPool;

public:
	EventInstance(PawsType* type, EventPool* eventPool) : type(type), blockedAwaitable(nullptr), eventPool(eventPool) {}
	void invoke(SingleshotAwaitableInstance* invokeInstance, MincObject* value)
	{
		mutex.lock();
		msgQueue.push(Message{value, invokeInstance}); // Queue message
		if (blockedAwaitable) // If this event is being awaited, ...
		{
			mutex.unlock();
			blockedAwaitable->wakeup(blockedAwaitableResult = MincSymbol(type, value)); // Wake up waiting awaitable
		}
		else
			mutex.unlock();
	}
	bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol* result)
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
			*result = MincSymbol(type, msg.value); // Return first queued message
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
			return true;
		}
	}
	bool getResult(MincSymbol* result)
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
			*result = MincSymbol(type, msg.value); // Return first queued message
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
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
		t->name = "Awaitable<" + returnType->name + '>';
		defineSymbol(importScope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(importScope, t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(importScope, t, PawsAwaitableInstance::TYPE);
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
		t->name = "Event<" + msgType->name + '>';
		defineSymbol(importScope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(importScope, t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(importScope, t, PawsEventInstance::TYPE);
defineOpaqueInheritanceCast(importScope, PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE); //TODO: This shouldn't be necessary
	}
	return const_cast<Event*>(&*iter); //TODO: Find a way to avoid const_cast
}

FrameInstance::FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool)
	: frame(frame), instance(cloneBlockExpr(frame->body)), eventPool(eventPool)
{
	setBlockExprParent(instance, frame->body);
	setBlockExprUser(instance, this);
	setBlockExprUserType(instance, &FRAME_INSTANCE_ID);

	// Define arguments in frame instance
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(instance, frame->argNames[i].c_str(), frame->argTypes[i], ((PawsBase*)codegenExpr(argExprs[i], callerScope).value)->copy());

	// Initialize and define frame variables in frame instance
	for (const std::pair<const std::string, Frame::MincSymbol>& pair: frame->variables)
	{
		MincObject* const value = codegenExpr(pair.second.initExpr, frame->body).value;
		defineSymbol(instance, pair.first.c_str(), pair.second.type, value);
		variables[pair.first] = value;
	}
}

bool FrameInstance::resume(MincSymbol* result)
{
	// Avoid executing instance while it's being executed by another thread
	// This happens when a frame is resumed by a new thread while the old thread is still busy unrolling the AwaitException
	if (isBlockExprBusy(instance))
	{
		eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, this, *result), 0.0f); // Re-post self
		return false; // Not done yet
	}

	try
	{
		codegenExpr((MincExpr*)instance, getBlockExprParent(instance));
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
		raiseCompileError("missing return statement in frame body", (MincExpr*)instance);
	*result = MincSymbol(PawsVoid::TYPE, nullptr);
	return true;
}

class PawsFramePackage : public MincPackage
{
private:
	EventPool* eventPool;
	void definePackage(MincBlockExpr* pkgScope);
public:
	PawsFramePackage() : MincPackage("paws.frame"), eventPool(nullptr) {}
	~PawsFramePackage()
	{
		if (eventPool != nullptr)
			delete eventPool;
	}
} PAWS_FRAME;

void PawsFramePackage::definePackage(MincBlockExpr* pkgScope)
{
	eventPool = new EventPool(1);
	importScope = pkgScope;

	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");

	// >>> Type hierarchy
	//
	// Frame instance:		frame -> PawsFrameInstance ---------------|
	//																  |
	// Event instance:		PawsEvent<msgType> -> PawsEventInstance --|--> PawsAwaitableInstance
	//																  |
	// Awaitable instance:	PawsAwaitable<returnType> ----------------|
	//
	// Frame class:			PawsFrame<frame> -> PawsFrame -> PawsAwaitable -> PawsType
	//

	registerType<Awaitable>(pkgScope, "PawsAwaitable");
	registerType<Event>(pkgScope, "PawsEvent");
	registerType<Frame>(pkgScope, "PawsFrame");
	defineOpaqueInheritanceCast(pkgScope, Frame::TYPE, Awaitable::TYPE);
	defineOpaqueInheritanceCast(pkgScope, Awaitable::TYPE, PawsType::TYPE);

	registerType<PawsAwaitableInstance>(pkgScope, "PawsAwaitableInstance");
	registerType<PawsEventInstance>(pkgScope, "PawsEventInstance");
	defineOpaqueInheritanceCast(pkgScope, PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE);
	registerType<PawsFrameInstance>(pkgScope, "PawsFrameInstance");
	defineOpaqueInheritanceCast(pkgScope, PawsFrameInstance::TYPE, PawsAwaitableInstance::TYPE);

	// Define sleep function
	defineConstantFunction(pkgScope, "sleep", Awaitable::get(PawsVoid::TYPE), { PawsDouble::TYPE }, { "duration" },
		[](MincBlockExpr* callerScope, const std::vector<MincExpr*>& args, void* funcArgs) -> MincSymbol
		{
			double duration = ((PawsDouble*)codegenExpr(args[0], callerScope).value)->get();
			SingleshotAwaitableInstance* sleepInstance = new SingleshotAwaitableInstance();
			((EventPool*)funcArgs)->post(std::bind(&SingleshotAwaitableInstance::wakeup, sleepInstance, MincSymbol(PawsVoid::TYPE, nullptr)), duration);
			return MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(sleepInstance));
		}, eventPool
	);

	// Define event definition
	defineExpr3(pkgScope, "event<$E<PawsType>>()",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
			return MincSymbol(Event::get(returnType), new PawsEventInstance(new EventInstance(returnType, (EventPool*)exprArgs)));
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* returnType = (PawsType*)codegenExpr(const_cast<MincExpr*>(params[0]), const_cast<MincBlockExpr*>(parentBlock)).value; //TODO: Remove const_cast
			//TODO	How can returnType be retrieved in a constant context?
			return Event::get(returnType);
		}, eventPool
	);

	// Define frame definition
	defineStmt2(pkgScope, "$E<PawsType> frame $I($E<PawsType> $I, ...) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			PawsType* returnType = (PawsType*)codegenExpr(params[0], parentBlock).value;
			const char* frameName = getIdExprName((MincIdExpr*)params[1]);
			const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
			const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back((PawsType*)codegenExpr(argTypeExpr, parentBlock).value);
			frame->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				frame->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
			frame->body = block;

			// Define types of arguments in frame body
			for (size_t i = 0; i < frame->argTypes.size(); ++i)
				defineSymbol(block, frame->argNames[i].c_str(), frame->argTypes[i], nullptr);

			// Define frame variable assignment
			defineStmt2(block, "public $I = $E<PawsBase>",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					MincExpr* exprAST = params[1];
					if (ExprIsCast(exprAST))
						exprAST = getCastExprSource((MincCastExpr*)exprAST);

					MincExpr* varAST = params[0];
					if (ExprIsCast(varAST))
						varAST = getCastExprSource((MincCastExpr*)varAST);

					Frame* frame = (Frame*)stmtArgs;
					frame->variables[getIdExprName((MincIdExpr*)varAST)] = Frame::MincSymbol{(PawsType*)getType(exprAST, parentBlock), exprAST};
				}, frame
			);

			defineDefaultStmt2(block,
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
					throw ReturnException(MincSymbol(PawsVoid::TYPE, nullptr));
				}
			);

			try
			{
				codegenExpr((MincExpr*)block, parentBlock);
			}
			catch (ReturnException) {}

			defineDefaultStmt2(block, nullptr);
			forgetExpr((MincExpr*)getCurrentBlockExprStmt(block));

			// Define await statement in frame instance scope
			defineExpr3(block, "await $E<PawsAwaitableInstance>",
				[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
					AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock).value)->get();
					MincSymbol blockerResult;
					MincBlockExpr* instance = parentBlock;
					while (getBlockExprUserType(instance) != &FRAME_INSTANCE_ID)
						instance = getBlockExprParent(instance);
					assert(instance);
					if (blocker->awaitResult((FrameInstance*)getBlockExprUser(instance), &blockerResult))
						return blockerResult;
					else
						throw AwaitException();
				}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
					assert(ExprIsCast(params[0]));
					const Awaitable* event = (Awaitable*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
					return event->returnType;
				}
			);

			// Name frame block
			std::string frameFullName(frameName);
			frameFullName += '(';
			if (frame->argTypes.size())
			{
				frameFullName += frame->argTypes[0]->name;
				for (size_t i = 1; i != frame->argTypes.size(); ++i)
					frameFullName += frame->argTypes[i]->name + ", ";
			}
			frameFullName += ')';
			setBlockExprName(block, frameFullName.c_str());

			// Set frame parent to frame definition scope
			setBlockExprParent(block, parentBlock);

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType);

			frame->name = frameName;
			defineSymbol(parentBlock, frameName, PawsTpltType::get(parentBlock, Frame::TYPE, frame), frame);
			defineOpaqueInheritanceCast(parentBlock, frame, PawsFrameInstance::TYPE);
			defineOpaqueInheritanceCast(parentBlock, PawsTpltType::get(parentBlock, Frame::TYPE, frame), PawsType::TYPE);
		}
	);

	// Define frame call
	defineExpr3(pkgScope, "$E<PawsFrame>($E, ...)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			Frame* frame = (Frame*)codegenExpr(params[0], parentBlock).value;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of frame arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = frame->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(parentBlock, getLocation(argExpr), "invalid frame argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = castExpr;
				}
			}

			// Call frame
			EventPool* const eventPool = (EventPool*)exprArgs;
			FrameInstance* instance = new FrameInstance(frame, parentBlock, argExprs, eventPool);
			eventPool->post(std::bind(&FrameInstance::wakeup, instance, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f);

			return MincSymbol(frame, new PawsFrameInstance(instance));
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			Frame* frame = (Frame*)((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			return frame;
		}, eventPool
	);

	// Define frame member getter
	defineExpr3(pkgScope, "$E<PawsFrameInstance>.$I",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			const MincSymbol& var = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			Frame* strct = (Frame*)var.type;
			FrameInstance* instance = ((PawsFrameInstance*)var.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = strct->variables.find(memberName);
			if (variable == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);

			return MincSymbol(variable->second.type, instance->variables[memberName]);
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			Frame* frame = (Frame*)(getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = frame->variables.find(memberName);
			return variable == frame->variables.end() ? nullptr : variable->second.type;
		}
	);

	// Define boolean operators on awaitables
	defineExpr2(pkgScope, "$E<PawsAwaitableInstance> || $E<PawsAwaitableInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			AwaitableInstance* a = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock).value)->get();
			AwaitableInstance* b = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[1]), parentBlock).value)->get();
			return MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AnyAwaitableInstance(a, b)));
		}, Awaitable::get(PawsVoid::TYPE)
	);
	defineExpr2(pkgScope, "$E<PawsAwaitableInstance> && $E<PawsAwaitableInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			AwaitableInstance* a = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock).value)->get();
			AwaitableInstance* b = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[1]), parentBlock).value)->get();
			return MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AllAwaitableInstance(a, b)));
		}, Awaitable::get(PawsVoid::TYPE)
	);

	// Define top-level await statement
	defineExpr3(pkgScope, "await $E<PawsAwaitableInstance>",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock).value)->get();
			EventPool* eventPool = (EventPool*)exprArgs;
			MincSymbol result = MincSymbol(PawsVoid::TYPE, nullptr);

			if (blocker->awaitResult(new TopLevelInstance(eventPool), &result))
				return result;
			eventPool->run();
			blocker->getResult(&result);
			return result;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			return event->returnType;
		}, eventPool
	);

	// Define event call
	defineExpr3(pkgScope, "$E<PawsEventInstance>($E)",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* exprArgs) -> MincSymbol {
			MincSymbol eventVar = codegenExpr(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			EventInstance* const event = ((PawsEventInstance*)eventVar.value)->get();
			MincExpr* argExpr = params[1];

			MincObject* const argType = getType(argExpr, parentBlock);
			PawsType* const msgType = ((Event*)eventVar.type)->msgType;
			if (msgType != argType)
			{
				MincExpr* castExpr = lookupCast(parentBlock, argExpr, msgType);
				if (castExpr == nullptr)
					throw CompileError(parentBlock, getLocation(argExpr), "invalid event type: %E<%t>, expected: <%t>", argExpr, argType, msgType);
				argExpr = castExpr;
			}

			// Invoke event
			SingleshotAwaitableInstance* invokeInstance = new SingleshotAwaitableInstance();
			((EventPool*)exprArgs)->post(std::bind(&EventInstance::invoke, event, invokeInstance, codegenExpr(argExpr, parentBlock).value), 0.0f);

			return MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(invokeInstance));
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return Awaitable::get(PawsVoid::TYPE);
		}, eventPool
	);
}