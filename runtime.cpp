#include <queue>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <condition_variable>
#include <functional>

using namespace std;

/*class EventLoop
{
private:
	vector<thread> threads;
	thread_local EventLoop* currentEventLoop;

public:
	EventLoop() : currentEventLoop(this)
	{
	}

	void enqueue(float delay, void* callback, void* args, thread* eventLoopAffinity = NULL)
	{
		if (threads.size() == 1) // If running singlethreaded, ...
		{
			// Execute callback from current eventloop
			if (currentEventLoop == this)
				this->post(delay, callback, args)
			else
				this->invoke(delay, callback, args)
		}
		else if (eventLoopAffinity) // If a target eventloop was provided, ...
		{
			// Execute callback from target eventloop
			if (currentEventLoop == eventLoopAffinity)
				eventLoopAffinity.post(delay, callback, args)
			else:
				eventLoopAffinity.invoke(delay, callback, args)
		}
		else // If no target eventloop was provided, ...
		{
			if delay > 0.0:
				// Call _enqueue again with 0 delay after 'delay' seconds
				//TODO: Consider running a dedicated event loop instead of eventloops[-1] for delays
				if currentEventLoop == threads[-1]:
					threads[-1]._post(delay, threads[-1]._enqueue, (0.0, callback, args))
				else:
					threads[-1]._invoke(delay, threads[-1]._enqueue, (0.0, callback, args))
			else: // If delay == 0, ...
				// Place the callback on the event queue
				this->event_queue.put((callback, args))

				// Wake up an idle event (if any)
				for eventloop in threads:
				{
					if (eventloop._idle)
					{
						eventloop._idle = false;
						eventloop._invoke(0, eventloop._dequeue, ());
						break;
					}
				}
		}
	}
};*/

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

void timerThreadFunc(int tid)
{
	cout << "Hello World! Thread ID, " << tid << endl;
	pthread_exit(NULL);
}

void thread1(EventLoop* eventLoop)
{
	eventLoop->post([]() { cout << "foo\n"; }, 1.0f);
	auto foo = bind(&EventLoop::close, eventLoop);
	eventLoop->post(foo, 2.5f);
	eventLoop->run();
	pthread_exit(NULL);
}

void thread2(EventLoop* eventLoop)
{
	
	pthread_exit(NULL);
}

int main()
{
	//thread timerThread(timerThreadFunc, 1);
	//timerThread.join();

	/*mutex cv_m;
	unique_lock<mutex> lk(cv_m);
	condition_variable cv;
	cv.wait_for(lk, chrono::seconds(3), []{return false;});
	cout << "done\n";*/

	EventLoop eventloops[2];
	thread threads[] = { thread(thread1, &eventloops[0]), thread(thread2, &eventloops[1]) };
	threads[0].join();
	threads[1].join();
}