#include "discord-rest-queue.hpp"
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using Limits = DiscordRateLimits;
using namespace std::chrono_literals;

bool expect(bool value, const char* name)
{
	if (!value) std::cerr << "FAIL: " << name << '\n';
	return value;
}

int main()
{
	bool ok = true;
	const auto now = Limits::Clock::now();
	const auto a = Limits::route("POST", "/channels/123/messages");
	const auto b = Limits::route("POST", "/channels/456/messages");
	const auto edit = Limits::route("PATCH", "/channels/123/messages/789");
	ok &= expect(edit.key == Limits::route("PATCH", "/channels/123/messages/999?x=1").key, "normalize message IDs and query strings");
	ok &= expect(a.key != Limits::route("GET", "/channels/123/messages").key, "distinguish HTTP methods");
	ok &= expect(Limits::seconds("1.25") == 1.25 && Limits::seconds("0.0001") == 0.0001, "fractional reset seconds");
	for (const auto* bad : {"nan", "inf", "-1", "1oops", "1e300", ""})
		ok &= expect(Limits::seconds(bad) < 0, "invalid retry duration rejected");
	ok &= expect(Limits::deadline(now, 0.0001) - now == 251ms, "round up fractional milliseconds and add buffer");
	Limits limits;
	limits.observe(edit, {{"x-ratelimit-bucket", "shared"}}, false, 0, false, now);
	limits.observe(b, {{"x-ratelimit-bucket", "shared"}}, false, 0, false, now);
	limits.observe(a, {{"x-ratelimit-bucket", "shared"}, {"x-ratelimit-remaining", "0"}, {"x-ratelimit-reset-after", "1.25"}}, false, 0, false, now);
	ok &= expect(limits.blockedUntil(a) == now + 1500ms, "successful response proactively blocks exhausted bucket");
	ok &= expect(limits.blockedUntil(edit) == now + 1500ms, "share deadline across known routes in same bucket");
	ok &= expect(limits.blockedUntil(b) <= now, "another channel remains ready with same bucket hash");
	limits.observe(a, {}, true, 3, false, now);
	ok &= expect(limits.blockedUntil(a) == now + 3250ms, "429 extends bucket deadline");
	limits.observe(a, {}, true, 5, true, now);
	ok &= expect(limits.blockedUntil(b) == now + 5250ms, "global 429 blocks other channels");
	const auto webhookA = Limits::route("POST", "/webhooks/123/tokenA");
	const auto webhookB = Limits::route("POST", "/webhooks/123/tokenB");
	Limits webhooks;
	webhooks.observe(webhookA, {}, true, 2, false, now);
	ok &= expect(webhooks.blockedUntil(webhookB) <= now, "webhook token participates in major resource");
	ok &= expect(webhookA.key.find("tokenA") == std::string::npos, "logged route omits webhook token");

	DiscordRestQueue capacity;
	for (size_t i = 0; i < 8192; ++i)
		ok &= expect(capacity.push([](unsigned&, Limits::Time) {}), "accept all 8192 entries");
	int warnings = 0;
	ok &= expect(!capacity.push([](unsigned&, Limits::Time) {}, [&] { ++warnings; }), "reject entry 8193");
	capacity.push([](unsigned&, Limits::Time) {}, [&] { ++warnings; });
	ok &= expect(warnings == 1, "queue overflow warning is throttled");
	capacity.stop();
	capacity.reset();
	ok &= expect(capacity.push([](unsigned&, Limits::Time) {}), "reset clears backlog");

	// A blocked channel must not delay B. Once A is ready, A1 precedes A2;
	// callbacks for completed tasks must not execute again on subsequent passes.
	DiscordRestQueue queue;
	Limits simulated;
	simulated.observe(a, {}, true, 0, false, Limits::Clock::now());
	std::vector<int> sent;
	std::promise<void> finished;
	auto done = finished.get_future();
	int errors = 0;
	auto task = [&](int id, Limits::Route route)
	{
		return [&, id, route](unsigned&, Limits::Time eligibleAt)
		{
			const auto until = simulated.blockedUntil(route);
			if (until > eligibleAt) throw DiscordRestDeferred(until);
			sent.push_back(id);
			if (sent.size() == 3) { finished.set_value(); queue.stop(); }
		};
	};
	queue.push(task(1, a));
	queue.push(task(2, a));
	queue.push(task(3, b));
	std::thread worker([&] { queue.run([&](const std::exception&) { ++errors; }); });
	const bool completed = done.wait_for(3s) == std::future_status::ready;
	queue.stop();
	worker.join();
	ok &= expect(completed && errors == 0 && sent == std::vector<int>({3, 1, 2}), "bypass blocked bucket, preserve FIFO, complete once");

	// Work submitted during a pass must not get stuck behind a lost wakeup.
	DiscordRestQueue arrival;
	std::promise<void> arrived;
	auto arrivalDone = arrived.get_future();
	arrival.push([&](unsigned&, Limits::Time)
	{
		arrival.push([&](unsigned&, Limits::Time) { arrived.set_value(); arrival.stop(); });
	});
	std::thread arrivalWorker([&] { arrival.run([](const std::exception&) {}); });
	const bool arrivalCompleted = arrivalDone.wait_for(3s) == std::future_status::ready;
	arrival.stop();
	arrivalWorker.join();
	ok &= expect(arrivalCompleted, "new work wakes worker even when submitted during a pass");

	// Shutdown must interrupt a long rate-limit wait.
	DiscordRestQueue stopping;
	std::promise<void> deferred;
	auto deferredDone = deferred.get_future();
	bool notified = false;
	stopping.push([&](unsigned&, Limits::Time time)
	{
		if (!notified) { notified = true; deferred.set_value(); }
		throw DiscordRestDeferred(time + 30min);
	});
	std::thread stoppingWorker([&] { stopping.run([](const std::exception&) {}); });
	ok &= expect(deferredDone.wait_for(3s) == std::future_status::ready, "worker reached rate-limit wait");
	const auto stopAt = Limits::Clock::now();
	stopping.stop();
	stoppingWorker.join();
	ok &= expect(Limits::Clock::now() - stopAt < 1s, "stop interrupts a 30-minute wait");
	if (ok) std::cout << "REST queue and rate-limit regression tests passed\n";
	return ok ? 0 : 1;
}
