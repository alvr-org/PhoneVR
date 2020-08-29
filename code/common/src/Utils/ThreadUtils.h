#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

typedef std::chrono::high_resolution_clock Clk; //global


inline void EndThread(std::thread *&thr) // get reference to pointer to be able to assign nullptr to thr
{
	if (thr)
	{
        thr->join();
		delete thr;
		thr = nullptr;
	}
}


template <typename T>
class SafeQueue
{
	std::queue<T> q;
	std::mutex mtx;
	std::condition_variable cond;

public:
	T& front()
	{
		std::unique_lock<std::mutex> mlock(mtx);
		while (q.empty())
			cond.wait(mlock);
		return q.front();
	}

	void pop_front()
	{
		std::unique_lock<std::mutex> mlock(mtx);
		while (q.empty())
			cond.wait(mlock);
		q.pop();
	}

	void push_back(const T& item)
	{
		std::unique_lock<std::mutex> mlock(mtx);
		q.push(item);
		mlock.unlock();     // unlock before notificiation to minimize mutex con
		cond.notify_one(); // notify one waiting thread
	}

	int size()
	{
		std::unique_lock<std::mutex> mlock(mtx);
		int size = q.size();
		mlock.unlock();
		return size;
	}
};


class TimeBomb
{
	std::function<void()> cb;
	std::chrono::microseconds tm;
	std::thread *thr = nullptr;
	std::condition_variable cond;
	std::mutex mtx;
	bool snoozed;
	bool loop;

public:
	void ignite(bool async = true)
	{
		EndThread(thr);
		thr = new std::thread([=]
		{
			do
			{
				snoozed = false;
				std::unique_lock<std::mutex> mlock(mtx);
				cond.wait_for(mlock, tm);
				if (!snoozed)
				{
					cb();
					loop = false;
				}
			} while (loop);
		});

		if (!async)
			EndThread(thr);
	}

	TimeBomb(std::chrono::microseconds timer, std::function<void()> boomCb = [] {}) : tm(timer), cb(boomCb), loop(true) {}

	void defuse()
	{
		loop = false;
		snoozed = true;
		cond.notify_all();
	}

	//async flag is read only on the first call of reset
	void reset(bool async = true)
	{
		loop = true;
		snoozed = true;
		cond.notify_all();
		if (!thr)
			ignite(async);
	}

	void explode()
	{
		cond.notify_all();
	}

	~TimeBomb()
	{
		defuse();
		EndThread(thr);
	}
};


class RefWhistle
{
	Clk::time_point tp;                  //last loop time point
	std::chrono::microseconds lt, pert;  //loop time, current loop perturbation
	std::condition_variable cond;
	std::mutex mtx;

public:
	RefWhistle(std::chrono::microseconds loopTime) : lt(loopTime) {
        tp = Clk::now();
        pert = std::chrono::microseconds::zero();
	}

	void wait()
	{
		std::unique_lock<std::mutex> mlock(mtx);
		while (Clk::now() < tp + lt + pert)
			cond.wait_until(mlock, tp + lt + pert);
		tp = Clk::now();
		pert = std::chrono::microseconds::zero();
	}

	// makes the current loop longer or shorter
	void perturbation(std::chrono::microseconds dt)
	{
		pert = dt;
		cond.notify_one();
	}

	void unblockNow()
	{
		perturbation(-lt);
	}
};