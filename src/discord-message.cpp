/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-message.hpp"
#include "discord-bot.hpp"
#include "discord-http.hpp"
#include "discord-json.hpp"
#include "utils.hpp"

DiscordMessage::DiscordMessage(DiscordBot* bot, StringView msgId, StringView chanId, StringView authId, StringView content)
	: messageId_(msgId.data(), msgId.length())
	, channelId_(chanId.data(), chanId.length())
	, authorId_(authId.data(), authId.length())
	, content_(content.data(), content.length())
	, timestamp_(0)
	, tts_(false)
	, mentionEveryone_(false)
	, persistent_(false)
	, bot_(bot)
{
}

void DiscordMessage::updateFromJson(const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		return;
	}
	if ((data.find("id") != data.end()) && data["id"].is_string()) messageId_ = data["id"].get<std::string>();
	if ((data.find("channel_id") != data.end()) && data["channel_id"].is_string()) channelId_ = data["channel_id"].get<std::string>();
	if ((data.find("content") != data.end()) && data["content"].is_string()) content_ = data["content"].get<std::string>();
	if ((data.find("tts") != data.end()) && data["tts"].is_boolean()) tts_ = data["tts"].get<bool>();
	if ((data.find("mention_everyone") != data.end()) && data["mention_everyone"].is_boolean()) mentionEveryone_ = data["mention_everyone"].get<bool>();
	if ((data.find("timestamp") != data.end()) && data["timestamp"].is_string()) timestamp_ = DiscordUtils::parseDiscordTimestamp(data["timestamp"].get<std::string>());
	if ((data.find("author") != data.end()) && data["author"].is_object() && (data["author"].find("id") != data["author"].end()) && data["author"]["id"].is_string()) authorId_ = data["author"]["id"].get<std::string>();
	if (data.find("mentions") != data.end())
	{
		userMentionIds_.clear();
		if (data["mentions"].is_array()) for (const auto& mention : data["mentions"])
		{
			if (mention.is_object() && (mention.find("id") != mention.end()) && mention["id"].is_string()) userMentionIds_.push_back(mention["id"].get<std::string>());
		}
	}
	if (data.find("mention_roles") != data.end())
	{
		roleMentionIds_.clear();
		if (data["mention_roles"].is_array()) for (const auto& role : data["mention_roles"])
		{
			if (role.is_string()) roleMentionIds_.push_back(role.get<std::string>());
		}
	}
}

bool DiscordMessage::deleteMessage()
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string messageId = messageId_;
	return bot_->submitRestTask([channelId, messageId](DiscordHTTP& http)
	{
		http.deleteMessage(channelId, messageId);
	}) ? true : false;
}

bool DiscordMessage::editMessage(StringView newContent)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string messageId = messageId_;
	const std::string content(newContent.data(), newContent.length());
	return bot_->submitRestTask([channelId, messageId, content](DiscordHTTP& http)
	{
		http.editMessage(channelId, messageId, content);
	}) ? true : false;
}

bool DiscordMessage::addReaction(StringView emoji)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string messageId = messageId_;
	const std::string token(emoji.data(), emoji.length());
	return bot_->submitRestTask([channelId, messageId, token](DiscordHTTP& http)
	{
		http.addReaction(channelId, messageId, token);
	}) ? true : false;
}

bool DiscordMessage::deleteReaction(StringView emoji)
{
	if (!bot_ || !bot_->getHTTP())
	{
		return false;
	}

	const std::string channelId = channelId_;
	const std::string messageId = messageId_;
	const std::string token(emoji.data(), emoji.length());
	return bot_->submitRestTask([channelId, messageId, token](DiscordHTTP& http)
	{
		http.deleteOwnReaction(channelId, messageId, token);
	}) ? true : false;
}
