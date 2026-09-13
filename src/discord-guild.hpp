/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include "discord-interface.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

class DiscordBot;
class DiscordBridgeComponent;

class DiscordGuild : public IDiscordGuild
{
public:
	struct Member
	{
		std::string userId;
		std::string nickname;
		std::vector<std::string> roleIds;
		std::string voiceChannelId;
		int presenceStatus = 0;
		std::string avatarHash;
		std::string joinedAt;
		std::string premiumSince;
		std::string timeoutUntil;
		bool pending = false;
		bool mute = false;
		bool deaf = false;
	};

private:
	std::string guildId_;
	std::string guildName_;
	std::string ownerId_;
	int memberCount_;
	std::string iconHash_;
	std::string bannerHash_;
	std::string description_;
	std::vector<std::string> roleIds_;
	std::vector<std::string> channelIds_;
	std::vector<std::string> memberOrder_;
	std::unordered_map<std::string, Member> members_;

	DiscordBot* bot_;
	DiscordBridgeComponent* component_;

public:
	DiscordGuild(DiscordBot* bot, DiscordBridgeComponent* component, StringView id, StringView name);

	StringView getGuildId() const override { return StringView(guildId_); }
	StringView getGuildName() const override { return StringView(guildName_); }
	StringView getOwnerId() const override { return StringView(ownerId_); }
	int getMemberCount() const override { return memberCount_; }
	const std::string& getIconHash() const { return iconHash_; }
	const std::string& getBannerHash() const { return bannerHash_; }
	const std::string& getDescription() const { return description_; }
	const std::vector<std::string>& getRoleIds() const { return roleIds_; }
	const std::vector<std::string>& getChannelIds() const { return channelIds_; }
	const std::vector<std::string>& getMemberIds() const { return memberOrder_; }
	const Member* findMember(StringView userId) const;
	Member* findMember(StringView userId);
	const std::string* getRoleIdAt(size_t index) const { return index < roleIds_.size() ? &roleIds_[index] : nullptr; }
	const std::string* getChannelIdAt(size_t index) const { return index < channelIds_.size() ? &channelIds_[index] : nullptr; }
	const std::string* getMemberIdAt(size_t index) const { return index < memberOrder_.size() ? &memberOrder_[index] : nullptr; }

	bool setName(StringView name) override;
	IDiscordChannel* getChannel(StringView channelId) override;
	IDiscordRole* getRole(StringView roleId) override;

	void updateFromJson(const std::string& json, bool includeMembers = true);
	void updateMemberFromJson(const std::string& json, StringView fallbackUserId = {});
	void removeMember(StringView userId);
	void addRoleId(StringView id);
	void addChannelId(StringView id);
	void setRoleIds(std::vector<std::string> ids) { roleIds_ = std::move(ids); }
	void setChannelIds(std::vector<std::string> ids) { channelIds_ = std::move(ids); }

	void setOwnerId(StringView id) { ownerId_ = std::string(id.data(), id.length()); }
	void setMemberCount(int count) { memberCount_ = count; }
};
