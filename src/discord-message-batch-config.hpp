#pragma once

#include <charconv>
#include <string>

namespace DiscordMessageBatchConfig
{
constexpr int DEFAULT_INTERVAL_MS = 5000;

inline int interval(const std::string& value)
{
	int result = 0;
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
	return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && result > 0
		? result : DEFAULT_INTERVAL_MS;
}

inline bool enabled(const std::string& value)
{
	return value == "1" || value == "true";
}
}
