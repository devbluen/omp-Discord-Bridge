/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include "discord-interface.hpp"
#include <string>

class DiscordUser : public IDiscordUser
{
private:
	std::string userId_;
	std::string username_;
	std::string discriminator_;
	std::string globalName_;
	bool isBot_;
	bool verified_;
	bool system_ = false;
	std::string avatarHash_;
	std::string bannerHash_;

public:
	DiscordUser(StringView id, StringView username, StringView discriminator, bool isBot);

	StringView getUserId() const override { return StringView(userId_); }
	StringView getUsername() const override { return StringView(username_); }
	StringView getDiscriminator() const override { return StringView(discriminator_); }
	bool isBot() const override { return isBot_; }
	bool isVerified() const override { return verified_; }
	StringView getGlobalName() const { return StringView(globalName_); }
	bool isSystem() const { return system_; }
	const std::string& getAvatarHash() const { return avatarHash_; }
	const std::string& getBannerHash() const { return bannerHash_; }

	void setUsername(StringView name) { username_ = std::string(name.data(), name.length()); }
	void setDiscriminator(StringView disc) { discriminator_ = std::string(disc.data(), disc.length()); }
	void setGlobalName(StringView name) { globalName_ = std::string(name.data(), name.length()); }
	void setBot(bool value) { isBot_ = value; }
	void setVerified(bool value) { verified_ = value; }
	void updateFromJson(const std::string& json);
};
