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
	return DiscordJson { { "content", content } }.dump(-1, ' ', false, DiscordJson::error_handler_t::replace);
}

std::string buildJsonPair(const std::string& key, const std::string& value)
{
	return DiscordJson(key).dump(-1, ' ', false, DiscordJson::error_handler_t::replace) + ":" + DiscordJson(value).dump(-1, ' ', false, DiscordJson::error_handler_t::replace);
}

std::string buildJsonPair(const std::string& key, int64_t value)
{
	return DiscordJson(key).dump(-1, ' ', false, DiscordJson::error_handler_t::replace) + ":" + DiscordJson(value).dump(-1, ' ', false, DiscordJson::error_handler_t::replace);
}

std::string buildJsonPair(const std::string& key, bool value)
{
	return DiscordJson(key).dump(-1, ' ', false, DiscordJson::error_handler_t::replace) + ":" + DiscordJson(value).dump(-1, ' ', false, DiscordJson::error_handler_t::replace);
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

	namespace
	{
		// Unicode code points of Windows-1252 bytes 0x80-0x9F (0 = unassigned).
		constexpr uint32_t kWindows1252High[32] = {
			0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
			0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
		};

		void appendUtf8(std::string& out, uint32_t codePoint)
		{
			if (codePoint < 0x80)
			{
				out.push_back(static_cast<char>(codePoint));
			}
			else if (codePoint < 0x800)
			{
				out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
			else if (codePoint < 0x10000)
			{
				out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
		}

		// Decodes one UTF-8 sequence starting at `index`.  Returns its length, or
		// 0 when the sequence is invalid.
		std::size_t decodeUtf8(const std::string& text, std::size_t index, uint32_t& codePoint)
		{
			const unsigned char lead = static_cast<unsigned char>(text[index]);
			std::size_t extra = 0;
			if (lead < 0x80) { codePoint = lead; return 1; }
			if ((lead & 0xE0) == 0xC0) { extra = 1; codePoint = lead & 0x1F; }
			else if ((lead & 0xF0) == 0xE0) { extra = 2; codePoint = lead & 0x0F; }
			else if ((lead & 0xF8) == 0xF0) { extra = 3; codePoint = lead & 0x07; }
			else return 0;
			if (index + extra >= text.size()) return 0;
			for (std::size_t offset = 1; offset <= extra; ++offset)
			{
				const unsigned char next = static_cast<unsigned char>(text[index + offset]);
				if ((next & 0xC0) != 0x80) return 0;
				codePoint = (codePoint << 6) | (next & 0x3F);
			}
			const bool overlong = (extra == 1 && codePoint < 0x80) || (extra == 2 && codePoint < 0x800) || (extra == 3 && codePoint < 0x10000);
			if (overlong || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) return 0;
			return extra + 1;
		}
	}

	bool isValidUtf8(const std::string& text)
	{
		uint32_t codePoint = 0;
		for (std::size_t index = 0; index < text.size();)
		{
			const std::size_t length = decodeUtf8(text, index, codePoint);
			if (length == 0) return false;
			index += length;
		}
		return true;
	}

	std::string windows1252ToUtf8(const std::string& text)
	{
		std::string out;
		out.reserve(text.size() + text.size() / 4);
		for (const char character : text)
		{
			const unsigned char byte = static_cast<unsigned char>(character);
			if (byte < 0x80) out.push_back(character);
			else if (byte < 0xA0) appendUtf8(out, kWindows1252High[byte - 0x80] ? kWindows1252High[byte - 0x80] : 0xFFFD);
			else appendUtf8(out, byte);
		}
		return out;
	}

	std::string utf8ToWindows1252(const std::string& text)
	{
		std::string out;
		out.reserve(text.size());
		uint32_t codePoint = 0;
		for (std::size_t index = 0; index < text.size();)
		{
			const std::size_t length = decodeUtf8(text, index, codePoint);
			if (length == 0)
			{
				out.push_back('?');
				++index;
				continue;
			}
			index += length;
			if (codePoint < 0x80 || (codePoint >= 0xA0 && codePoint <= 0xFF))
			{
				out.push_back(static_cast<char>(codePoint));
				continue;
			}
			char mapped = '?';
			for (std::size_t slot = 0; slot < 32; ++slot)
			{
				if (kWindows1252High[slot] == codePoint)
				{
					mapped = static_cast<char>(0x80 + slot);
					break;
				}
			}
			out.push_back(mapped);
		}
		return out;
	}
}
