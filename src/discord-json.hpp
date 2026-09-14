/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

// Discord payloads are parsed with the bundled nlohmann/json instead of
// substring searches.  Discord's payloads are nested and may contain escaped strings,
// arrays, nulls, and fields added over time.
#include <json.hpp>

#include <string>

using DiscordJson = nlohmann::json;

// nlohmann's json::value(key, fallback) only uses the fallback when the key is
// missing: a key that is present but null (Discord sends null for many fields,
// such as emoji.id or the gateway "t" field) throws type_error.  These helpers
// return the fallback for missing, null or wrongly typed values instead.
inline std::string jsonString(const DiscordJson& object, const char* key, const std::string& fallback = std::string())
{
	if (!object.is_object()) return fallback;
	const auto it = object.find(key);
	return it != object.end() && it->is_string() ? it->get<std::string>() : fallback;
}

inline int jsonInt(const DiscordJson& object, const char* key, int fallback = 0)
{
	if (!object.is_object()) return fallback;
	const auto it = object.find(key);
	return it != object.end() && it->is_number_integer() ? it->get<int>() : fallback;
}

inline bool jsonBool(const DiscordJson& object, const char* key, bool fallback = false)
{
	if (!object.is_object()) return fallback;
	const auto it = object.find(key);
	return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
}
