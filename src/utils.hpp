/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <string>
#include <cstdint>

namespace DiscordUtils
{
	std::string escapeJson(const std::string& str);
	std::string unescapeJson(const std::string& str);
	std::string urlEncode(const std::string& str);

	std::string extractJsonString(const std::string& json, const std::string& key);
	int64_t extractJsonInt(const std::string& json, const std::string& key);
	bool extractJsonBool(const std::string& json, const std::string& key);

	std::string buildJsonObject(const std::string& content);
	std::string buildJsonPair(const std::string& key, const std::string& value);
	std::string buildJsonPair(const std::string& key, int64_t value);
	std::string buildJsonPair(const std::string& key, bool value);

	std::string snowflakeToString(uint64_t snowflake);
	uint64_t stringToSnowflake(const std::string& str);

	uint64_t getCurrentTimestamp();
	uint64_t parseDiscordTimestamp(const std::string& timestamp);
}
