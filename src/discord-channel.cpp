/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-channel.hpp"
#include "discord-bot.hpp"
#include "discord-http.hpp"
#include "discord-json.hpp"
#include "utils.hpp"

DiscordChannel::DiscordChannel(DiscordBot* bot, StringView id, StringView name, EDiscordChannelType type)
	: channelId_(id.data(), id.length())
	, channelName_(name.data(), name.length())
	, type_(type)
	, position_(0)
	, nsfw_(false)
	, flags_(0)
	, bot_(bot)
{
}

bool DiscordChannel::sendMessage(StringView content)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string message(content.data(), content.length());
	return bot_->submitRestTask([channelId, message](DiscordHTTP& http)
	{
		http.sendMessage(channelId, message);
	}) ? true : false;
}

bool DiscordChannel::setName(StringView name)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string json = std::string("{") + DiscordUtils::buildJsonPair("name", std::string(name.data(), name.length())) + "}";
	return bot_->submitRestTask([channelId, json](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, json);
	}) ? true : false;
}

bool DiscordChannel::setTopic(StringView topic)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string json = std::string("{") + DiscordUtils::buildJsonPair("topic", std::string(topic.data(), topic.length())) + "}";
	return bot_->submitRestTask([channelId, json](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, json);
	}) ? true : false;
}

bool DiscordChannel::deleteChannel()
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	return bot_->submitRestTask([channelId](DiscordHTTP& http)
	{
		http.deleteChannel(channelId);
	}) ? true : false;
}

bool DiscordChannel::updatePosition(int position)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("position", static_cast<int64_t>(position)) + "}";
	return bot_->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? true : false;
}

bool DiscordChannel::updateNSFW(bool nsfw)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("nsfw", nsfw) + "}";
	return bot_->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? true : false;
}

bool DiscordChannel::setParentCategory(StringView parentId)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string parent(parentId.data(), parentId.length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("parent_id", parent) + "}";
	const std::string channelId = channelId_;
	return bot_->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? true : false;
}

void DiscordChannel::updateFromJson(const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object()) return;
	if ((data.find("id") != data.end()) && data["id"].is_string()) channelId_ = data["id"].get<std::string>();
	if ((data.find("name") != data.end()) && data["name"].is_string()) channelName_ = data["name"].get<std::string>();
	if ((data.find("guild_id") != data.end()) && (data["guild_id"].is_string() || data["guild_id"].is_null())) guildId_ = data["guild_id"].is_string() ? data["guild_id"].get<std::string>() : std::string();
	if ((data.find("topic") != data.end()) && (data["topic"].is_string() || data["topic"].is_null())) topic_ = data["topic"].is_string() ? data["topic"].get<std::string>() : std::string();
	if ((data.find("parent_id") != data.end()) && (data["parent_id"].is_string() || data["parent_id"].is_null())) parentId_ = data["parent_id"].is_string() ? data["parent_id"].get<std::string>() : std::string();
	if ((data.find("last_message_id") != data.end()) && (data["last_message_id"].is_string() || data["last_message_id"].is_null())) lastMessageId_ = data["last_message_id"].is_string() ? data["last_message_id"].get<std::string>() : std::string();
	if ((data.find("type") != data.end()) && data["type"].is_number_integer()) type_ = static_cast<EDiscordChannelType>(data["type"].get<int>());
	if ((data.find("position") != data.end()) && data["position"].is_number_integer()) position_ = data["position"].get<int>();
	if ((data.find("nsfw") != data.end()) && data["nsfw"].is_boolean()) nsfw_ = data["nsfw"].get<bool>();
	if ((data.find("flags") != data.end()) && data["flags"].is_number_unsigned()) flags_ = data["flags"].get<uint32_t>();
	if ((data.find("rate_limit_per_user") != data.end()) && data["rate_limit_per_user"].is_number_integer()) rateLimitPerUser_ = data["rate_limit_per_user"].get<int>();
}
