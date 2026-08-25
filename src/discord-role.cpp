/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-role.hpp"
#include "discord-json.hpp"

DiscordRole::DiscordRole(StringView id, StringView name)
	: roleId_(id.data(), id.length())
	, roleName_(name.data(), name.length())
	, color_(0)
	, permissions_(0)
	, hoisted_(false)
	, position_(0)
	, mentionable_(false)
{
}

void DiscordRole::updateFromJson(const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		return;
	}
	if ((data.find("id") != data.end()) && data["id"].is_string()) roleId_ = data["id"].get<std::string>();
	if ((data.find("name") != data.end()) && data["name"].is_string()) roleName_ = data["name"].get<std::string>();
	if ((data.find("color") != data.end()) && data["color"].is_number_unsigned()) color_ = data["color"].get<uint32_t>();
	if ((data.find("permissions") != data.end()) && data["permissions"].is_string())
	{
		try { permissions_ = std::stoull(data["permissions"].get<std::string>()); } catch (...) { permissions_ = 0; }
	}
	else if ((data.find("permissions") != data.end()) && data["permissions"].is_number_unsigned())
	{
		permissions_ = data["permissions"].get<uint64_t>();
	}
	if ((data.find("hoist") != data.end()) && data["hoist"].is_boolean()) hoisted_ = data["hoist"].get<bool>();
	if ((data.find("position") != data.end()) && data["position"].is_number_integer()) position_ = data["position"].get<int>();
	if ((data.find("mentionable") != data.end()) && data["mentionable"].is_boolean()) mentionable_ = data["mentionable"].get<bool>();
}
