/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-guild.hpp"
#include "discord-bot.hpp"
#include "discord-component.hpp"
#include "discord-http.hpp"
#include "discord-json.hpp"
#include "discord-role.hpp"
#include "utils.hpp"
#include <algorithm>

namespace
{
void readNullableString(const DiscordJson& data, const char* key, std::string& target)
{
	const auto it = data.find(key);
	if (it != data.end() && (it->is_string() || it->is_null())) target = it->is_string() ? it->get<std::string>() : std::string();
}

void readBool(const DiscordJson& data, const char* key, bool& target)
{
	const auto it = data.find(key);
	if (it != data.end() && it->is_boolean()) target = it->get<bool>();
}
}

DiscordGuild::DiscordGuild(DiscordBot* bot, DiscordBridgeComponent* component, StringView id, StringView name)
	: guildId_(id.data(), id.length())
	, guildName_(name.data(), name.length())
	, memberCount_(0)
	, bot_(bot)
	, component_(component)
{
}

bool DiscordGuild::setName(StringView name)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string guildId = guildId_;
	const std::string json = std::string("{") + DiscordUtils::buildJsonPair("name", std::string(name.data(), name.length())) + "}";
	return bot_->submitRestTask([guildId, json](DiscordHTTP& http)
	{
		http.modifyGuild(guildId, json);
	}) ? true : false;
}

IDiscordChannel* DiscordGuild::getChannel(StringView channelId)
{
	if (!component_)
	{
		return nullptr;
	}
	return component_->findChannelById(channelId);
}

IDiscordRole* DiscordGuild::getRole(StringView roleId)
{
	if (!component_)
	{
		return nullptr;
	}
	return component_->findRoleByIdInternal(roleId);
}

const DiscordGuild::Member* DiscordGuild::findMember(StringView userId) const
{
	const std::string id(userId.data(), userId.length());
	const auto it = members_.find(id);
	return it == members_.end() ? nullptr : &it->second;
}

DiscordGuild::Member* DiscordGuild::findMember(StringView userId)
{
	const std::string id(userId.data(), userId.length());
	const auto it = members_.find(id);
	return it == members_.end() ? nullptr : &it->second;
}

void DiscordGuild::updateFromJson(const std::string& json, bool includeMembers)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		return;
	}

	if ((data.find("id") != data.end()) && data["id"].is_string()) guildId_ = data["id"].get<std::string>();
	if ((data.find("name") != data.end()) && data["name"].is_string()) guildName_ = data["name"].get<std::string>();
	if ((data.find("owner_id") != data.end()) && data["owner_id"].is_string()) ownerId_ = data["owner_id"].get<std::string>();
	if ((data.find("member_count") != data.end()) && data["member_count"].is_number_integer()) memberCount_ = data["member_count"].get<int>();
	else if ((data.find("approximate_member_count") != data.end()) && data["approximate_member_count"].is_number_integer()) memberCount_ = data["approximate_member_count"].get<int>();
	readNullableString(data, "icon", iconHash_);
	readNullableString(data, "banner", bannerHash_);
	readNullableString(data, "description", description_);

	if ((data.find("roles") != data.end()) && data["roles"].is_array())
	{
		roleIds_.clear();
		for (const auto& role : data["roles"])
		{
			if (role.is_object() && (role.find("id") != role.end()) && role["id"].is_string())
			{
				roleIds_.push_back(role["id"].get<std::string>());
			}
		}
	}
	if ((data.find("channels") != data.end()) && data["channels"].is_array())
	{
		channelIds_.clear();
		for (const auto& channel : data["channels"])
		{
			if (channel.is_object() && (channel.find("id") != channel.end()) && channel["id"].is_string())
			{
				channelIds_.push_back(channel["id"].get<std::string>());
			}
		}
	}

	if (includeMembers && (data.find("members") != data.end()) && data["members"].is_array())
	{
		for (const auto& member : data["members"])
		{
			if (member.is_object())
			{
				updateMemberFromJson(member.dump(-1, ' ', false, DiscordJson::error_handler_t::replace));
			}
		}
	}
	if ((data.find("voice_states") != data.end()) && data["voice_states"].is_array())
	{
		for (const auto& voiceState : data["voice_states"])
		{
			if (!voiceState.is_object()) continue;
			const std::string userId = jsonString(voiceState, "user_id");
			if (!userId.empty()) updateMemberFromJson(voiceState.dump(-1, ' ', false, DiscordJson::error_handler_t::replace), userId);
		}
	}
	if ((data.find("presences") != data.end()) && data["presences"].is_array())
	{
		for (const auto& presence : data["presences"])
		{
			if (!presence.is_object()) continue;
			const DiscordJson user = presence.value("user", DiscordJson::object());
			const std::string userId = user.is_object() ? jsonString(user, "id") : std::string();
			if (userId.empty()) continue;
			DiscordJson member = {
				{ "user_id", userId },
				{ "presence", { { "status", jsonString(presence, "status", "offline") } } }
			};
			updateMemberFromJson(member.dump(-1, ' ', false, DiscordJson::error_handler_t::replace), userId);
		}
	}
}

void DiscordGuild::updateMemberFromJson(const std::string& json, StringView fallbackUserId)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		return;
	}

	std::string userId;
	if ((data.find("user") != data.end()) && data["user"].is_object() && (data["user"].find("id") != data["user"].end()) && data["user"]["id"].is_string())
	{
		userId = data["user"]["id"].get<std::string>();
	}
	if (userId.empty() && (data.find("user_id") != data.end()) && data["user_id"].is_string())
	{
		userId = data["user_id"].get<std::string>();
	}
	if (userId.empty())
	{
		userId.assign(fallbackUserId.data(), fallbackUserId.length());
	}
	if (userId.empty())
	{
		return;
	}

	auto [memberIt, inserted] = members_.try_emplace(userId);
	if (inserted)
	{
		memberOrder_.push_back(userId);
	}
	Member& member = memberIt->second;
	member.userId = userId;
	if ((data.find("nick") != data.end()) && (data["nick"].is_string() || data["nick"].is_null()))
	{
		member.nickname = data["nick"].is_string() ? data["nick"].get<std::string>() : std::string();
	}
	readNullableString(data, "avatar", member.avatarHash);
	readNullableString(data, "joined_at", member.joinedAt);
	readNullableString(data, "premium_since", member.premiumSince);
	readNullableString(data, "communication_disabled_until", member.timeoutUntil);
	readBool(data, "pending", member.pending);
	readBool(data, "mute", member.mute);
	readBool(data, "deaf", member.deaf);
	if ((data.find("roles") != data.end()) && data["roles"].is_array())
	{
		member.roleIds.clear();
		for (const auto& role : data["roles"])
		{
			if (role.is_string()) member.roleIds.push_back(role.get<std::string>());
		}
	}
	if ((data.find("channel_id") != data.end()) && (data["channel_id"].is_string() || data["channel_id"].is_null()))
	{
		member.voiceChannelId = data["channel_id"].is_string() ? data["channel_id"].get<std::string>() : std::string();
	}
	if ((data.find("presence") != data.end()) && data["presence"].is_object() && (data["presence"].find("status") != data["presence"].end()) && data["presence"]["status"].is_string())
	{
		const std::string status = data["presence"]["status"].get<std::string>();
		member.presenceStatus = status == "online" ? 1 : status == "idle" ? 2 : status == "dnd" ? 3 : status == "offline" ? 4 : 0;
	}
}

void DiscordGuild::removeMember(StringView userId)
{
	const std::string id(userId.data(), userId.length());
	members_.erase(id);
	memberOrder_.erase(std::remove(memberOrder_.begin(), memberOrder_.end(), id), memberOrder_.end());
}

void DiscordGuild::addRoleId(StringView id)
{
	const std::string value(id.data(), id.length());
	if (!value.empty() && std::find(roleIds_.begin(), roleIds_.end(), value) == roleIds_.end()) roleIds_.push_back(value);
}

void DiscordGuild::addChannelId(StringView id)
{
	const std::string value(id.data(), id.length());
	if (!value.empty() && std::find(channelIds_.begin(), channelIds_.end(), value) == channelIds_.end()) channelIds_.push_back(value);
}
