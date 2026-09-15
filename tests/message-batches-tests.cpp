#include "discord-message-batch-config.hpp"
#include "discord-rest-queue.hpp"
#include <future>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

using Limits = DiscordRateLimits;
using namespace std::chrono_literals;

bool expect(bool value, const char* name)
{
	if (!value) std::cerr << "FAIL: " << name << '\n';
	return value;
}

struct Scenario
{
	DiscordRestQueue queue;
	std::vector<std::pair<std::string, std::string>> sent;
	std::vector<Limits::Time> sentAt;
	size_t expected;
	int errors = 0;
	explicit Scenario(size_t count) : expected(count) {}
	void record(const std::string& channel, const std::string& text)
	{
		sent.emplace_back(channel, text);
		sentAt.push_back(Limits::Clock::now());
		if (sent.size() == expected) queue.stop();
	}
	DiscordRestQueue::MessageTask sender(std::string channel, Limits::Time blocked = {})
	{
		return [this, channel, blocked](const std::string& text, unsigned&, Limits::Time eligibleAt)
		{
			if (blocked > eligibleAt) throw DiscordRestDeferred(blocked);
			record(channel, text);
		};
	}
	bool run()
	{
		std::promise<void> finished;
		auto done = finished.get_future();
		std::thread worker([&]
		{
			queue.run([&](const std::exception&) { ++errors; });
			finished.set_value();
		});
		const bool completed = done.wait_for(3s) == std::future_status::ready;
		queue.stop();
		worker.join();
		return completed;
	}
};

int main()
{
	bool ok = true;
	for (const auto* value : {"", "0", "-1", "5000ms", "999999999999999999999", "abc"})
		ok &= expect(DiscordMessageBatchConfig::interval(value) == 5000, "invalid interval defaults to five seconds");
	ok &= expect(DiscordMessageBatchConfig::interval("1000") == 1000, "custom interval");
	for (const auto* value : {"", "0", "false", "5000", "invalid"})
		ok &= expect(!DiscordMessageBatchConfig::enabled(value), "only explicit opt-in enables batching");
	ok &= expect(DiscordMessageBatchConfig::enabled("true") && DiscordMessageBatchConfig::enabled("1"), "environment and SA-MP opt-in");

	{
		Scenario s(2);
		// An enabled five-second window must not delay unrestricted chat.
		s.queue.pushMessage("a", "first", 5000, s.sender("a"));
		s.queue.pushMessage("a", "second", 5000, s.sender("a"));
		ok &= expect(s.run() && s.errors == 0 && s.sent[0].second == "first" && s.sent[1].second == "second",
			"enabled but unrestricted chat sends immediately and separately");
	}
	{
		Scenario s(2);
		const auto blocked = Limits::Clock::now() + 40ms;
		s.queue.pushMessage("a", "first", 0, s.sender("a", blocked));
		s.queue.pushMessage("a", "second", 0, s.sender("a", blocked));
		ok &= expect(s.run() && s.sent.size() == 2 && s.sent[0].second == "first" && s.sent[1].second == "second",
			"disabled option preserves individual rate-limited messages");
	}
	{
		Scenario s(3);
		const auto started = Limits::Clock::now();
		bool deferred = false;
		s.queue.pushMessage("a", "first", 80, [&](const std::string& text, unsigned&, Limits::Time)
		{
			if (!deferred) { deferred = true; throw DiscordRestDeferred(started + 10ms); }
			s.record("a", text);
			// After draining a batch, a new send uses the immediate path again.
			s.queue.pushMessage("a", "fresh", 5000, s.sender("a"));
		});
		s.queue.pushMessage("b", "other channel", 80, [&](const std::string& text, unsigned&, Limits::Time)
		{
			s.record("b", text);
			s.queue.pushMessage("a", "late arrival", 80, s.sender("a"));
		});
		s.queue.pushMessage("a", "second", 80, s.sender("a"));
		ok &= expect(s.run() && s.errors == 0 && s.sent.size() == 3
			&& s.sent[0].first == "b" && s.sent[1].second == "first\nsecond\nlate arrival"
			&& s.sent[2].second == "fresh" && s.sentAt[1] >= started + 80ms,
			"batch only blocked channel, include new arrivals, respect window, then resume immediate chat");
	}
	{
		Scenario s(2);
		const auto globalDeadline = Limits::Clock::now() + 90ms;
		for (const auto* channel : {"a", "b"})
		{
			s.queue.pushMessage(channel, "first", 20, s.sender(channel, globalDeadline));
			s.queue.pushMessage(channel, "second", 20, s.sender(channel, globalDeadline));
		}
		ok &= expect(s.run() && s.sent.size() == 2 && s.sent[0].first != s.sent[1].first
			&& s.sent[0].second == "first\nsecond" && s.sent[1].second == "first\nsecond"
			&& s.sentAt[0] >= globalDeadline && s.sentAt[1] >= globalDeadline,
			"longer global retry deadline wins and channel batches stay separate");
	}
	{
		Scenario s(3);
		const std::string emoji = "\xF0\x9F\x98\x80";
		bool deferred = false;
		s.queue.pushMessage("a", std::string(1998, 'x'), 30, [&](const std::string& text, unsigned&, Limits::Time now)
		{
			if (!deferred) { deferred = true; throw DiscordRestDeferred(now + 1ms); }
			s.record("a", text);
		});
		for (const auto& text : {std::string("y"), emoji, std::string("last"), std::string(1999, 'z')})
			s.queue.pushMessage("a", text, 30, s.sender("a"));
		ok &= expect(s.run() && s.sent.size() == 3 && s.sent[0].second == std::string(1998, 'x') + "\ny"
			&& s.sent[1].second == emoji + "\nlast" && s.sent[2].second == std::string(1999, 'z'),
			"size-limited batches preserve UTF-8, whole messages and order across chunks");
	}
	{
		Scenario s(3);
		bool deferred = false;
		int callbacks = 0;
		s.queue.pushMessage("a", "before callback", 40, [&](const std::string& text, unsigned&, Limits::Time now)
		{
			if (!deferred) { deferred = true; throw DiscordRestDeferred(now + 1ms); }
			s.record("a", text);
		});
		s.queue.pushMessage("a", "callback", 0, [&](const std::string& text, unsigned&, Limits::Time)
		{
			++callbacks;
			s.record("a", text);
		});
		s.queue.pushMessage("a", "after callback", 40, s.sender("a"));
		ok &= expect(s.run() && s.sent.size() == 3 && callbacks == 1 && s.sent[0].second == "before callback"
			&& s.sent[1].second == "callback" && s.sent[2].second == "after callback",
			"callback sends are separate and retain channel order");
	}
	{
		Scenario s(2);
		bool deferred = false;
		int operations = 0;
		s.queue.pushMessage("a", "before operation", 30, [&](const std::string& text, unsigned&, Limits::Time now)
		{
			if (!deferred) { deferred = true; throw DiscordRestDeferred(now + 1ms); }
			s.record("a", text);
		});
		s.queue.push([&](unsigned&, Limits::Time) { ++operations; });
		s.queue.pushMessage("a", "after operation", 30, s.sender("a"));
		ok &= expect(s.run() && operations == 1 && s.sent.size() == 2
			&& s.sent[0].second == "before operation" && s.sent[1].second == "after operation",
			"opaque REST operations separate batches without reordering plain chat");
	}
	{
		Scenario s(1);
		int attempts = 0;
		Limits::Time extended;
		s.queue.pushMessage("a", "first", 20, [&](const std::string& text, unsigned& retries, Limits::Time now)
		{
			++attempts;
			if (++retries < 3)
			{
				extended = now + 40ms;
				throw DiscordRestDeferred(extended);
			}
			s.record("a", text);
		});
		s.queue.pushMessage("a", "second", 20, s.sender("a"));
		ok &= expect(s.run() && attempts == 3 && s.sent.size() == 1
			&& s.sent[0].second == "first\nsecond" && s.sentAt[0] >= extended,
			"repeat 429 extends deadline without duplicating or losing batch lines");
	}
	{
		Scenario s(1);
		int failedAttempts = 0;
		s.queue.pushMessage("a", "ambiguous failure", 20, [&](const std::string&, unsigned&, Limits::Time)
		{
			++failedAttempts;
			throw std::runtime_error("simulated transport failure");
		});
		s.queue.pushMessage("a", "next", 20, s.sender("a"));
		ok &= expect(s.run() && failedAttempts == 1 && s.errors == 1 && s.sent[0].second == "next",
			"ordinary failures are not batched or replayed");
	}
	{
		Scenario s(0);
		bool rejected = false;
		for (size_t i = 0; i < DiscordRestQueue::CAPACITY - 1; ++i)
			ok &= s.queue.pushMessage("a", "x", 20, [](const std::string&, unsigned&, Limits::Time now)
			{
				throw DiscordRestDeferred(now + 30min);
			});
		s.queue.push([&](unsigned&, Limits::Time)
		{
			rejected = !s.queue.pushMessage("a", "overflow", 20, s.sender("a"));
			s.queue.stop();
		});
		ok &= expect(s.run() && rejected && s.sent.empty(), "merged lines still count toward capacity and shutdown discards them");
		s.queue.reset();
		s.expected = 1;
		s.queue.pushMessage("a", "fresh after reset", 5000, s.sender("a"));
		ok &= expect(s.run() && s.sent.size() == 1 && s.sent[0].second == "fresh after reset", "reset releases batch capacity and deadlines");
	}
	{
		Scenario s(0);
		ok &= expect(!s.queue.pushMessage("", "text", 20, s.sender("a"))
			&& !s.queue.pushMessage("a", "", 20, s.sender("a"))
			&& !s.queue.pushMessage("a", std::string(2001, 'x'), 20, s.sender("a"))
			&& !s.queue.pushMessage("a", "text", -1, s.sender("a")), "invalid message input is rejected");
	}
	if (ok) std::cout << "Rate-limited message batching tests passed\n";
	return ok ? 0 : 1;
}
