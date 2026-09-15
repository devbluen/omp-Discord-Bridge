#pragma once

#include "discord-rate-limit.hpp"
#include <condition_variable>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>

// Deferred entries keep their position. New work can bypass a blocked bucket,
// but an older entry is always considered first when its bucket becomes ready.
class DiscordRestQueue
{
public:
	static constexpr size_t CAPACITY = 8192;
	using Task = std::function<void(unsigned&, DiscordRateLimits::Time)>;
	using MessageTask = std::function<void(const std::string&, unsigned&, DiscordRateLimits::Time)>;
	bool push(Task task, const std::function<void()>& onFull = {})
	{
		if (!task) return false;
		return enqueue(Entry{std::move(task)}, onFull);
	}
	// A positive interval opts a plain text send into batching after deferral.
	// Zero keeps a send (including its callback) separate, in channel order.
	bool pushMessage(const std::string& channel, const std::string& text, int intervalMs,
		MessageTask task, const std::function<void()>& onFull = {})
	{
		if (!task || channel.empty() || text.empty() || text.size() > 2000 || intervalMs < 0) return false;
		auto message = std::make_shared<Message>(Message{channel, text, intervalMs});
		Entry entry {[message, task = std::move(task)](unsigned& retries, DiscordRateLimits::Time eligibleAt)
		{
			task(message->text, retries, eligibleAt);
		}};
		entry.message = std::move(message);
		return enqueue(std::move(entry), onFull);
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
		pending_ = 0;
		batchGroup_ = 0;
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
			std::unordered_map<std::string, DiscordRateLimits::Time> channelWaits;
			size_t remaining = tasks_.size();
			for (auto it = tasks_.begin(); it != tasks_.end() && !stopped_ && remaining-- > 0;)
			{
				if (it->message)
				{
					const auto prior = channelWaits.find(it->message->channel);
					if (prior != channelWaits.end())
					{
						it->until = std::max(it->until, prior->second);
						if (it->message->intervalMs > 0) it->batching = true;
					}
					if (it->batching) mergeMessages(it);
					if (it->until > eligibleAt)
					{
						channelWaits[it->message->channel] = it->until;
						wake = std::min(wake, it->until);
						++it;
						continue;
					}
				}
				bool deferred = false;
				lock.unlock();
				try { it->task(it->retries, eligibleAt); }
				catch (const DiscordRestDeferred& delay)
				{
					deferred = true;
					it->until = delay.until;
					if (it->message && it->message->intervalMs > 0 && !it->batching)
					{
						it->batching = true;
						it->until = std::max(it->until, DiscordRateLimits::Clock::now()
							+ std::chrono::milliseconds(it->message->intervalMs));
					}
					wake = std::min(wake, it->until);
				}
				catch (const std::exception& error) { onError(error); }
				catch (...) { const std::runtime_error error("unknown REST task exception"); onError(error); }
				lock.lock();
				if (deferred)
				{
					if (it->message)
					{
						if (it->batching) mergeMessages(it);
						channelWaits[it->message->channel] = it->until;
						wake = std::min(wake, it->until);
					}
					++it;
				}
				else
				{
					pending_ -= it->count;
					it = tasks_.erase(it);
				}
			}
			if (!stopped_ && generation == generation_) condition_.wait_until(lock, wake);
		}
		tasks_.clear();
		pending_ = 0;
	}
private:
	struct Message { std::string channel; std::string text; int intervalMs; };
	struct Entry
	{
		Task task;
		unsigned retries = 0;
		std::shared_ptr<Message> message;
		DiscordRateLimits::Time until {};
		bool batching = false;
		size_t count = 1;
		size_t batchGroup = 0;
		explicit Entry(Task value) : task(std::move(value)) {}
	};
	bool enqueue(Entry entry, const std::function<void()>& onFull)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stopped_) return false;
		if (pending_ >= CAPACITY)
		{
			if (!overflowLogged_ && onFull) onFull();
			overflowLogged_ = true;
			return false;
		}
		if (pending_ < CAPACITY / 2) overflowLogged_ = false;
		// Keep barriers even after the intervening task has finished.
		if (!entry.message || entry.message->intervalMs == 0) ++batchGroup_;
		entry.batchGroup = batchGroup_;
		tasks_.push_back(std::move(entry));
		++pending_;
		++generation_;
		condition_.notify_one();
		return true;
	}
	// Called with the queue locked. Only the worker changes message text, and
	// never while a request is in flight. Opaque REST tasks and callback sends
	// are barriers; another channel's plain chat can be skipped safely.
	void mergeMessages(std::list<Entry>::iterator first)
	{
		for (auto next = std::next(first); next != tasks_.end();)
		{
			if (next->batchGroup != first->batchGroup) break;
			if (!next->message || next->message->intervalMs == 0) break;
			if (next->message->channel != first->message->channel) { ++next; continue; }
			if (next->message->intervalMs != first->message->intervalMs
				|| first->message->text.size() + 1 + next->message->text.size() > 2000) break;
			first->message->text += '\n';
			first->message->text += next->message->text;
			first->count += next->count;
			first->retries = std::max(first->retries, next->retries);
			first->until = std::max(first->until, next->until);
			next = tasks_.erase(next);
		}
	}
	std::list<Entry> tasks_;
	std::mutex mutex_;
	std::condition_variable condition_;
	bool stopped_ = false;
	bool overflowLogged_ = false;
	size_t generation_ = 0;
	size_t pending_ = 0;
	size_t batchGroup_ = 0;
};
