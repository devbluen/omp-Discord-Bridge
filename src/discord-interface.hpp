/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <sdk.hpp>
#include <string>
#include <functional>

struct IDiscordBot;
struct IDiscordChannel;
struct IDiscordGuild;
struct IDiscordMessage;
struct IDiscordUser;
struct IDiscordRole;

struct DiscordMessageData
{
	std::string content;
	std::string channelId;
	std::string authorId;
	std::string messageId;
	uint64_t timestamp;
	bool tts;
	bool mentionEveryone;
};

enum class EDiscordChannelType
{
	GuildText = 0,
	DM = 1,
	GuildVoice = 2,
	GroupDM = 3,
	GuildCategory = 4,
	GuildNews = 5,
	GuildStore = 6,
	GuildNewsThread = 10,
	GuildPublicThread = 11,
	GuildPrivateThread = 12,
	GuildStageVoice = 13,
	GuildDirectory = 14,
	GuildForum = 15,
	GuildMedia = 16
};

enum class EDiscordPresenceStatus
{
	Online,
	DoNotDisturb,
	Idle,
	Invisible,
	Offline
};

enum class EDiscordActivityType
{
	Playing = 0,
	Streaming = 1,
	Listening = 2,
	Watching = 3,
	Competing = 5
};

// Preserve the original connector's default (ALL_INTENTS = (1 << 17) - 1).
// Deployments can override this to opt into newer Discord intent bits.
constexpr int DCC_DEFAULT_INTENTS = 131071;

struct IDiscordEventHandler
{
	virtual ~IDiscordEventHandler() = default;

	virtual void onBotReady() {}
	virtual void onBotDisconnected() {}

	virtual void onMessageCreate(IDiscordMessage& message) {}
	virtual void onMessageUpdate(IDiscordMessage& message) {}
	virtual void onMessageDelete(StringView channelId, StringView messageId) {}

	virtual void onGuildCreate(IDiscordGuild& guild) {}
	virtual void onGuildUpdate(IDiscordGuild& guild) {}
	virtual void onGuildDelete(StringView guildId) {}
	virtual void onGuildMemberAdd(IDiscordGuild& guild, IDiscordUser& user) {}
	virtual void onGuildMemberUpdate(IDiscordGuild& guild, IDiscordUser& user) {}
	virtual void onGuildMemberRemove(IDiscordGuild& guild, IDiscordUser& user) {}
	virtual void onMessageReaction(IDiscordMessage& message, IDiscordUser* user, StringView emoji, int reactionType) {}

	virtual void onChannelCreate(IDiscordChannel& channel) {}
	virtual void onChannelUpdate(IDiscordChannel& channel) {}
	virtual void onChannelDelete(IDiscordChannel& channel) {}
};

struct IDiscordBot : public IExtensible
{
	virtual ~IDiscordBot() = default;

	virtual StringView getBotId() const = 0;
	virtual StringView getBotUsername() const = 0;
	virtual bool isConnected() const = 0;

	virtual bool setPresenceStatus(EDiscordPresenceStatus status) = 0;
	virtual bool setActivity(EDiscordActivityType type, StringView name) = 0;
	virtual bool disconnect() = 0;
	virtual bool reconnect() = 0;
};

struct IDiscordChannel : public IExtensible
{
	virtual ~IDiscordChannel() = default;

	virtual StringView getChannelId() const = 0;
	virtual StringView getChannelName() const = 0;
	virtual StringView getGuildId() const = 0;
	virtual EDiscordChannelType getChannelType() const = 0;
	virtual StringView getTopic() const = 0;
	virtual int getPosition() const = 0;
	virtual bool isNSFW() const = 0;

	virtual bool sendMessage(StringView content) = 0;
	virtual bool setName(StringView name) = 0;
	virtual bool setTopic(StringView topic) = 0;
	virtual bool deleteChannel() = 0;
};

struct IDiscordGuild : public IExtensible
{
	virtual ~IDiscordGuild() = default;

	virtual StringView getGuildId() const = 0;
	virtual StringView getGuildName() const = 0;
	virtual StringView getOwnerId() const = 0;
	virtual int getMemberCount() const = 0;

	virtual bool setName(StringView name) = 0;
	virtual IDiscordChannel* getChannel(StringView channelId) = 0;
	virtual IDiscordRole* getRole(StringView roleId) = 0;
};

struct IDiscordMessage : public IExtensible
{
	virtual ~IDiscordMessage() = default;

	virtual StringView getMessageId() const = 0;
	virtual StringView getChannelId() const = 0;
	virtual StringView getAuthorId() const = 0;
	virtual StringView getContent() const = 0;
	virtual uint64_t getTimestamp() const = 0;
	virtual bool isTTS() const = 0;
	virtual bool mentionsEveryone() const = 0;

	virtual bool deleteMessage() = 0;
	virtual bool editMessage(StringView newContent) = 0;
	virtual bool addReaction(StringView emoji) = 0;
};

struct IDiscordUser : public IExtensible
{
	virtual ~IDiscordUser() = default;

	virtual StringView getUserId() const = 0;
	virtual StringView getUsername() const = 0;
	virtual StringView getDiscriminator() const = 0;
	virtual bool isBot() const = 0;
	virtual bool isVerified() const { return false; }
};

struct IDiscordRole : public IExtensible
{
	virtual ~IDiscordRole() = default;

	virtual StringView getRoleId() const = 0;
	virtual StringView getRoleName() const = 0;
	virtual uint32_t getColor() const = 0;
	virtual bool isHoisted() const = 0;
	virtual int getPosition() const = 0;
	virtual bool isMentionable() const = 0;
};

struct IDiscordBridgeComponent : public IComponent
{
	PROVIDE_UID(0xDAF38E7590642BAE);

	virtual ~IDiscordBridgeComponent() = default;

	virtual IDiscordBot* getBot() = 0;
	virtual bool connectBot(StringView token, int intents = DCC_DEFAULT_INTENTS) = 0;

	virtual IDiscordChannel* findChannelById(StringView channelId) = 0;
	virtual IDiscordChannel* findChannelByName(StringView channelName) = 0;
	virtual IDiscordGuild* findGuildById(StringView guildId) = 0;
	virtual IDiscordGuild* findGuildByName(StringView guildName) = 0;
	virtual IDiscordUser* findUserById(StringView userId) = 0;
	virtual IDiscordUser* findUserByName(StringView username) = 0;
	virtual IDiscordMessage* findMessageById(StringView messageId) = 0;

	virtual IEventDispatcher<IDiscordEventHandler>& getEventDispatcher() = 0;
};

struct IDiscordPlayerExtension : public IExtension
{
	PROVIDE_EXT_UID(0xA4CC94275468EB8B);

	virtual ~IDiscordPlayerExtension() = default;

	virtual void setLinkedDiscordId(StringView discordId) = 0;
	virtual StringView getLinkedDiscordId() const = 0;
	virtual bool isLinked() const = 0;
};
