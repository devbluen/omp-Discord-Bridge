/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

// Pure payload builders.  Nothing in this file touches AMX, the network or the
// entity cache, so every Discord JSON shape produced by the plugin can be unit
// tested in isolation.

#include "discord-json.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace DiscordBuilders
{
using Handle = std::int32_t;

enum ComponentType : int
{
	ActionRow = 1,
	Button = 2,
	StringSelect = 3,
	TextInput = 4,
	UserSelect = 5,
	RoleSelect = 6,
	MentionableSelect = 7,
	ChannelSelect = 8,
	Section = 9,
	TextDisplay = 10,
	Thumbnail = 11,
	MediaGallery = 12,
	File = 13,
	Separator = 14,
	Container = 17,
	Label = 18,
	FileUpload = 19
};

constexpr int MESSAGE_FLAG_SUPPRESS_EMBEDS = 1 << 2;
constexpr int MESSAGE_FLAG_EPHEMERAL = 1 << 6;
constexpr int MESSAGE_FLAG_SUPPRESS_NOTIFICATIONS = 1 << 12;
constexpr int MESSAGE_FLAG_COMPONENTS_V2 = 1 << 15;

struct EmbedField
{
	std::string name;
	std::string value;
	bool inlineField = false;
};

struct Embed
{
	std::string title;
	std::string description;
	std::string url;
	std::string timestamp;
	int color = 0;
	std::string footerText;
	std::string footerIconUrl;
	std::string thumbnailUrl;
	std::string imageUrl;
	std::string authorName;
	std::string authorUrl;
	std::string authorIconUrl;
	std::vector<EmbedField> fields;

	DiscordJson toJson() const;
	size_t characterCount() const;
	bool empty() const;
};

bool isSelectType(int type);
bool isV2LayoutType(int type);
bool isKnownComponentType(int type);

// Accepts a unicode emoji ("🔥"), a custom emoji mention ("<:name:id>" or
// "<a:name:id>") or the short "name:id" form.
DiscordJson parseEmoji(const std::string& emoji);

struct Component
{
	int type = 0;
	DiscordJson data = DiscordJson::object();
	std::vector<Handle> children;
	Handle accessory = 0;
	bool attached = false;
};

class ComponentStore
{
public:
	static constexpr size_t MAX_COMPONENTS = 16384;

	Handle create(int type);
	Component* get(Handle handle);
	const Component* get(Handle handle) const;
	// Destroys a component together with every child it owns.
	bool destroy(Handle handle);
	// Marks a root component as owned by a message, modal or parent.
	bool attach(Handle handle);
	bool addChild(Handle parent, Handle child, std::string& error);
	bool setAccessory(Handle section, Handle accessory, std::string& error);
	bool addSelectOption(Handle menu, const std::string& label, const std::string& value,
		const std::string& description, const std::string& emoji, bool isDefault, std::string& error);
	bool addSelectDefaultValue(Handle menu, const std::string& id, const std::string& type, std::string& error);
	bool addMediaItem(Handle gallery, const std::string& url, const std::string& description, bool spoiler, std::string& error);

	DiscordJson render(Handle handle) const;
	bool validate(Handle handle, std::string& error) const;
	size_t countTree(Handle handle) const;
	void clear();
	size_t size() const { return components_.size(); }

private:
	bool contains(Handle root, Handle target) const;

	std::map<Handle, Component> components_;
	Handle next_ = 1;
};

struct MessageBuilder
{
	std::string content;
	std::vector<Embed> embeds;
	std::vector<Handle> components;
	bool tts = false;
	bool suppressEmbeds = false;
	bool silent = false;
	std::string replyMessageId;
	bool replyMention = true;
	std::optional<DiscordJson> allowedMentions;
};

// Builds a create/edit message body.  Components V2 are detected
// automatically: plain content becomes a leading Text Display because
// Discord rejects `content` and `embeds` on V2 messages.
bool renderMessage(const MessageBuilder& message, const ComponentStore& store, bool ephemeral,
	DiscordJson& out, std::string& error);

struct Modal
{
	std::string customId;
	std::string title;
	std::vector<Handle> components;
};

bool renderModal(const Modal& modal, const ComponentStore& store, DiscordJson& out, std::string& error);

enum CommandType : int
{
	ChatInputCommand = 1,
	UserCommand = 2,
	MessageCommand = 3
};

enum OptionType : int
{
	SubCommandOption = 1,
	SubCommandGroupOption = 2,
	StringOption = 3,
	IntegerOption = 4,
	BooleanOption = 5,
	UserOption = 6,
	ChannelOption = 7,
	RoleOption = 8,
	MentionableOption = 9,
	NumberOption = 10,
	AttachmentOption = 11
};

struct CommandOption
{
	int type = StringOption;
	std::string name;
	std::string description;
	bool required = false;
	bool autocomplete = false;
	std::optional<double> minValue;
	std::optional<double> maxValue;
	int minLength = -1;
	int maxLength = -1;
	std::vector<int> channelTypes;
	DiscordJson choices = DiscordJson::array();
	std::vector<Handle> options;
	Handle command = 0;
	Handle parent = 0;
};

struct Command
{
	int type = ChatInputCommand;
	std::string name;
	std::string description;
	std::string guildId;
	std::string callback;
	int ownerScriptId = -1;
	bool hasPermissions = false;
	std::string defaultMemberPermissions;
	bool nsfw = false;
	std::vector<int> contexts;
	std::vector<Handle> options;
};

bool isValidCommandName(const std::string& name, int commandType);

class CommandStore
{
public:
	static constexpr size_t MAX_COMMANDS = 1024;

	Handle createCommand(int type, const std::string& name, const std::string& description,
		const std::string& guildId, std::string& error);
	Command* getCommand(Handle handle);
	const Command* getCommand(Handle handle) const;
	CommandOption* getOption(Handle handle);
	const CommandOption* getOption(Handle handle) const;
	bool destroyCommand(Handle handle);
	Handle addOption(Handle command, Handle parent, int type, const std::string& name,
		const std::string& description, bool required, std::string& error);
	bool addChoice(Handle option, const std::string& name, const DiscordJson& value, std::string& error);
	bool setAutocomplete(Handle option, bool enabled, std::string& error);

	DiscordJson renderCommand(Handle handle) const;
	// Every command in one registration scope; an empty id is the global scope.
	DiscordJson renderScope(const std::string& guildId) const;
	std::vector<std::string> scopes() const;
	Handle findCommand(const std::string& name, int type, const std::string& guildId) const;
	const std::map<Handle, Command>& commands() const { return commands_; }
	void clear();

private:
	DiscordJson renderOption(Handle handle) const;
	void destroyOption(Handle handle);

	std::map<Handle, Command> commands_;
	std::map<Handle, CommandOption> options_;
	Handle nextCommand_ = 1;
	Handle nextOption_ = 1;
};

// Interaction payload readers.  `data` is the interaction's `data` object.
const DiscordJson* findCommandOption(const DiscordJson& data, const std::string& name);
const DiscordJson* findFocusedOption(const DiscordJson& data);
std::string findSubcommand(const DiscordJson& data, bool group);
void collectSubmittedValues(const DiscordJson& data, const std::string& customId, std::vector<std::string>& out);
std::string jsonScalarToString(const DiscordJson& value);
}
