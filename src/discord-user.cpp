/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-user.hpp"
#include "discord-json.hpp"

DiscordUser::DiscordUser(StringView id, StringView username, StringView discriminator, bool isBot)
	: userId_(id.data(), id.length())
	, username_(username.data(), username.length())
	, discriminator_(discriminator.data(), discriminator.length())
	, isBot_(isBot)
	, verified_(false)
{
}

void DiscordUser::updateFromJson(const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		return;
	}
	if ((data.find("id") != data.end()) && data["id"].is_string()) userId_ = data["id"].get<std::string>();
	if ((data.find("username") != data.end()) && data["username"].is_string()) username_ = data["username"].get<std::string>();
	if ((data.find("global_name") != data.end()) && (data["global_name"].is_string() || data["global_name"].is_null())) globalName_ = data["global_name"].is_string() ? data["global_name"].get<std::string>() : std::string();
	if ((data.find("discriminator") != data.end()) && data["discriminator"].is_string()) discriminator_ = data["discriminator"].get<std::string>();
	if ((data.find("bot") != data.end()) && data["bot"].is_boolean()) isBot_ = data["bot"].get<bool>();
	if ((data.find("verified") != data.end()) && data["verified"].is_boolean()) verified_ = data["verified"].get<bool>();
	if ((data.find("system") != data.end()) && data["system"].is_boolean()) system_ = data["system"].get<bool>();
	if ((data.find("avatar") != data.end()) && (data["avatar"].is_string() || data["avatar"].is_null())) avatarHash_ = data["avatar"].is_string() ? data["avatar"].get<std::string>() : std::string();
	if ((data.find("banner") != data.end()) && (data["banner"].is_string() || data["banner"].is_null())) bannerHash_ = data["banner"].is_string() ? data["banner"].get<std::string>() : std::string();
}
