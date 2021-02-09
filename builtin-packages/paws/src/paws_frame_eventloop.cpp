#include "paws_frame_eventloop.h"
#include "minc_api.hpp"

thread_local IEventLoop* threadlocalEventLoop = nullptr;

void IEventLoop::aquire()
{
	if (threadlocalEventLoop == this)
		return; // Ignore if this event loop is already current
	if (threadlocalEventLoop == nullptr)
		throw CompileError("No event loop to aquire");
	EventLoop* oldEventLoop = dynamic_cast<EventLoop*>(threadlocalEventLoop);
	if (dynamic_cast<EventLoop*>(threadlocalEventLoop) == nullptr)
		throw CompileError("Can't aquire custom event loop");

	oldEventLoop->mutex.lock();

	// Activate new eventloop
	this->owner = oldEventLoop->owner;
	threadlocalEventLoop = this;
	oldEventLoop->owner = nullptr;
	this->owner->swapEventLoop(oldEventLoop, this);

	// Take over queued messages
	if (!oldEventLoop->eventQueue.empty())
		for (EventLoop::EventQueueValueType front = oldEventLoop->eventQueue.top(); !oldEventLoop->eventQueue.empty(); oldEventLoop->eventQueue.pop())
			this->post(front.second, 1e-9f * std::chrono::duration_cast<std::chrono::nanoseconds>(front.first - std::chrono::high_resolution_clock::now()).count());

	oldEventLoop->mutex.unlock();

	// Run new event loop from old event loop
	oldEventLoop->post(std::bind(&IEventLoop::run, this), 0.0f);

	// Close old event loop after new event loop is closed
	oldEventLoop->post(std::bind(&IEventLoop::close, oldEventLoop), 0.0f);
}

EventLoop::EventLoop(EventPool* owner) : IEventLoop(owner), lk(cv_m), running(false), ready(false) {}

void EventLoop::run()
{
	running = true;
	mutex.lock();
	if (running && eventQueue.empty())
	{
		ready = true;
		mutex.unlock();
		cv.wait(lk);
		mutex.lock();
		ready = false;
	}

	EventQueueValueType front;
	while (running)
	{
		EventQueueValueType front = eventQueue.top();
		while (std::chrono::high_resolution_clock::now() < front.first)
		{
			mutex.unlock();
			cv.wait_until(lk, front.first);
			mutex.lock();
			front = eventQueue.top();
		}

		eventQueue.pop();
		mutex.unlock();
		front.second();
		mutex.lock();

		if (running && eventQueue.empty())
		{
			ready = true;
			mutex.unlock();
			cv.wait(lk);
			mutex.lock();
			ready = false;
		}
	}
	mutex.unlock();
}

void EventLoop::close()
{
	if (running)
	{
		running = ready = false;
		cv.notify_one();
	}
}

void EventLoop::post(CALLBACK_TYPE callback, float duration)
{
	mutex.lock();
	ready = false;
	eventQueue.push(EventQueueValueType(std::chrono::high_resolution_clock::now() + std::chrono::nanoseconds((long long)(1e9f * duration)), callback));
	mutex.unlock();
	cv.notify_one();
}

bool EventLoop::idle()
{
	return ready;
}

void EventPool::eventThreadFunc(IEventLoop* eventLoop)
{
	threadlocalEventLoop = eventLoop; // Register eventLoop as threadlocalEventLoop for this thread
	eventLoop->run();
	pthread_exit(nullptr);
}

EventPool::EventPool(size_t poolSize)
{
	if (poolSize < 1)
		throw std::invalid_argument("Event pool size must be at least 1");

	// Create eventloops
	for (size_t i = 0; i < poolSize; ++i)
		eventLoops.push_back(new EventLoop(this));

	// Register first event loop as threadlocalEventLoop for the main thread
	threadlocalEventLoop = eventLoops[0];
}
EventPool::~EventPool()
{
	// Close all event loops
	close();

	// Wait for all event loops to finish
	for (std::thread& eventThread: eventThreads)
		eventThread.join();

	// Remove threads
	eventThreads.clear();

	// Remove all event loops
	for (IEventLoop* eventLoop: eventLoops)
		delete eventLoop;
	eventLoops.clear();
}
void EventPool::run()
{
	// Spawn threads for each but the first eventloop
	for (size_t i = 1; i < eventLoops.size(); ++i)
		eventThreads.push_back(std::thread(eventThreadFunc, eventLoops[i]));

	// Process already queued events
	size_t numQueuedEvents = eventQueue.size();
	if (numQueuedEvents != 0)
	{
		if (numQueuedEvents > eventLoops.size())
			numQueuedEvents = eventLoops.size();
		for (size_t i = 0; i < numQueuedEvents; ++i)
			eventLoops[i]->post(std::bind(&EventPool::dequeue, this), 0.0f);
	}

	// Run first event loop on the current thread
	eventLoops[0]->run();

	// Wait for all eventloops to finish
	for (std::thread& eventThread: eventThreads)
		eventThread.join();

	// Remove threads
	eventThreads.clear();
}

void EventPool::post(CALLBACK_TYPE callback, float duration)
{
	enqueue(callback, duration);
}

void EventPool::close()
{
	for (IEventLoop* eventLoop: eventLoops)
		eventLoop->close();
}

void EventPool::swapEventLoop(IEventLoop* oldEventLoop, IEventLoop* newEventLoop)
{
	for (size_t i = 0, n = eventLoops.size(); i < n; ++i)
		if (eventLoops[i] == oldEventLoop)
		{
			eventLoops[i] = newEventLoop;
			break;
		}
}

void EventPool::enqueue(CALLBACK_TYPE callback, float duration)
{
	if (eventLoops.size() == 1) // If this pool consists of only one event loop, ...
		eventLoops[0]->post(callback, duration); // Pass event to event loop
	else if (duration > 0.0f)
	{
		eventLoops.back()->post(std::bind(&EventPool::enqueue, this, callback, 0.0f), duration);
		//TODO: Use dedicated delay thread to wait `duration`, then call enqueue() again!
	}
	else // duration == 0.0f
	{
		// Place event on the shared queue
		eventQueueMutex.lock();
		eventQueue.push(callback);
		eventQueueMutex.unlock();

		// Wake up any idle event (if any) to process the queued event
		for (IEventLoop* eventLoop: eventLoops)
			if (eventLoop->idle())
			{
				eventLoop->post(std::bind(&EventPool::dequeue, this), 0.0f);
				break;
			}
	}
}

void EventPool::dequeue()
{
	while (true)
	{
		// Pop event from shared queue
		eventQueueMutex.lock();
		if (eventQueue.empty())
		{
			eventQueueMutex.unlock();
			return;
		}
		CALLBACK_TYPE callback = eventQueue.front();
		eventQueue.pop();
		eventQueueMutex.unlock();

		// Execute event on current thread
		callback();
	}
}