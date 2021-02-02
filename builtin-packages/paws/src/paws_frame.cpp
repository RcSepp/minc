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
		: PawsType(sizeof(void*)), returnType(returnType) {}

public:
	static PawsMetaType* const TYPE;
	PawsType* returnType;
	static Awaitable* get(PawsType* returnType);
	Awaitable(): PawsType(sizeof(void*)) {};
	MincObject* copy(MincObject* value);
	void copyTo(MincObject* src, MincObject* dest);
	void copyToNew(MincObject* src, MincObject* dest);
	MincObject* alloc();
	MincObject* allocTo(MincObject* memory);
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
	void copyTo(MincObject* src, MincObject* dest);
	void copyToNew(MincObject* src, MincObject* dest);
	MincObject* alloc();
	MincObject* allocTo(MincObject* memory);
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
		const MincStackSymbol* symbol;
		MincExpr* initExpr;
	};

	static PawsMetaType* const TYPE;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	std::vector<const MincStackSymbol*> args;
	std::map<std::string, MincSymbol> variables;
	MincBlockExpr* body;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, MincBlockExpr* body)
		: Awaitable(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};
inline PawsMetaType* const Frame::TYPE = new PawsMetaType(sizeof(Frame));

struct SingleshotAwaitableInstance;
struct AwaitableInstance
{
	virtual bool awaitResult(MincRuntime* runtime, SingleshotAwaitableInstance* awaitable, MincSymbol& result) = 0;
	virtual bool getResult(MincRuntime* runtime, MincSymbol* result) = 0;
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
	void wakeup(MincRuntime* runtime, MincSymbol result)
	{
		bool done;
		if (resume(runtime, result, done))
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
				awaitable->wakeup(runtime, result);
			delete blocked;
		}
	}
	bool awaitResult(MincRuntime* runtime, SingleshotAwaitableInstance* awaitable, MincSymbol& result)
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
	bool getResult(MincRuntime* runtime, MincSymbol* result)
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
	virtual bool resume(MincRuntime* runtime, MincSymbol& result, bool& done) { done = true; return false; }
};

struct TopLevelInstance : public SingleshotAwaitableInstance
{
private:
	EventPool* const eventPool;

public:
	TopLevelInstance(EventPool* eventPool) : eventPool(eventPool) {}

protected:
	bool resume(MincRuntime* runtime, MincSymbol& result, bool& done)
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
	const MincStackSymbol* resultSymbol;

public:
	std::map<std::string, MincObject*> variables;
	MincStackFrame heapFrame;
	bool suspended;

	FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool, const MincStackSymbol* result);

protected:
	bool resume(MincRuntime* runtime, MincSymbol& result, bool& done);
};
typedef PawsValue<FrameInstance*> PawsFrameInstance;

struct AnyAwaitableInstance : public SingleshotAwaitableInstance
{
protected:
	AwaitableInstance *const a, *const b;

public:
	AnyAwaitableInstance(MincRuntime* runtime, AwaitableInstance* a, AwaitableInstance* b) : a(a), b(b)
	{
		MincSymbol result;
		if (a->awaitResult(runtime, this, result) || b->awaitResult(runtime, this, result))
			wakeup(runtime, result);
	}

protected:
	bool resume(MincRuntime* runtime, MincSymbol& result, bool& done)
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
	AllAwaitableInstance(MincRuntime* runtime, AwaitableInstance* a, AwaitableInstance* b) : a(a), b(b)
	{
		MincSymbol result;
		bool aDone = a->awaitResult(runtime, this, result), bDone = b->awaitResult(runtime, this, result);
		if (aDone && bDone)
			wakeup(runtime, result);
	}

protected:
	bool resume(MincRuntime* runtime, MincSymbol& result, bool& done)
	{
		done = a->awaitResult(runtime, this, result) && b->awaitResult(runtime, this, result);
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
	void invoke(MincRuntime* runtime, SingleshotAwaitableInstance* invokeInstance, MincObject* value)
	{
		mutex.lock();
		msgQueue.push(Message{value, invokeInstance}); // Queue message
		if (blockedAwaitable) // If this event is being awaited, ...
		{
			SingleshotAwaitableInstance* _blockedAwaitable = blockedAwaitable;
			blockedAwaitable = nullptr;
			mutex.unlock();
			_blockedAwaitable->wakeup(runtime, blockedAwaitableResult = MincSymbol(type, value)); // Wake up waiting awaitable
		}
		else
			mutex.unlock();
	}
	bool awaitResult(MincRuntime* runtime, SingleshotAwaitableInstance* awaitable, MincSymbol& result)
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
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, runtime, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
			return true;
		}
	}
	bool getResult(MincRuntime* runtime, MincSymbol* result)
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
			eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, msg.invokeInstance, runtime, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f); // Signal event processed
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

void Awaitable::copyTo(MincObject* src, MincObject* dest)
{
	PawsAwaitableInstance::TYPE->copyTo(src, dest);
}

void Awaitable::copyToNew(MincObject* src, MincObject* dest)
{
	PawsAwaitableInstance::TYPE->copyToNew(src, dest);
}

MincObject* Awaitable::alloc()
{
	return new PawsAwaitableInstance(nullptr);
}
MincObject* Awaitable::allocTo(MincObject* memory)
{
	return new(memory) PawsAwaitableInstance(nullptr);
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

void Event::copyTo(MincObject* src, MincObject* dest)
{
	PawsEventInstance::TYPE->copyTo(src, dest);
}

void Event::copyToNew(MincObject* src, MincObject* dest)
{
	PawsEventInstance::TYPE->copyToNew(src, dest);
}

MincObject* Event::alloc()
{
	return new PawsEventInstance(nullptr);
}
MincObject* Event::allocTo(MincObject* memory)
{
	return new(memory) PawsEventInstance(nullptr);
}

std::string Event::toString(MincObject* value) const
{
	return PawsType::toString(value); //TODO: This uses default toString() behaviour. Consider a more verbose format.
}

FrameInstance::FrameInstance(const Frame* frame, MincBlockExpr* callerScope, const std::vector<MincExpr*>& argExprs, EventPool* eventPool, const MincStackSymbol* result)
	: frame(frame), eventPool(eventPool), resultSymbol(result), suspended(false)
{
	setBlockExprParent(frame->body, getBlockExprParent(frame->body));
	setBlockExprUser(frame->body, this);
	setBlockExprUserType(frame->body, &FRAME_INSTANCE_ID);
	setResumable(frame->body, true);
}

bool FrameInstance::resume(MincRuntime* runtime, MincSymbol& result, bool& done)
{
	// Avoid executing frame->body while it's being executed by another thread
	// This happens when a frame is resumed by a new thread while the old thread is still busy unrolling the stack
	if (isBlockExprBusy(frame->body))
	{
		eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, this, runtime, result), 0.0f); // Re-post self
		done = false;
		return false;
	}

	runtime->parentBlock = getBlockExprParent(frame->body);
	runtime->resume = suspended;
	runtime->heapFrame = &heapFrame; // Assign heap frame
	if (runExpr((MincExpr*)frame->body, *runtime))
	{
		if (runtime->result.type == &PAWS_RETURN_TYPE)
		{
			result.value = frame->returnType->alloc();
			frame->returnType->copyTo(runtime->result.value, result.value);
			result.type = frame->returnType;
			done = true;
			return false;
		}
		else if (runtime->result.type == &PAWS_AWAIT_TYPE)
		{
			suspended = true;
			done = false;
			return false;
		}
		else
			return true;
	}

	if (frame->returnType != getVoid().type && frame->returnType != PawsVoid::TYPE)
		raiseCompileError("missing return statement in frame body", (MincExpr*)frame->body);
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
			((EventPool*)funcArgs)->post(std::bind(&SingleshotAwaitableInstance::wakeup, sleepInstance, &runtime, MincSymbol(PawsVoid::TYPE, nullptr)), duration);
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(sleepInstance));
			return false;
		}, eventPool
	);

	// Define event definition
	defineExpr8(pkgScope, "event<$I<PawsType>>()",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			PawsType* returnType = (PawsType*)lookupSymbol(buildtime.parentBlock, getIdExprName((MincIdExpr*)params[0]))->value;
			buildtime.result = MincSymbol(Event::get(returnType), new PawsEventInstance(new EventInstance(returnType, (EventPool*)exprArgs)));
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
	defineStmt5(pkgScope, "$E<PawsType> frame $I($E<PawsType> $I, ...) $B",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* stmtArgs) {
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

			// Allocate types of arguments in frame block
			frame->args.reserve(frame->argTypes.size());
			for (size_t i = 0; i < frame->argTypes.size(); ++i)
				frame->args.push_back(allocStackSymbol(block, frame->argNames[i].c_str(), frame->argTypes[i], frame->argTypes[i]->size));

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
					const MincStackSymbol* symbol = allocStackSymbol(block, getIdExprName((MincIdExpr*)varAST), type, type->size);
					frame->variables[getIdExprName((MincIdExpr*)varAST)] = Frame::MincSymbol{symbol, exprAST};
					frame->size += type->size;

					// Build frame variable expression
					buildExpr(exprAST, buildtime);
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
					MincBlockExpr* instance = buildtime.parentBlock;
					while (getBlockExprUserType(instance) != &FRAME_ID)
					{
						setResumable(instance, true); // Make all blocks surrounding `await` resumable
						instance = getBlockExprParent(instance);
					}
				},
				[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* exprArgs) -> bool {
					if (runExpr(params[0], runtime))
						return true;
					AwaitableInstance* blocker = ((PawsAwaitableInstance*)runtime.result.value)->get();
					MincBlockExpr* instance = runtime.parentBlock;
					while (getBlockExprUserType(instance) != &FRAME_INSTANCE_ID)
						instance = getBlockExprParent(instance);
					if (blocker->awaitResult(&runtime, (FrameInstance*)getBlockExprUser(instance), runtime.result))
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
		}
	);

	// Define frame call
	class FrameCallKernel : public MincKernel
	{
		EventPool* const eventPool;
		const MincStackSymbol* const result;
	public:
		FrameCallKernel(EventPool* eventPool, const MincStackSymbol* result=nullptr) : eventPool(eventPool), result(result) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			Frame* frame = (Frame*)buildExpr(params[0], buildtime).value;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of frame arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = frame->argTypes[i], *gotType = ::getType(argExpr, buildtime.parentBlock);

				if (expectedType != gotType)
				{
					MincExpr* castExpr = lookupCast(buildtime.parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
						throw CompileError(buildtime.parentBlock, getLocation(argExpr), "invalid frame argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = castExpr;
				}
				buildExpr(argExprs[i], buildtime);
			}

			// Allocate anonymous symbol for return value in calling scope
			return new FrameCallKernel(eventPool, allocAnonymousStackSymbol(buildtime.parentBlock, frame->returnType, frame->returnType->size));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (runExpr(params[0], runtime))
				return true;
			Frame* frame = (Frame*)runtime.result.value;
			std::vector<MincExpr*>& argExprs = getListExprExprs((MincListExpr*)params[1]);

			// Instantiate frame
			FrameInstance* instance = new FrameInstance(frame, runtime.parentBlock, argExprs, eventPool, result);
			runtime.heapFrame = &instance->heapFrame; // Assign heap frame
			MincEnteredBlockExpr entered(runtime, frame->body); // Enter block to trigger heap frame allocation
			//TODO: This will break for recursive frames if a parameter contains another call of the same frame

			// Assign arguments in frame instance
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				if (runExpr(argExprs[i], runtime))
					return true;
				assert(frame->args[i]->type == runtime.result.type);
				MincObject* var = getStackSymbol(frame->body, runtime, frame->args[i]);
				((PawsType*)runtime.result.type)->allocTo(var);
				((PawsType*)runtime.result.type)->copyTo(runtime.result.value, var);
			}

			// Initialize and assign frame variables in frame instance
			runtime.parentBlock = frame->body;
			for (const std::pair<const std::string, Frame::MincSymbol>& pair: frame->variables)
			{
				if (runExpr(pair.second.initExpr, runtime))
					return true;
				assert(pair.second.symbol->type == runtime.result.type);
				MincObject* var = getStackSymbol(frame->body, runtime, pair.second.symbol);
				((PawsType*)runtime.result.type)->allocTo(var);
				((PawsType*)runtime.result.type)->copyTo(runtime.result.value, var);
				instance->variables[pair.first] = var;
			}

			// Call frame
			eventPool->post(std::bind(&FrameInstance::wakeup, instance, &runtime, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f);

			runtime.result = MincSymbol(frame, new PawsFrameInstance(instance));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(ExprIsCast(params[0]));
			Frame* frame = (Frame*)((PawsTpltType*)::getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock))->tpltType;
			return frame;
		}
	};
	defineExpr6(pkgScope, "$E<PawsFrame>($E, ...)", new FrameCallKernel(eventPool));

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
			runtime.result = MincSymbol(variable->second.symbol->type, instance->variables[memberName]);
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			MincExpr* expr = params[0];
			if (ExprIsCast(expr))
				expr = getCastExprSource((MincCastExpr*)expr);
			Frame* frame = (Frame*)(getType(expr, parentBlock));
			std::string memberName = getIdExprName((MincIdExpr*)params[1]);

			auto variable = frame->variables.find(memberName);
			return variable == frame->variables.end() ? nullptr : variable->second.symbol->type;
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
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AnyAwaitableInstance(&runtime, a, b)));
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
			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(new AllAwaitableInstance(&runtime, a, b)));
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

			if (!blocker->awaitResult(&runtime, new TopLevelInstance(eventPool), runtime.result))
			{
				eventPool->run();
				blocker->getResult(&runtime, &runtime.result);
			}
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			assert(ExprIsCast(params[0]));
			const Awaitable* event = (Awaitable*)getType(getCastExprSource((MincCastExpr*)params[0]), parentBlock);
			return event->returnType;
		}, eventPool
	);
	// Define await non-awaitable expression
	defineExpr7(pkgScope, "await $E",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			getType(params[0], buildtime.parentBlock); // Raise expression errors if any
			raiseCompileError("expression is not awaitable", params[0]);
		},
		getErrorType()
	);
	// Define await non-awaitable identifier
	defineExpr7(pkgScope, "await $I",
		[](MincBuildtime& buildtime, std::vector<MincExpr*>& params, void* exprArgs) {
			const char* name = getIdExprName((MincIdExpr*)params[0]);
			if (lookupSymbol(buildtime.parentBlock, name) == nullptr && lookupStackSymbol(buildtime.parentBlock, name) == nullptr)
				raiseCompileError(('`' + std::string(name) + "` was not declared in this scope").c_str(), params[0]);
			else
				raiseCompileError(('`' + std::string(name) + "` is not awaitable").c_str(), params[0]);
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
			((EventPool*)exprArgs)->post(std::bind(&EventInstance::invoke, event, &runtime, invokeInstance, runtime.result.value), 0.0f);

			runtime.result = MincSymbol(Awaitable::get(PawsVoid::TYPE), new PawsAwaitableInstance(invokeInstance));
			return false;
		}, [](const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params, void* exprArgs) -> MincObject* {
			return Awaitable::get(PawsVoid::TYPE);
		}, eventPool
	);
}