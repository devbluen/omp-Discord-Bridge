#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <string>
#include <unordered_map>
#include <vector>

struct DiscordRestDeferred : std::exception
{
	std::chrono::steady_clock::time_point until;
	explicit DiscordRestDeferred(std::chrono::steady_clock::time_point value) : until(value) {}
	const char* what() const noexcept override { return "Discord REST request deferred"; }
};

// Owned by the REST worker. Bucket hashes exclude major resources, so retain
// channel/guild/webhook identifiers when sharing a deadline between routes.
class DiscordRateLimits
{
public:
	using Clock = std::chrono::steady_clock;
	using Time = Clock::time_point;
	struct Route { std::string key; std::string major; };
	static Route route(const std::string& method, std::string path)
	{
		path.erase(path.find('?') == std::string::npos ? path.size() : path.find('?'));
		std::vector<std::string> parts;
		for (size_t start = 0; start < path.size();)
		{
			if (path[start] == '/') { ++start; continue; }
			auto end = path.find('/', start);
			if (end == std::string::npos) end = path.size();
			parts.push_back(path.substr(start, end - start));
			start = end;
		}
		Route result { method, {} };
		for (size_t i = 0; i < parts.size(); ++i)
		{
			const bool major = i == 1 && (parts[0] == "channels" || parts[0] == "guilds" || parts[0] == "webhooks");
			const bool webhookToken = i == 2 && parts[0] == "webhooks" && parts[i] != "messages";
			if (major) result.major = "/" + parts[0] + "/" + parts[i];
			if (webhookToken) result.major += "/" + parts[i];
			const bool numeric = !parts[i].empty() && parts[i].find_first_not_of("0123456789") == std::string::npos;
			const bool interactionToken = i == 2 && parts[0] == "interactions";
			const bool reaction = i > 0 && parts[i - 1] == "reactions";
			result.key += "/" + ((numeric && !major) || webhookToken || interactionToken || reaction ? ":id" : parts[i]);
		}
		return result;
	}
	static double seconds(const std::string& value)
	{
		try
		{
			size_t end = 0;
			const double number = std::stod(value, &end);
			return end == value.size() && std::isfinite(number) && number >= 0 && number <= 31536000 ? number : -1;
		}
		catch (...) { return -1; }
	}
	static Time deadline(Time now, double seconds)
	{
		return now + std::chrono::milliseconds(static_cast<long long>(std::ceil(seconds * 1000)) + 250);
	}
	Time blockedUntil(const Route& route) const
	{
		Time until = global_;
		const auto local = blocked_.find(route.key + route.major);
		if (local != blocked_.end()) until = std::max(until, local->second);
		const auto mapping = buckets_.find(route.key);
		if (mapping != buckets_.end())
		{
			const auto bucket = blocked_.find(mapping->second + route.major);
			if (bucket != blocked_.end()) until = std::max(until, bucket->second);
		}
		return until;
	}
	void observe(const Route& route, const std::unordered_map<std::string, std::string>& headers,
		bool limited, double retryAfter, bool global, Time now)
	{
		const auto bucket = headers.find("x-ratelimit-bucket");
		if (bucket != headers.end()) buckets_[route.key] = "bucket:" + bucket->second;
		double wait = -1;
		const auto remaining = headers.find("x-ratelimit-remaining");
		const auto reset = headers.find("x-ratelimit-reset-after");
		if (remaining != headers.end() && remaining->second == "0" && reset != headers.end()) wait = seconds(reset->second);
		if (limited) wait = std::max(wait, retryAfter >= 0 ? retryAfter : 1.0);
		if (wait < 0) return;
		const auto until = deadline(now, wait);
		if (limited && global) global_ = std::max(global_, until);
		else
		{
			blocked_[route.key + route.major] = std::max(blocked_[route.key + route.major], until);
			const auto mapping = buckets_.find(route.key);
			if (mapping != buckets_.end())
			{
				auto& value = blocked_[mapping->second + route.major];
				value = std::max(value, until);
			}
		}
	}
private:
	Time global_ {};
	std::unordered_map<std::string, std::string> buckets_;
	std::unordered_map<std::string, Time> blocked_;
};
