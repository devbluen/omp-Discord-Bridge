/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include "discord-interface.hpp"
#include <string>

class DiscordRole : public IDiscordRole
{
private:
	std::string roleId_;
	std::string roleName_;
	std::string guildId_;
	uint32_t color_;
	uint64_t permissions_;
	bool hoisted_;
	int position_;
	bool mentionable_;

public:
	DiscordRole(StringView id, StringView name);

	StringView getRoleId() const override { return StringView(roleId_); }
	StringView getRoleName() const override { return StringView(roleName_); }
	StringView getGuildId() const { return StringView(guildId_); }
	uint32_t getColor() const override { return color_; }
	uint64_t getPermissions() const { return permissions_; }
	bool isHoisted() const override { return hoisted_; }
	int getPosition() const override { return position_; }
	bool isMentionable() const override { return mentionable_; }

	void setName(StringView name) { roleName_ = std::string(name.data(), name.length()); }
	void setColor(uint32_t color) { color_ = color; }
	void setGuildId(StringView id) { guildId_ = std::string(id.data(), id.length()); }
	void setPermissions(uint64_t permissions) { permissions_ = permissions; }
	void setHoisted(bool hoisted) { hoisted_ = hoisted; }
	void setPosition(int pos) { position_ = pos; }
	void setMentionable(bool mentionable) { mentionable_ = mentionable; }
	void updateFromJson(const std::string& json);
};
