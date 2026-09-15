#pragma once

#include "discord-rate-limit.hpp"
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <stdexcept>

// Deferred entries keep their position. New work can bypass a blocked bucket,
// but an older entry is always considered first when its bucket becomes ready.
class DiscordRestQueue
{
public:
	static constexpr size_t CAPACITY = 8192;
	using Task = std::function<void(unsigned&, DiscordRateLimits::Time)>;
	bool push(Task task, const std::function<void()>& onFull = {})
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stopped_) return false;
		if (tasks_.size() >= CAPACITY)
		{
			if (!overflowLogged_ && onFull) onFull();
			overflowLogged_ = true;
			return false;
		}
		if (tasks_.size() < CAPACITY / 2) overflowLogged_ = false;
		tasks_.push_back({std::move(task), 0});
		++generation_;
		condition_.notify_one();
		return true;
	}
	void stop()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopped_ = true;
		condition_.notify_all();
	}
	void reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		tasks_.clear();
		stopped_ = false;
		overflowLogged_ = false;
	}
	void run(const std::function<void(const std::exception&)>& onError)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		while (!stopped_)
		{
			auto wake = DiscordRateLimits::Time::max();
			const auto eligibleAt = DiscordRateLimits::Clock::now();
			const auto generation = generation_;
			size_t remaining = tasks_.size();
			for (auto it = tasks_.begin(); it != tasks_.end() && !stopped_ && remaining-- > 0;)
			{
				bool deferred = false;
				lock.unlock();
				try { it->task(it->retries, eligibleAt); }
				catch (const DiscordRestDeferred& delay)
				{
					deferred = true;
					wake = std::min(wake, delay.until);
				}
				catch (const std::exception& error) { onError(error); }
				catch (...) { const std::runtime_error error("unknown REST task exception"); onError(error); }
				lock.lock();
				if (deferred) ++it;
				else it = tasks_.erase(it);
			}
			if (!stopped_ && generation == generation_) condition_.wait_until(lock, wake);
		}
		tasks_.clear();
	}
private:
	struct Entry { Task task; unsigned retries; };
	std::list<Entry> tasks_;
	std::mutex mutex_;
	std::condition_variable condition_;
	bool stopped_ = false;
	bool overflowLogged_ = false;
	size_t generation_ = 0;
};
