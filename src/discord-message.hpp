/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include "discord-interface.hpp"
#include <string>
#include <vector>
#include <utility>

class DiscordBot;

class DiscordMessage : public IDiscordMessage
{
private:
	std::string messageId_;
	std::string channelId_;
	std::string authorId_;
	std::string content_;
	uint64_t timestamp_;
	bool tts_;
	bool mentionEveryone_;
	std::vector<std::string> userMentionIds_;
	std::vector<std::string> roleMentionIds_;
	bool persistent_;

	DiscordBot* bot_;

public:
	DiscordMessage(DiscordBot* bot, StringView msgId, StringView chanId, StringView authId, StringView content);

	StringView getMessageId() const override { return StringView(messageId_); }
	StringView getChannelId() const override { return StringView(channelId_); }
	StringView getAuthorId() const override { return StringView(authorId_); }
	StringView getContent() const override { return StringView(content_); }
	uint64_t getTimestamp() const override { return timestamp_; }
	bool isTTS() const override { return tts_; }
	bool mentionsEveryone() const override { return mentionEveryone_; }
	const std::vector<std::string>& getUserMentionIds() const { return userMentionIds_; }
	const std::vector<std::string>& getRoleMentionIds() const { return roleMentionIds_; }
	bool isPersistent() const { return persistent_; }

	bool deleteMessage() override;
	bool editMessage(StringView newContent) override;
	bool addReaction(StringView emoji) override;
	bool deleteReaction(StringView emoji);

	void setTimestamp(uint64_t ts) { timestamp_ = ts; }
	void setTTS(bool tts) { tts_ = tts; }
	void setMentionEveryone(bool mention) { mentionEveryone_ = mention; }
	void setContent(StringView content) { content_ = std::string(content.data(), content.length()); }
	void setPersistent(bool persistent) { persistent_ = persistent; }
	void setUserMentionIds(std::vector<std::string> ids) { userMentionIds_ = std::move(ids); }
	void setRoleMentionIds(std::vector<std::string> ids) { roleMentionIds_ = std::move(ids); }
	void updateFromJson(const std::string& json);
};
