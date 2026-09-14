/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "utils.hpp"
#include "discord-json.hpp"
#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace DiscordUtils
{
std::string escapeJson(const std::string& str)
{
	std::string out;
	out.reserve(str.size());
	for (char c : str)
	{
		switch (c)
		{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default: out += c; break;
		}
	}
	return out;
}

std::string unescapeJson(const std::string& str)
{
	std::string out;
	out.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] == '\\' && i + 1 < str.size())
		{
			++i;
			switch (str[i])
			{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				default: out += str[i]; break;
			}
		}
		else
		{
			out += str[i];
		}
	}
	return out;
}

std::string urlEncode(const std::string& str)
{
	std::ostringstream encoded;
	encoded << std::uppercase << std::hex;

	for (unsigned char c : str)
	{
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			encoded << c;
		}
		else
		{
			encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
		}
	}
	return encoded.str();
}

std::string extractJsonString(const std::string& json, const std::string& key)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object()) return {};
	const auto it = data.find(key);
	return it != data.end() && it->is_string() ? it->get<std::string>() : std::string();
}

int64_t extractJsonInt(const std::string& json, const std::string& key)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object()) return 0;
	const auto it = data.find(key);
	if (it == data.end() || !it->is_number_integer()) return 0;
	try { return it->get<int64_t>(); } catch (...) { return 0; }
}

bool extractJsonBool(const std::string& json, const std::string& key)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object()) return false;
	const auto it = data.find(key);
	return it != data.end() && it->is_boolean() && it->get<bool>();
}

std::string buildJsonObject(const std::string& content)
{
	return DiscordJson { { "content", content } }.dump();
}

std::string buildJsonPair(const std::string& key, const std::string& value)
{
	return DiscordJson(key).dump() + ":" + DiscordJson(value).dump();
}

std::string buildJsonPair(const std::string& key, int64_t value)
{
	return DiscordJson(key).dump() + ":" + DiscordJson(value).dump();
}

std::string buildJsonPair(const std::string& key, bool value)
{
	return DiscordJson(key).dump() + ":" + DiscordJson(value).dump();
}

std::string snowflakeToString(uint64_t snowflake)
{
	return std::to_string(snowflake);
}

uint64_t stringToSnowflake(const std::string& str)
{
	if (str.empty())
	{
		return 0;
	}

	for (char c : str)
	{
		if (!std::isdigit(static_cast<unsigned char>(c)))
		{
			return 0;
		}
	}

	try
	{
		return static_cast<uint64_t>(std::stoull(str));
	}
	catch (...)
	{
		return 0;
	}
}

uint64_t getCurrentTimestamp()
{
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

uint64_t parseDiscordTimestamp(const std::string& timestamp)
{
	if (timestamp.size() < 20 || timestamp[4] != '-' || timestamp[7] != '-'
		|| timestamp[10] != 'T' || timestamp[13] != ':' || timestamp[16] != ':') return 0;
	if (timestamp.back() != 'Z') return 0;
	if (timestamp.size() > 20 && timestamp[19] != '.') return 0;
	if (timestamp.size() > 20)
	{
		for (size_t i = 20; i + 1 < timestamp.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(timestamp[i]))) return 0;
		}
	}

	std::tm tm{};
	std::istringstream parser(timestamp.substr(0, 19));
	parser >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
	if (parser.fail()) return 0;
#if defined(_WIN32)
	const std::time_t epoch = _mkgmtime(&tm);
#else
	const std::time_t epoch = timegm(&tm);
#endif
	return epoch < 0 ? 0 : static_cast<uint64_t>(epoch);
}

	std::string base64Encode(const std::string& data)
	{
		static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string encoded;
		encoded.reserve(((data.size() + 2) / 3) * 4);
		size_t index = 0;
		for (; index + 2 < data.size(); index += 3)
		{
			const uint32_t chunk = (static_cast<uint32_t>(static_cast<unsigned char>(data[index])) << 16) |
				(static_cast<uint32_t>(static_cast<unsigned char>(data[index + 1])) << 8) |
				static_cast<uint32_t>(static_cast<unsigned char>(data[index + 2]));
			encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
			encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
			encoded.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
			encoded.push_back(kAlphabet[chunk & 0x3F]);
		}
		const size_t remaining = data.size() - index;
		if (remaining > 0)
		{
			uint32_t chunk = static_cast<uint32_t>(static_cast<unsigned char>(data[index])) << 16;
			if (remaining == 2) chunk |= static_cast<uint32_t>(static_cast<unsigned char>(data[index + 1])) << 8;
			encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
			encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
			encoded.push_back(remaining == 2 ? kAlphabet[(chunk >> 6) & 0x3F] : '=');
			encoded.push_back('=');
		}
		return encoded;
	}
}
