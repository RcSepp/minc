#include <queue>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <condition_variable>
#include <functional>

typedef std::function<void()> CALLBACK_TYPE;

class EventLoop;
thread_local EventLoop* threadlocalEventLoop = nullptr;
class EventLoop
{
private:
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk;
	std::condition_variable cv;
	bool running, ready;

	typedef std::pair<std::chrono::time_point<std::chrono::high_resolution_clock>, CALLBACK_TYPE> EventQueueValueType;
	struct EventQueueCompare {
		constexpr bool operator()( EventQueueValueType const & a, EventQueueValueType const & b) const
		{
			return a.first > b.first;
		}
	};
	std::priority_queue<EventQueueValueType, std::vector<EventQueueValueType>, EventQueueCompare> eventQueue;

public:
	EventLoop() : lk(cv_m), running(true), ready(false)
	{
		threadlocalEventLoop = this; // Register as threadlocalEventLoop for this thread
	}

	void run()
	{
		if (running && eventQueue.empty()) { ready = true; cv.wait(lk); ready = false; } //TODO: Make atomic

		EventQueueValueType front;
		while (running)
		{
			EventQueueValueType front = eventQueue.top();
			while (std::chrono::high_resolution_clock::now() < front.first)
			{
				cv.wait_until(lk, front.first);
				front = eventQueue.top();
			}

			eventQueue.pop();
			front.second();

			if (running && eventQueue.empty()) { ready = true; cv.wait(lk); ready = false; } //TODO: Make atomic
		}
	}

	void close()
	{
		if (running)
		{
			running = ready = false;
			cv.notify_one();
		}
	}

	void post(CALLBACK_TYPE callback, float duration)
	{
		ready = false;
		eventQueue.push(EventQueueValueType(std::chrono::high_resolution_clock::now() + std::chrono::nanoseconds((long long)(1e9f * duration)), callback));
		cv.notify_one();
	}

	bool idle()
	{
		return ready;
	}
};

class EventPool
{
private:
	std::vector<EventLoop*> eventLoops;
	std::vector<std::thread> eventThreads;
	std::queue<CALLBACK_TYPE> eventQueue;
	std::mutex eventQueueMutex;

	static void eventThreadFunc(EventLoop* eventLoop)
	{
		threadlocalEventLoop = eventLoop; // Register eventLoop as threadlocalEventLoop for this thread
		eventLoop->run();
		pthread_exit(nullptr);
	}

public:
	EventPool(size_t poolSize)
	{
		if (poolSize < 1)
			throw std::invalid_argument("Event pool size must be at least 1");

		// Create eventloops
		for (size_t i = 0; i < poolSize; ++i)
			eventLoops.push_back(new EventLoop());
	}
	~EventPool()
	{
		// Close all event loops
		close();

		// Wait for all eventloops to finish
		for (std::thread& eventThread: eventThreads)
			eventThread.join();

		// Remove threads
		eventThreads.clear();

		// Remove all event loops
		for (EventLoop* eventLoop: eventLoops)
			delete eventLoop;
		eventLoops.clear();
	}
	void run()
	{
		// Spawn threads for each but the first eventloop
		for (size_t i = 1; i < eventLoops.size(); ++i)
			eventThreads.push_back(std::thread(eventThreadFunc, eventLoops[i]));

		// Register first event loop as threadlocalEventLoop for this thread
		threadlocalEventLoop = eventLoops[0];

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

	void post(CALLBACK_TYPE callback, float duration)
	{
		enqueue(callback, duration);
	}

	void close()
	{
		for (EventLoop* eventLoop: eventLoops)
			eventLoop->close();
	}


private:
	void enqueue(CALLBACK_TYPE callback, float duration)
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
			for (EventLoop* eventLoop: eventLoops)
				if (eventLoop->idle())
				{
					eventLoop->post(std::bind(&EventPool::dequeue, this), 0.0f);
					break;
				}
		}
	}

	void dequeue()
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
};
