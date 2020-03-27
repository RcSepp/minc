#include <queue>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <condition_variable>
#include <functional>

using namespace std;

typedef std::function<void()> CALLBACK_TYPE;

class EventLoop;
thread_local EventLoop* currentEventLoop;
class EventLoop
{
private:
	mutex cv_m;
	unique_lock<mutex> lk;
	condition_variable cv;
	bool running;

	typedef pair<std::chrono::time_point<std::chrono::high_resolution_clock>, CALLBACK_TYPE> EventQueueValueType;
	struct EventQueueCompare {
		constexpr bool operator()( EventQueueValueType const & a, EventQueueValueType const & b) const
		{
			return a.first > b.first;
		}
	};
	priority_queue<EventQueueValueType, vector<EventQueueValueType>, EventQueueCompare> eventQueue;

public:
	EventLoop() : lk(cv_m), running(true)
	{
		currentEventLoop = this;
	}
	EventLoop(const EventLoop& src) : lk(cv_m), running(src.running), eventQueue(src.eventQueue) {}

	void run()
	{
		if (eventQueue.empty()) cv.wait(lk); //TODO: Make atomic

		EventQueueValueType front;
		while (running)
		{
			EventQueueValueType front = eventQueue.top();
			while (chrono::high_resolution_clock::now() < front.first)
			{
				cv.wait_until(lk, front.first);
				front = eventQueue.top();
			}

			eventQueue.pop();
			front.second();

			if (running && eventQueue.empty()) cv.wait(lk); //TODO: Make atomic
		}
	}

	void close()
	{
		running = false;
		cv.notify_one();
	}

	void post(CALLBACK_TYPE callback, float duration)
	{
		if (currentEventLoop == this && duration <= 0.0f)
			callback();
		else
		{
			eventQueue.push(EventQueueValueType(chrono::high_resolution_clock::now() + chrono::nanoseconds((long long)(1e9f * duration)), callback));
			cv.notify_one();
		}
	}
};
