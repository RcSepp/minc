#ifndef __PAWS_FRAME_EVENTLOOP_H
#define __PAWS_FRAME_EVENTLOOP_H

#include <queue>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <condition_variable>
#include <functional>

typedef std::function<void()> CALLBACK_TYPE;

class EventPool;

class IEventLoop
{
protected:
	EventPool* owner;

public:
	IEventLoop(EventPool* owner = nullptr) : owner(owner) {}
	virtual ~IEventLoop() {}
	virtual void run() = 0;
	virtual void close() = 0;
	virtual bool idle() = 0;
	virtual void post(CALLBACK_TYPE callback, float duration) = 0;

	void aquire();
};

class EventLoop : public IEventLoop
{
private:
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk;
	std::condition_variable cv;
	bool running, ready;

public:
	std::mutex mutex;

	typedef std::pair<std::chrono::time_point<std::chrono::high_resolution_clock>, CALLBACK_TYPE> EventQueueValueType;
	struct EventQueueCompare {
		constexpr bool operator()( EventQueueValueType const & a, EventQueueValueType const & b) const
		{
			return a.first > b.first;
		}
	};
	std::priority_queue<EventQueueValueType, std::vector<EventQueueValueType>, EventQueueCompare> eventQueue;

	EventLoop(EventPool* owner = nullptr);
	void run();
	void close();
	void post(CALLBACK_TYPE callback, float duration);
	bool idle();
};

class EventPool
{
private:
	std::vector<IEventLoop*> eventLoops;
	std::vector<std::thread> eventThreads;
	std::queue<CALLBACK_TYPE> eventQueue;
	std::mutex eventQueueMutex;

	static void eventThreadFunc(IEventLoop* eventLoop);

public:
	EventPool(size_t poolSize);
	~EventPool();
	void run();
	void post(CALLBACK_TYPE callback, float duration);
	void close();
	void swapEventLoop(IEventLoop* oldEventLoop, IEventLoop* newEventLoop);

private:
	void enqueue(CALLBACK_TYPE callback, float duration);
	void dequeue();
};

#endif