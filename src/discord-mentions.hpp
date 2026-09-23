/*
 * Discord Bridge for open.mp
 * Copyright (c) 2026 Neufox
 */
#pragma once

#include "discord-json.hpp"
#include <string>
#include <vector>

namespace DiscordMentions
{
// Text mentions in string command options, in encounter order. Repeated
// mentions remain repeated, matching the legacy count/index contract.
inline void collect(const DiscordJson& options, std::vector<std::string>& users)
{
	if (!options.is_array()) return;
	for (const auto& option : options)
	{
		if (!option.is_object()) continue;
		const auto type = option.find("type");
		if (type == option.end() || !type->is_number_integer()) continue;
		if (*type == 1 || *type == 2)
		{
			const auto nested = option.find("options");
			if (nested != option.end()) collect(*nested, users);
			continue;
		}
		const auto value = option.find("value");
		if (*type != 3 || value == option.end() || !value->is_string()) continue;
		const auto& text = value->get_ref<const std::string&>();
		for (size_t start = 0; (start = text.find("<@", start)) != std::string::npos;)
		{
			start += 2;
			size_t cursor = start;
			if (cursor < text.size() && text[cursor] == '!') ++cursor;
			const auto whitespace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; };
			while (cursor < text.size() && whitespace(text[cursor])) ++cursor;
			const size_t digits = cursor;
			while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') ++cursor;
			const std::string id = text.substr(digits, cursor - digits);
			while (cursor < text.size() && whitespace(text[cursor])) ++cursor;
			if (cursor == text.size() || text[cursor] != '>' || id.empty() || id.front() == '0'
				|| id.size() > 20 || (id.size() == 20 && id > "18446744073709551615")) continue;
			users.push_back(id);
			start = cursor + 1;
		}
	}
}
}
