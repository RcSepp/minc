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

MincBlockExpr* pawsFrameScope = nullptr; //TODO: This will not work if this package is imported more than once

static struct {} FRAME_ID, FRAME_INSTANCE_ID;

struct __attribute__((visibility("hidden"))) Awaitable : public PawsType
{
	typedef PawsType CType;
private:
	static std::mutex mutex;
	static std::set<Awaitable> awaitableTypes;

protected:
	Awaitable(PawsType* returnType)
		: returnType(returnType) {}

public:
	static PawsMetaType* const TYPE;
	PawsType* returnType;
	static Awaitable* get(PawsType* returnType);
	Awaitable() = default;
	MincObject* copy(MincObject* value);
	std::string toString(MincObject* value) const;
};
inline PawsMetaType* const Awaitable::TYPE = new PawsMetaType(sizeof(Awaitable));
std::mutex Awaitable::mutex;
std::set<Awaitable> Awaitable::awaitableTypes;
bool operator<(const Awaitable& lhs, const Awaitable& rhs)
{
	return lhs.returnType < rhs.returnType;
}

struct __attribute__((visibility("hidden"))) Event : public PawsType
{
	typedef PawsType CType;
private:
	static std::mutex mutex;
	static std::set<Event> eventTypes;

protected:
	Event(PawsType* msgType)
		: PawsType(sizeof(Event)), msgType(msgType) {}

public:
	static PawsMetaType* const TYPE;
	PawsType* msgType;
	static Event* get(PawsType* msgType);
	Event() = default;
	MincObject* copy(MincObject* value);
	std::string toString(MincObject* value) const;
};
inline PawsMetaType* const Event::TYPE = new PawsMetaType(sizeof(Event));
std::mutex Event::mutex;
std::set<Event> Event::eventTypes;
bool operator<(const Event& lhs, const Event& rhs)
{
	return lhs.msgType < rhs.msgType;
}

struct __attribute__((visibility("hidden"))) Frame : public Awaitable
{
	struct MincSymbol
	{
		PawsType* type;
		MincExpr* initExpr;
	};

	static PawsMetaType* const TYPE;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	std::map<std::string, MincSymbol> variables;
	MincBlockExpr* body;
	size_t beginStmtIndex;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
		: Awaitable(returnType), argTypes(argTypes), argNames(argNames), body(body), beginStmtIndex(0) {}
};
inline PawsMetaType* const Frame::TYPE = new PawsMetaType(sizeof(Frame));

struct SingleshotAwaitableInstance;
struct AwaitableInstance
{
	virtual bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol& result) = 0;
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
		bool done;
		if (resume(result, done))
			throw result;
		if (done)
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
	bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol& result)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (isDone())
		{
			result = this->result;
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
	virtual bool resume(MincSymbol& result, bool& done) { done = true; return false; }
};

struct TopLevelInstance : public SingleshotAwaitableInstance
{
private:
	EventPool* const eventPool;

public:
	TopLevelInstance(EventPool* eventPool) : eventPool(eventPool) {}

protected:
	bool resume(MincSymbol& result, bool& done)
	{
		eventPool->close();
		done = true;
		return false;
	}
};

struct __attribute__((visibility("hidden"))) FrameInstance : public SingleshotAwaitableInstance
{
private:
	const Frame* frame;
	EventPool* const eventPool;

public:
	MincBlockExpr* instance;
	std::map<std::string, MincObject*> variables;
	bool suspended;

	FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool);
	~FrameInstance() { removeBlockExpr(instance); }

protected:
	bool resume(MincSymbol& result, bool& done);
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
		if (a->awaitResult(this, result) || b->awaitResult(this, result))
			wakeup(result);
	}

protected:
	bool resume(MincSymbol& result, bool& done)
	{
		result = MincSymbol(PawsVoid::TYPE, nullptr);
		done = true;
		return false;
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
		bool aDone = a->awaitResult(this, result), bDone = b->awaitResult(this, result);
		if (aDone && bDone)
			wakeup(result);
	}

protected:
	bool resume(MincSymbol& result, bool& done)
	{
		done = a->awaitResult(this, result) && b->awaitResult(this, result);
		return false;
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
			SingleshotAwaitableInstance* _blockedAwaitable = blockedAwaitable;
			blockedAwaitable = nullptr;
			mutex.unlock();
			_blockedAwaitable->wakeup(blockedAwaitableResult = MincSymbol(type, value)); // Wake up waiting awaitable
		}
		else
			mutex.unlock();
	}
	bool awaitResult(SingleshotAwaitableInstance* awaitable, MincSymbol& result)
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
			result = MincSymbol(type, msg.value); // Return first queued message
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

Awaitable* Awaitable::get(PawsType* returnType)
{
	std::unique_lock<std::mutex> lock(mutex);
	std::set<Awaitable>::iterator iter = awaitableTypes.find(Awaitable(returnType));
	if (iter == awaitableTypes.end())
	{
		iter = awaitableTypes.insert(Awaitable(returnType)).first;
		Awaitable* t = const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
		t->name = "Awaitable<" + returnType->name + '>';
		defineSymbol(pawsFrameScope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(pawsFrameScope, t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(pawsFrameScope, t, PawsAwaitableInstance::TYPE);
	}
	return const_cast<Awaitable*>(&*iter); //TODO: Find a way to avoid const_cast
}

MincObject* Awaitable::copy(MincObject* value)
{
	return value; //TODO: This passes awaitables by reference. Think of how to handle struct assignment (by value, by reference, via reference counting, ...)
}

std::string Awaitable::toString(MincObject* value) const
{
	return PawsType::toString(value); //TODO: This uses default toString() behaviour. Consider a more verbose format.
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
		defineSymbol(pawsFrameScope, t->name.c_str(), PawsType::TYPE, t);
		defineOpaqueInheritanceCast(pawsFrameScope, t, PawsBase::TYPE);
		defineOpaqueInheritanceCast(pawsFrameScope, t, PawsEventInstance::TYPE);
defineOpaqueInheritanceCast(pawsFrameScope, PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE); //TODO: This shouldn't be necessary
	}
	return const_cast<Event*>(&*iter); //TODO: Find a way to avoid const_cast
}

MincObject* Event::copy(MincObject* value)
{
	return value; //TODO: This passes events by reference. Think of how to handle struct assignment (by value, by reference, via reference counting, ...)
}

std::string Event::toString(MincObject* value) const
{
	return PawsType::toString(value); //TODO: This uses default toString() behaviour. Consider a more verbose format.
}

FrameInstance::FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool)
	: frame(frame), eventPool(eventPool), instance(cloneBlockExpr(frame->body)), suspended(false)
{
	setBlockExprParent(instance, getBlockExprParent(frame->body));
	setBlockExprUser(instance, this);
	setBlockExprUserType(instance, &FRAME_INSTANCE_ID);
	setCurrentBlockExprStmtIndex(instance, frame->beginStmtIndex);
}

bool FrameInstance::resume(MincSymbol& result, bool& done)
{
	// Avoid executing instance while it's being executed by another thread
	// This happens when a frame is resumed by a new thread while the old thread is still busy unrolling the stack
	if (isBlockExprBusy(instance))
	{
		eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, this, result), 0.0f); // Re-post self
		done = false;
		return false;
	}

	MincRuntime runtime(getBlockExprParent(instance), suspended);
	if (runExpr((MincExpr*)instance, runtime))
	{
		if (runtime.result.type == &PAWS_RETURN_TYPE)
		{
			result.value = runtime.result.value;
			result.type = frame->returnType;
			done = true;
			return false;
		}
		else if (runtime.result.type == &PAWS_AWAIT_TYPE)
		{
			suspended = true;
			done = false;
			return false;
		}
		else
			return true;
	}

	if (frame->returnType != getVoid().type && frame->returnType != PawsVoid::TYPE)
		raiseCompileError("missing return statement in frame body", (MincExpr*)instance);
	result = MincSymbol(PawsVoid::TYPE, nullptr);
	done = true;
	return false;
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
	pawsFrameScope = pkgScope;

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
		[](MincRuntime& runtime, const std::vector<MincExpr*>& args, void* funcArgs) -> bool
		{
			if(runExpr(args[0], runtime))
				return true;
			double duration = ((PawsDouble*)runtime.result.value)->get();
			SingleshotAwaitableInstance* sleepInstance = new SingleshotAwaitableInstance();
			((EventPool*)funcArgs)->post(std::bind(&SingleshotAwaitableInstance::wakeup, sleepInstance, MincSymbol(PawsVoid::TYPE, nullptr)), duration);
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(sleepInstance));
			return false;
		}, eventPool
	);

	// Define event definition
	defineExpr3(pkgScope, "event<$I<PawsType>>()",
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			PawsType* returnType = (PawsType*)lookupSymbol(runtime.parentBlock, getIdExprName((MincIdExpr*)params[0]))->value;
			runtime.result = MincSymbol(Event::get(returnType), new PawsEventInstance(new EventInstance(returnType, (EventPool*)exprArgs)));
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			PawsType* returnType = (PawsType*)lookupSymbol(parentBlock, getIdExprName((MincIdExpr*)params[0]))->value;
			return Event::get(returnType);
		}, eventPool
	);
	defineExpr9(pkgScope, "event<$E<PawsType>>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0], buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			runtime.result = MincSymbol(PawsType::TYPE, Event::get((PawsType*)runtime.result.value));
			return false;
		},
		PawsType::TYPE
	);

	// Define frame definition
	class FrameDefinitionKernel : public MincKernel
	{
		const MincSymbolId varId;
		Frame* const frame;
		PawsType* const structType;
	public:
		FrameDefinitionKernel() : varId(MincSymbolId::NONE), frame(nullptr), structType(nullptr) {}
		FrameDefinitionKernel(MincSymbolId varId, PawsType* structType, Frame* frame) : varId(varId), frame(frame), structType(structType) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)buildExpr(params[0], buildtime).value;
			const char* frameName = getIdExprName((MincIdExpr*)params[1]);
			const std::vector<MincExpr*>& argTypeExprs = getListExprExprs((MincListExpr*)params[2]);
			const std::vector<MincExpr*>& argNameExprs = getListExprExprs((MincListExpr*)params[3]);
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			// Set frame parent to frame definition scope
			setBlockExprParent(block, buildtime.parentBlock);

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back((PawsType*)buildExpr(argTypeExpr, buildtime).value);
			frame->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				frame->argNames.push_back(getIdExprName((MincIdExpr*)argNameExpr));
			frame->body = block;

			setBlockExprUser(block, frame);
			setBlockExprUserType(block, &FRAME_ID);

			// Define types of arguments in frame block
			for (size_t i = 0; i < frame->argTypes.size(); ++i)
				defineSymbol(block, frame->argNames[i].c_str(), frame->argTypes[i], nullptr);

			// Define frame variable assignment
			defineStmt5(block, "public $I = $E<PawsBase>",
				[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
					MincExpr* exprAST = params[1];
					if (ExprIsCast(exprAST))
						exprAST = getCastExprSource((MincCastExpr*)exprAST);

					MincExpr* varAST = params[0];
					if (ExprIsCast(varAST))
						varAST = getCastExprSource((MincCastExpr*)varAST);

					MincBlockExpr* block = buildtime.parentBlock;
					while (getBlockExprUserType(block) != &FRAME_ID)
						block = getBlockExprParent(block);
					assert(block);
					Frame* frame = (Frame*)getBlockExprUser(block);
					PawsType* type = (PawsType*)::getType(exprAST, buildtime.parentBlock);
					frame->variables[getIdExprName((MincIdExpr*)varAST)] = Frame::MincSymbol{type, exprAST};
					frame->size += type->size;

					// Build frame variable expression and define frame variable inside frame body
					buildExpr(exprAST, buildtime);
					defineSymbol(frame->body, getIdExprName((MincIdExpr*)varAST), type, nullptr);
					//TODO: Store symbolId
				}
			);

			defineDefaultStmt6(block,
				[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
					MincBlockExpr* block = buildtime.parentBlock;
					while (getBlockExprUserType(block) != &FRAME_ID)
						block = getBlockExprParent(block);
					assert(block);

					// Prohibit declarations of frame variables after other statements
					defineStmt5(block, "public $I = $E<PawsBase>",
						[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
							throw CompileError(buildtime.parentBlock, getLocation(params[0]), "frame variables have to be defined at the beginning of the frame");
						}
					);

					// Unset default statement
					defineDefaultStmt5(block, nullptr);

					// Build current statement
					buildExpr(params[0], buildtime);
				},
				[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
					// Run current statement
					return runExpr(params[0], runtime);
				}
			);

			// Define await expression in frame instance scope
			defineExpr10(block, "await $E<PawsAwaitableInstance>",
				[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
					buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
				},
				[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
					if (runExpr(params[0], runtime))
						return true;
					AwaitableInstance* blocker = ((PawsAwaitableInstance*)runtime.result.value)->get();
					MincBlockExpr* instance = runtime.parentBlock;
					while (getBlockExprUserType(instance) != &FRAME_INSTANCE_ID)
						instance = getBlockExprParent(instance);
					assert(instance);
					if (blocker->awaitResult((FrameInstance*)getBlockExprUser(instance), runtime.result))
						return false;
					else
					{
						runtime.result = MincSymbol(&PAWS_AWAIT_TYPE, nullptr);
						return true;
					}
				}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
					assert(ExprIsCast(params[0]));
					const Awaitable* event = (Awaitable*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
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
					frameFullName += ", " + frame->argTypes[i]->name;
			}
			frameFullName += ')';
			setBlockExprName(block, frameFullName.c_str());

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType, "frame");

			// Build frame
			buildExpr((MincExpr*)block, buildtime);

			frame->name = frameName;
			defineSymbol(buildtime.parentBlock, frameName, PawsTpltType::get(buildtime.parentBlock, Frame::TYPE, frame), frame);
			defineOpaqueInheritanceCast(buildtime.parentBlock, frame, PawsFrameInstance::TYPE);
			defineOpaqueInheritanceCast(buildtime.parentBlock, PawsTpltType::get(buildtime.parentBlock, Frame::TYPE, frame), PawsType::TYPE);
			return new FrameDefinitionKernel(lookupSymbolId(buildtime.parentBlock, frameName), PawsTpltType::get(buildtime.parentBlock, Frame::TYPE, frame), frame);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			// Set frame parent to frame definition scope (the parent may have changed during frame cloning)
			MincBlockExpr* block = (MincBlockExpr*)params[4];
			setBlockExprParent(block, runtime.parentBlock);

			MincSymbol* varFromId = getSymbol(runtime.parentBlock, varId);
			varFromId->value = frame;
			varFromId->type = structType;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	defineStmt4(pkgScope, "$E<PawsType> frame $I($E<PawsType> $I, ...) $B", new FrameDefinitionKernel());

	// Define frame call
	defineExpr10(pkgScope, "$E<PawsFrame>($E, ...)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			Frame* frame = (Frame*)buildExpr(params[0], buildtime).value;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of frame arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = frame->argTypes[i], *gotType = getType(argExpr, buildtime.parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(buildtime.parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(buildtime.parentBlock, getLocation(argExpr), "invalid frame argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = castExpr;
				}
				buildExpr(argExprs[i], buildtime);
			}
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			Frame* frame = (Frame*)runtime.result.value;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Instantiate frame
			EventPool* const eventPool = (EventPool*)exprArgs;
			FrameInstance* instance = new FrameInstance(frame, runtime.parentBlock, argExprs, eventPool);

			// Define arguments in frame instance
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				if (runExpr(argExprs[i], runtime))
					return true;
				defineSymbol(instance->instance, frame->argNames[i].c_str(), frame->argTypes[i], ((PawsType*)runtime.result.type)->copy((PawsBase*)runtime.result.value));
			}

			// Initialize and define frame variables in frame instance
			runtime.parentBlock = frame->body;
			for (const std::pair<const std::string, Frame::MincSymbol>& pair: frame->variables)
			{
				if (runExpr(pair.second.initExpr, runtime))
					return true;
				defineSymbol(instance->instance, pair.first.c_str(), pair.second.type, runtime.result.value);
				instance->variables[pair.first] = runtime.result.value;
			}

			// Call frame
			eventPool->post(std::bind(&FrameInstance::wakeup, instance, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f);

			runtime.result = MincSymbol(frame, new PawsFrameInstance(instance));
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			Frame* frame = (Frame*)((PawsTpltType*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			return frame;
		}, eventPool
	);

	// Define frame member getter
	defineExpr10(pkgScope, "$E<PawsFrameInstance>.$I",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
			Frame* strct = (Frame*)getType(params[0], buildtime.parentBlock);
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = strct->variables.find(memberName);
			if (variable == strct->variables.end())
				raiseCompileError(("no member named '" + memberName + "' in '" + strct->name + "'").c_str(), params[1]);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			Frame* strct = (Frame*)runtime.result.type;
			FrameInstance* instance = ((PawsFrameInstance*)runtime.result.value)->get();
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = strct->variables.find(memberName);
			runtime.result = MincSymbol(variable->second.type, instance->variables[memberName]);
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* expr = params[0];
			if (ExprIsCast(expr))
				expr = getCastExprSource((MincCastExpr*)expr);
			Frame* frame = (Frame*)(getType(expr, parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = frame->variables.find(memberName);
			return variable == frame->variables.end() ? nullptr : variable->second.type;
		}
	);

	// Define boolean operators on awaitables
	defineExpr9(pkgScope, "$E<PawsAwaitableInstance> || $E<PawsAwaitableInstance>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
			buildExpr(params[1] = getCastExprSource((MincCastExpr*)params[1]), buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			AwaitableInstance* a = ((PawsAwaitableInstance*)runtime.result.value)->get();
			if (runExpr(params[1], runtime))
				return true;
			AwaitableInstance* b = ((PawsAwaitableInstance*)runtime.result.value)->get();
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AnyAwaitableInstance(a, b)));
			return false;
		}, Awaitable::get(PawsVoid::TYPE)
	);
	defineExpr9(pkgScope, "$E<PawsAwaitableInstance> && $E<PawsAwaitableInstance>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
			buildExpr(params[1] = getCastExprSource((MincCastExpr*)params[1]), buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			AwaitableInstance* a = ((PawsAwaitableInstance*)runtime.result.value)->get();
			if (runExpr(params[1], runtime))
				return true;
			AwaitableInstance* b = ((PawsAwaitableInstance*)runtime.result.value)->get();
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AllAwaitableInstance(a, b)));
			return false;
		}, Awaitable::get(PawsVoid::TYPE)
	);

	// Define top-level await expression
	defineExpr10(pkgScope, "await $E<PawsAwaitableInstance>",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)runtime.result.value)->get();
			EventPool* eventPool = (EventPool*)exprArgs;
			runtime.result = MincSymbol(PawsVoid::TYPE, nullptr);

			if (!blocker->awaitResult(new TopLevelInstance(eventPool), runtime.result))
			{
				eventPool->run();
				blocker->getResult(&runtime.result);
			}
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			return event->returnType;
		}, eventPool
	);
	// Define await non-awaitable expression
	defineExpr2(pkgScope, "await $E",
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			getType(params[0], runtime.parentBlock); // Raise expression errors if any
			raiseCompileError("expression is not awaitable", params[0]);
			return false; // LCOV_EXCL_LINE
		},
		getErrorType()
	);
	// Define await non-awaitable identifier
	defineExpr2(pkgScope, "await $I",
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			const char* name = getIdExprName((MincIdExpr*)params[0]);
			if (lookupSymbol(runtime.parentBlock, name) == nullptr)
				raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
			else
				raiseCompileError(('`' + std::string(name) + "` is not awaitable").c_str(), params[0]);
			
			return false; // LCOV_EXCL_LINE
		},
		getErrorType()
	);

	// Define event call
	defineExpr10(pkgScope, "$E<PawsEventInstance>($E)",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			buildExpr(params[0] = getCastExprSource((MincCastExpr*)params[0]), buildtime);
			MincExpr* argExpr = params[1];

			MincObject* const argType = getType(argExpr, buildtime.parentBlock);
			PawsType* const msgType = ((Event*)getType(params[0], buildtime.parentBlock))->msgType;
			if (msgType != argType)
			{
				MincExpr* castExpr = lookupCast(buildtime.parentBlock, argExpr, msgType);
				if (castExpr == nullptr)
					throw CompileError(buildtime.parentBlock, getLocation(argExpr), "invalid event type: %E<%t>, expected: <%t>", argExpr, argType, msgType);
				argExpr = castExpr;
			}
			buildExpr(params[1] = argExpr, buildtime);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
			if (runExpr(params[0], runtime))
				return true;
			EventInstance* const event = ((PawsEventInstance*)runtime.result.value)->get();
			MincExpr* argExpr = params[1];

			// Invoke event
			if (runExpr(argExpr, runtime))
				return true;
			SingleshotAwaitableInstance* invokeInstance = new SingleshotAwaitableInstance();
			((EventPool*)exprArgs)->post(std::bind(&EventInstance::invoke, event, invokeInstance, runtime.result.value), 0.0f);

			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(invokeInstance));
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return Awaitable::get(PawsVoid::TYPE);
		}, eventPool
	);
}