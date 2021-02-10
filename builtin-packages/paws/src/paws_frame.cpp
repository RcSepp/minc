#include "paws_frame_eventloop.h"
#include <cassert>
#include <vector>
#include <list>
#include <queue>
#include <limits> // For NaN
#include <cmath> // For isnan()
#include "minc_api.hpp"
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
			throw CompileError("Event awaited more than once"); //TODO: Raise runtime exception instead
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
		pawsFrameScope->defineSymbol(t->name, PawsType::TYPE, t);
		pawsFrameScope->defineCast(new InheritanceCast(t, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE)));
		pawsFrameScope->defineCast(new InheritanceCast(t, PawsAwaitableInstance::TYPE, new MincOpaqueCastKernel(PawsAwaitableInstance::TYPE)));
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
		pawsFrameScope->defineSymbol(t->name, PawsType::TYPE, t);
		pawsFrameScope->defineCast(new InheritanceCast(t, PawsBase::TYPE, new MincOpaqueCastKernel(PawsBase::TYPE)));
		pawsFrameScope->defineCast(new InheritanceCast(t, PawsEventInstance::TYPE, new MincOpaqueCastKernel(PawsEventInstance::TYPE)));
pawsFrameScope->defineCast(new InheritanceCast(PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE, new MincOpaqueCastKernel(PawsAwaitableInstance::TYPE))); //TODO: This shouldn't be necessary
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
	frame->body->parent = frame->body->parent;
	frame->body->user = this;
	frame->body->userType = &FRAME_INSTANCE_ID;
	frame->body->isResumable = true;
}

bool FrameInstance::resume(MincRuntime* runtime, MincSymbol& result, bool& done)
{
	// Avoid executing frame->body while it's being executed by another thread
	// This happens when a frame is resumed by a new thread while the old thread is still busy unrolling the stack
	if (frame->body->isBusy)
	{
		eventPool->post(std::bind(&SingleshotAwaitableInstance::wakeup, this, runtime, result), 0.0f); // Re-post self
		done = false;
		return false;
	}

	runtime->parentBlock = frame->body->parent;
	runtime->resume = suspended;
	runtime->heapFrame = &heapFrame; // Assign heap frame
	if (frame->body->run(*runtime))
	{
		if (runtime->exceptionType == &PAWS_RETURN_TYPE)
		{
			result.value = frame->returnType->alloc();
			frame->returnType->copyTo(runtime->result, result.value);
			result.type = frame->returnType;
			done = true;
			return false;
		}
		else if (runtime->exceptionType == &PAWS_AWAIT_TYPE)
		{
			suspended = true;
			done = false;
			return false;
		}
		else
			return true;
	}

	if (frame->returnType != getVoid().type && frame->returnType != PawsVoid::TYPE)
		throw CompileError(runtime->parentBlock, frame->body->loc, "missing return statement in frame body");
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
	pkgScope->defineCast(new InheritanceCast(Frame::TYPE, Awaitable::TYPE, new MincOpaqueCastKernel(Awaitable::TYPE)));
	pkgScope->defineCast(new InheritanceCast(Awaitable::TYPE, PawsType::TYPE, new MincOpaqueCastKernel(PawsType::TYPE)));

	registerType<PawsAwaitableInstance>(pkgScope, "PawsAwaitableInstance");
	registerType<PawsEventInstance>(pkgScope, "PawsEventInstance");
	pkgScope->defineCast(new InheritanceCast(PawsEventInstance::TYPE, PawsAwaitableInstance::TYPE, new MincOpaqueCastKernel(PawsAwaitableInstance::TYPE)));
	registerType<PawsFrameInstance>(pkgScope, "PawsFrameInstance");
	pkgScope->defineCast(new InheritanceCast(PawsFrameInstance::TYPE, PawsAwaitableInstance::TYPE, new MincOpaqueCastKernel(PawsAwaitableInstance::TYPE)));

	// Define sleep function
	defineConstantFunction(pkgScope, "sleep", Awaitable::get(PawsVoid::TYPE), { PawsDouble::TYPE }, { "duration" },
		[](MincRuntime& runtime, const std::vector<MincExpr*>& args, void* funcArgs) -> bool
		{
			if(args[0]->run(runtime))
				return true;
			double duration = ((PawsDouble*)runtime.result)->get();
			SingleshotAwaitableInstance* sleepInstance = new SingleshotAwaitableInstance();
			((EventPool*)funcArgs)->post(std::bind(&SingleshotAwaitableInstance::wakeup, sleepInstance, &runtime, MincSymbol(PawsVoid::TYPE, nullptr)), duration);
			runtime.result = new PawsAwaitableInstance(sleepInstance);
			return false;
		}, eventPool
	);

	// Define event definition
	class EventConstructorKernel : public MincKernel
	{
		EventPool* const eventPool;
		const MincSymbol symbol;
	public:
		EventConstructorKernel(EventPool* eventPool) : eventPool(eventPool), symbol() {}
		EventConstructorKernel(EventPool* eventPool, const MincSymbol& symbol) : eventPool(eventPool), symbol(symbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)buildtime.parentBlock->lookupSymbol(((MincIdExpr*)params[0])->name)->value;
			buildtime.result = MincSymbol(Event::get(returnType), new PawsEventInstance(new EventInstance(returnType, eventPool)));
			return new EventConstructorKernel(eventPool, buildtime.result);
		}
		void dispose(MincKernel* kernel)
		{
			delete this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			runtime.result = symbol.value;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			PawsType* returnType = (PawsType*)parentBlock->lookupSymbol(((MincIdExpr*)params[0])->name)->value;
			return Event::get(returnType);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("event<$I<PawsType>>()")[0], new EventConstructorKernel(eventPool));

	struct EventTypeKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			runtime.result = Event::get((PawsType*)runtime.result);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return PawsType::TYPE;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("event<$E<PawsType>>")[0], new EventTypeKernel());

	// Define frame definition
	struct FrameDefinitionKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			PawsType* returnType = (PawsType*)params[0]->build(buildtime).value;
			const std::string& frameName = ((MincIdExpr*)params[1])->name;
			const std::vector<MincExpr*>& argTypeExprs = ((MincListExpr*)params[2])->exprs;
			const std::vector<MincExpr*>& argNameExprs = ((MincListExpr*)params[3])->exprs;
			MincBlockExpr* block = (MincBlockExpr*)params[4];

			// Set frame parent to frame definition scope
			block->parent = buildtime.parentBlock;

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (MincExpr* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back((PawsType*)argTypeExpr->build(buildtime).value);
			frame->argNames.reserve(argNameExprs.size());
			for (MincExpr* argNameExpr: argNameExprs)
				frame->argNames.push_back(((MincIdExpr*)argNameExpr)->name);
			frame->body = block;

			block->user = frame;
			block->userType = &FRAME_ID;

			// Allocate types of arguments in frame block
			frame->args.reserve(frame->argTypes.size());
			for (size_t i = 0; i < frame->argTypes.size(); ++i)
				frame->args.push_back(block->allocStackSymbol(frame->argNames[i], frame->argTypes[i], frame->argTypes[i]->size));

			// Define frame variable assignment
			struct FrameVariableDeclarationKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					MincExpr* exprAST = params[1];
					if (exprAST->exprtype == MincExpr::ExprType::CAST)
						exprAST = ((MincCastExpr*)exprAST)->getSourceExpr();

					MincExpr* varAST = params[0];
					if (varAST->exprtype == MincExpr::ExprType::CAST)
						varAST = ((MincCastExpr*)varAST)->getSourceExpr();

					MincBlockExpr* block = buildtime.parentBlock;
					while (block->userType != &FRAME_ID)
						block = block->parent;
					assert(block);
					Frame* frame = (Frame*)block->user;
					PawsType* type = (PawsType*)exprAST->getType(buildtime.parentBlock);
					const MincStackSymbol* symbol = block->allocStackSymbol(((MincIdExpr*)varAST)->name, type, type->size);
					frame->variables[((MincIdExpr*)varAST)->name] = Frame::MincSymbol{symbol, exprAST};
					frame->size += type->size;

					// Build frame variable expression
					exprAST->build(buildtime);
					return this;
				}

				bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
				{
					return false;
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			block->defineStmt(MincBlockExpr::parseCTplt("public $I = $E<PawsBase>"), new FrameVariableDeclarationKernel());

			struct DefaultKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					MincBlockExpr* block = buildtime.parentBlock;
					while (block->userType != &FRAME_ID)
						block = block->parent;
					assert(block);

					// Prohibit declarations of frame variables after other statements
					struct ProhibitedFrameVariableDeclarationKernel : public MincKernel
					{
						MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
						{
							throw CompileError(buildtime.parentBlock, params[0]->loc, "frame variables have to be defined at the beginning of the frame");
						}

						bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
						{
							return false;
						}

						MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
						{
							return getVoid().type;
						}
					};
					block->defineStmt(MincBlockExpr::parseCTplt("public $I = $E<PawsBase>"), new ProhibitedFrameVariableDeclarationKernel());

					// Unset default statement
					block->defineDefaultStmt(nullptr);

					// Build current statement
					params[0]->build(buildtime);
					return this;
				}

				bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
				{
					// Run current statement
					return params[0]->run(runtime);
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					return getVoid().type;
				}
			};
			block->defineDefaultStmt(new DefaultKernel());

			// Define await expression in frame instance scope
			struct AwaitKernel : public MincKernel
			{
				MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
				{
					(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
					MincBlockExpr* instance = buildtime.parentBlock;
					while (instance->userType != &FRAME_ID)
					{
						instance->isResumable = true; // Make all blocks surrounding `await` resumable
						instance = instance->parent;
					}
					return this;
				}

				bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
				{
					if (params[0]->run(runtime))
						return true;
					AwaitableInstance* blocker = ((PawsAwaitableInstance*)runtime.result)->get();
					MincBlockExpr* instance = runtime.parentBlock;
					while (instance->userType != &FRAME_INSTANCE_ID)
						instance = instance->parent;
					MincSymbol result(PawsAwaitableInstance::TYPE, nullptr);
					if (blocker->awaitResult(&runtime, (FrameInstance*)instance->user, result))
					{
						runtime.result = result.value;
						return false;
					}
					else
					{
						runtime.exceptionType = &PAWS_AWAIT_TYPE;
						runtime.result = nullptr;
						return true;
					}
				}

				MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
				{
					assert(params[0]->exprtype == MincExpr::ExprType::CAST);
					const Awaitable* event = (Awaitable*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock);
					return event->returnType;;
				}
			};
			block->defineExpr(MincBlockExpr::parseCTplt("await $E<PawsAwaitableInstance>")[0], new AwaitKernel());

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
			block->name = frameFullName;

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType, "frame");

			// Build frame
			block->build(buildtime);

			frame->name = frameName;
			buildtime.parentBlock->defineSymbol(frameName, PawsTpltType::get(buildtime.parentBlock, Frame::TYPE, frame), frame);
			buildtime.parentBlock->defineCast(new InheritanceCast(frame, PawsFrameInstance::TYPE, new MincOpaqueCastKernel(PawsFrameInstance::TYPE)));
			buildtime.parentBlock->defineCast(new InheritanceCast(PawsTpltType::get(buildtime.parentBlock, Frame::TYPE, frame), PawsType::TYPE, new MincOpaqueCastKernel(PawsType::TYPE)));
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("$E<PawsType> frame $I($E<PawsType> $I, ...) $B"), new FrameDefinitionKernel());

	// Define frame call
	class FrameCallKernel : public MincKernel
	{
		EventPool* const eventPool;
		const MincStackSymbol* const result;
	public:
		FrameCallKernel(EventPool* eventPool, const MincStackSymbol* result=nullptr) : eventPool(eventPool), result(result) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			Frame* frame = (Frame*)params[0]->build(buildtime).value;
			std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				throw CompileError(buildtime.parentBlock, params[0]->loc, "invalid number of frame arguments");

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				MincExpr* argExpr = argExprs[i];
				MincObject *expectedType = frame->argTypes[i], *gotType = argExpr->getType(buildtime.parentBlock);

				if (expectedType != gotType)
				{
					const MincCast* cast = buildtime.parentBlock->lookupCast(gotType, expectedType);
					if (cast == nullptr)
						throw CompileError(buildtime.parentBlock, argExpr->loc, "invalid frame argument type: %E<%t>, expected: <%t>", argExpr, gotType, expectedType);
					argExprs[i] = new MincCastExpr(cast, argExpr);
				}
				argExprs[i]->build(buildtime);
			}

			// Allocate anonymous symbol for return value in calling scope
			return new FrameCallKernel(eventPool, buildtime.parentBlock->allocStackSymbol(frame->returnType, frame->returnType->size));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			Frame* frame = (Frame*)runtime.result;
			std::vector<MincExpr*>& argExprs = ((MincListExpr*)params[1])->exprs;

			// Instantiate frame
			FrameInstance* instance = new FrameInstance(frame, runtime.parentBlock, argExprs, eventPool, result);
			runtime.heapFrame = &instance->heapFrame; // Assign heap frame
			MincEnteredBlockExpr entered(runtime, frame->body); // Enter block to trigger heap frame allocation
			//TODO: This will break for recursive frames if a parameter contains another call of the same frame

			// Assign arguments in frame instance
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				if (argExprs[i]->run(runtime))
					return true;
				MincObject* var = frame->body->getStackSymbol(runtime, frame->args[i]);
				((PawsType*)frame->args[i]->type)->copyToNew(runtime.result, var);
			}

			// Initialize and assign frame variables in frame instance
			runtime.parentBlock = frame->body;
			for (const std::pair<const std::string, Frame::MincSymbol>& pair: frame->variables)
			{
				if (pair.second.initExpr->run(runtime))
					return true;
				MincObject* var = frame->body->getStackSymbol(runtime, pair.second.symbol);
				((PawsType*)pair.second.symbol->type)->copyToNew(runtime.result, var);
				instance->variables[pair.first] = var;
			}

			// Call frame
			eventPool->post(std::bind(&FrameInstance::wakeup, instance, &runtime, MincSymbol(PawsVoid::TYPE, nullptr)), 0.0f);

			runtime.result = new PawsFrameInstance(instance);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(params[0]->exprtype == MincExpr::ExprType::CAST);
			Frame* frame = (Frame*)((PawsTpltType*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock))->tpltType;
			return frame;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsFrame>($E, ...)")[0], new FrameCallKernel(eventPool));

	// Define frame member getter
	class FrameMemberGetterKernel : public MincKernel
	{
		Frame* const frame;
	public:
		FrameMemberGetterKernel(Frame* frame=nullptr) : frame(frame) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
			Frame* frame = (Frame*)params[0]->getType(buildtime.parentBlock);
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			auto variable = frame->variables.find(memberName);
			if (variable == frame->variables.end())
				throw CompileError(buildtime.parentBlock, params[1]->loc, "no member named '%S' in '%S'", memberName, frame->name);
			return new FrameMemberGetterKernel(frame);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			FrameInstance* instance = ((PawsFrameInstance*)runtime.result)->get();
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			runtime.result = instance->variables[memberName];
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			MincExpr* expr = params[0];
			if (expr->exprtype == MincExpr::ExprType::CAST)
				expr = ((MincCastExpr*)expr)->getSourceExpr();
			Frame* frame = (Frame*)expr->getType(parentBlock);
			const std::string& memberName = ((MincIdExpr*)params[1])->name;

			auto variable = frame->variables.find(memberName);
			return variable == frame->variables.end() ? nullptr : variable->second.symbol->type;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsFrameInstance>.$I")[0], new FrameMemberGetterKernel());

	// Define boolean operators on awaitables
	struct LogicalOrKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
			(params[1] = ((MincCastExpr*)params[1])->getSourceExpr())->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			AwaitableInstance* a = ((PawsAwaitableInstance*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			AwaitableInstance* b = ((PawsAwaitableInstance*)runtime.result)->get();
			runtime.result = new PawsAwaitableInstance(new AnyAwaitableInstance(&runtime, a, b));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return Awaitable::get(PawsVoid::TYPE);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsAwaitableInstance> || $E<PawsAwaitableInstance>")[0], new LogicalOrKernel());

	struct LogicalAndKernel : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
			(params[1] = ((MincCastExpr*)params[1])->getSourceExpr())->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			AwaitableInstance* a = ((PawsAwaitableInstance*)runtime.result)->get();
			if (params[1]->run(runtime))
				return true;
			AwaitableInstance* b = ((PawsAwaitableInstance*)runtime.result)->get();
			runtime.result = new PawsAwaitableInstance(new AllAwaitableInstance(&runtime, a, b));
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return Awaitable::get(PawsVoid::TYPE);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsAwaitableInstance> && $E<PawsAwaitableInstance>")[0], new LogicalAndKernel());

	// Define top-level await expression
	class TopLevelAwaitKernel : public MincKernel
	{
		EventPool* const eventPool;
	public:
		TopLevelAwaitKernel(EventPool* eventPool) : eventPool(eventPool) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			AwaitableInstance* blocker = ((PawsAwaitableInstance*)runtime.result)->get();
			MincSymbol result(PawsAwaitableInstance::TYPE, nullptr);

			if (!blocker->awaitResult(&runtime, new TopLevelInstance(eventPool), result))
			{
				eventPool->run();
				blocker->getResult(&runtime, &result);
			}
			runtime.result = result.value;
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			assert(params[0]->exprtype == MincExpr::ExprType::CAST);
			const Awaitable* event = (Awaitable*)((MincCastExpr*)params[0])->getSourceExpr()->getType(parentBlock);
			return event->returnType;
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("await $E<PawsAwaitableInstance>")[0], new TopLevelAwaitKernel(eventPool));

	// Define await non-awaitable expression
	struct TopLevelNonAwaitableKernel1 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->getType(buildtime.parentBlock); // Raise expression errors if any
			throw CompileError(buildtime.parentBlock, params[0]->loc, "expression is not awaitable");
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("await $E")[0], new TopLevelNonAwaitableKernel1());

	// Define await non-awaitable identifier
	struct TopLevelNonAwaitableKernel2 : public MincKernel
	{
		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& name = ((MincIdExpr*)params[0])->name;
			if (buildtime.parentBlock->lookupSymbol(name) == nullptr && buildtime.parentBlock->lookupStackSymbol(name) == nullptr)
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` was not declared in this scope", name);
			else
				throw CompileError(buildtime.parentBlock, params[0]->loc, "`%S` is not awaitable", name);
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getErrorType();
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("await $I")[0], new TopLevelNonAwaitableKernel2());

	// Define event call
	class EventCallKernel : public MincKernel
	{
		EventPool* const eventPool;
	public:
		EventCallKernel(EventPool* eventPool) : eventPool(eventPool) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			(params[0] = ((MincCastExpr*)params[0])->getSourceExpr())->build(buildtime);
			MincExpr* argExpr = params[1];

			MincObject* const argType = argExpr->getType(buildtime.parentBlock);
			PawsType* const msgType = ((Event*)params[0]->getType(buildtime.parentBlock))->msgType;
			if (msgType != argType)
			{
				const MincCast* cast = buildtime.parentBlock->lookupCast(argType, msgType);
				if (cast == nullptr)
					throw CompileError(buildtime.parentBlock, argExpr->loc, "invalid event type: %E<%t>, expected: <%t>", argExpr, argType, msgType);
				argExpr = new MincCastExpr(cast, argExpr);
			}
			(params[1] = argExpr)->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			EventInstance* const event = ((PawsEventInstance*)runtime.result)->get();
			MincExpr* argExpr = params[1];

			// Invoke event
			if (argExpr->run(runtime))
				return true;
			SingleshotAwaitableInstance* invokeInstance = new SingleshotAwaitableInstance();
			eventPool->post(std::bind(&EventInstance::invoke, event, &runtime, invokeInstance, runtime.result), 0.0f);

			runtime.result = new PawsAwaitableInstance(invokeInstance);
			return false;
		}

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return Awaitable::get(PawsVoid::TYPE);
		}
	};
	pkgScope->defineExpr(MincBlockExpr::parseCTplt("$E<PawsEventInstance>($E)")[0], new EventCallKernel(eventPool));
}