/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include "discord-interface.hpp"
#include <string>

class DiscordBot;

class DiscordChannel : public IDiscordChannel
{
private:
	std::string channelId_;
	std::string channelName_;
	std::string guildId_;
	std::string topic_;
	std::string parentId_;
	std::string lastMessageId_;
	EDiscordChannelType type_;
	int position_;
	bool nsfw_;
	uint32_t flags_;
	int rateLimitPerUser_ = 0;

	DiscordBot* bot_;

public:
	DiscordChannel(DiscordBot* bot, StringView id, StringView name, EDiscordChannelType type);

	StringView getChannelId() const override { return StringView(channelId_); }
	StringView getChannelName() const override { return StringView(channelName_); }
	StringView getGuildId() const override { return StringView(guildId_); }
	EDiscordChannelType getChannelType() const override { return type_; }
	StringView getTopic() const override { return StringView(topic_); }
	int getPosition() const override { return position_; }
	bool isNSFW() const override { return nsfw_; }

	const std::string& getParentId() const { return parentId_; }
	const std::string& getLastMessageId() const { return lastMessageId_; }
	uint32_t getFlags() const { return flags_; }
	int getRateLimitPerUser() const { return rateLimitPerUser_; }

	bool sendMessage(StringView content) override;
	bool setName(StringView name) override;
	bool setTopic(StringView topic) override;
	bool deleteChannel() override;
	bool updatePosition(int position);
	bool updateNSFW(bool nsfw);
	bool setParentCategory(StringView parentId);

	void updateFromJson(const std::string& json);

	void setGuildId(StringView id) { guildId_ = std::string(id.data(), id.length()); }
	void setCachedTopic(StringView t) { topic_ = std::string(t.data(), t.length()); }
	void setPosition(int pos) { position_ = pos; }
	void setNSFW(bool nsfw) { nsfw_ = nsfw; }
	void setParentId(StringView id) { parentId_ = std::string(id.data(), id.length()); }
	void setLastMessageId(StringView id) { lastMessageId_ = std::string(id.data(), id.length()); }
	void setFlags(uint32_t flags) { flags_ = flags; }
};
