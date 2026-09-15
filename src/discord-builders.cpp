/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-builders.hpp"
#include <algorithm>
#include <cmath>

namespace DiscordBuilders
{
namespace
{
constexpr size_t MAX_CONTENT = 2000;
constexpr size_t MAX_TEXT_DISPLAY = 4000;
constexpr size_t MAX_EMBEDS = 10;
constexpr size_t MAX_EMBED_CHARACTERS = 6000;
constexpr size_t MAX_ACTION_ROWS = 5;
constexpr size_t MAX_V2_COMPONENTS = 40;
constexpr size_t MAX_MODAL_COMPONENTS = 5;

bool hasString(const DiscordJson& data, const char* key)
{
	const auto it = data.find(key);
	return it != data.end() && it->is_string() && !it->get<std::string>().empty();
}

bool isDigits(const std::string& value)
{
	return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}
}

DiscordJson Embed::toJson() const
{
	DiscordJson result = DiscordJson::object();
	if (!title.empty()) result["title"] = title;
	if (!description.empty()) result["description"] = description;
	if (!url.empty()) result["url"] = url;
	if (!timestamp.empty()) result["timestamp"] = timestamp;
	if (color != 0) result["color"] = color & 0xFFFFFF;
	if (!footerText.empty())
	{
		result["footer"] = { { "text", footerText } };
		if (!footerIconUrl.empty()) result["footer"]["icon_url"] = footerIconUrl;
	}
	if (!thumbnailUrl.empty()) result["thumbnail"] = { { "url", thumbnailUrl } };
	if (!imageUrl.empty()) result["image"] = { { "url", imageUrl } };
	if (!authorName.empty())
	{
		result["author"] = { { "name", authorName } };
		if (!authorUrl.empty()) result["author"]["url"] = authorUrl;
		if (!authorIconUrl.empty()) result["author"]["icon_url"] = authorIconUrl;
	}
	if (!fields.empty())
	{
		result["fields"] = DiscordJson::array();
		for (const auto& field : fields)
		{
			result["fields"].push_back({ { "name", field.name }, { "value", field.value }, { "inline", field.inlineField } });
		}
	}
	return result;
}

size_t Embed::characterCount() const
{
	size_t count = title.size() + description.size() + footerText.size() + authorName.size();
	for (const auto& field : fields) count += field.name.size() + field.value.size();
	return count;
}

bool Embed::empty() const
{
	return title.empty() && description.empty() && fields.empty() && imageUrl.empty() &&
		thumbnailUrl.empty() && footerText.empty() && authorName.empty();
}

bool isSelectType(int type)
{
	return type == StringSelect || type == UserSelect || type == RoleSelect ||
		type == MentionableSelect || type == ChannelSelect;
}

bool isV2LayoutType(int type)
{
	return type == Section || type == TextDisplay || type == Thumbnail || type == MediaGallery ||
		type == File || type == Separator || type == Container;
}

bool isKnownComponentType(int type)
{
	return (type >= ActionRow && type <= Separator) || type == Container || type == Label || type == FileUpload;
}

DiscordJson parseEmoji(const std::string& emoji)
{
	if (emoji.empty()) return nullptr;
	std::string body = emoji;
	bool animated = false;
	if (body.size() > 2 && body.front() == '<' && body.back() == '>')
	{
		body = body.substr(1, body.size() - 2);
		if (body.rfind("a:", 0) == 0)
		{
			animated = true;
			body.erase(0, 2);
		}
		else if (!body.empty() && body.front() == ':')
		{
			body.erase(0, 1);
		}
	}
	const size_t colon = body.rfind(':');
	if (colon != std::string::npos && isDigits(body.substr(colon + 1)))
	{
		DiscordJson result = { { "id", body.substr(colon + 1) }, { "name", body.substr(0, colon) } };
		if (animated) result["animated"] = true;
		return result;
	}
	return { { "name", emoji } };
}

Handle ComponentStore::create(int type)
{
	if (!isKnownComponentType(type) || components_.size() >= MAX_COMPONENTS) return 0;
	while (next_ <= 0 || components_.count(next_)) next_ = next_ <= 0 ? 1 : next_ + 1;
	const Handle handle = next_++;
	Component component;
	component.type = type;
	if (type == Button) component.data["style"] = 1;
	if (type == TextInput) component.data["style"] = 1;
	if (type == Separator)
	{
		component.data["divider"] = true;
		component.data["spacing"] = 1;
	}
	if (type == MediaGallery) component.data["items"] = DiscordJson::array();
	if (type == StringSelect) component.data["options"] = DiscordJson::array();
	components_.emplace(handle, std::move(component));
	return handle;
}

Component* ComponentStore::get(Handle handle)
{
	const auto it = components_.find(handle);
	return it == components_.end() ? nullptr : &it->second;
}

const Component* ComponentStore::get(Handle handle) const
{
	const auto it = components_.find(handle);
	return it == components_.end() ? nullptr : &it->second;
}

bool ComponentStore::destroy(Handle handle)
{
	const auto it = components_.find(handle);
	if (it == components_.end()) return false;
	const std::vector<Handle> children = it->second.children;
	const Handle accessory = it->second.accessory;
	components_.erase(it);
	for (const Handle child : children) destroy(child);
	if (accessory) destroy(accessory);
	return true;
}

bool ComponentStore::attach(Handle handle)
{
	Component* component = get(handle);
	if (!component || component->attached) return false;
	component->attached = true;
	return true;
}

bool ComponentStore::contains(Handle root, Handle target) const
{
	if (root == target) return true;
	const Component* component = get(root);
	if (!component) return false;
	for (const Handle child : component->children)
	{
		if (contains(child, target)) return true;
	}
	return component->accessory && contains(component->accessory, target);
}

bool ComponentStore::addChild(Handle parentHandle, Handle childHandle, std::string& error)
{
	Component* parent = get(parentHandle);
	Component* child = get(childHandle);
	if (!parent || !child)
	{
		error = "invalid component handle";
		return false;
	}
	if (child->attached)
	{
		error = "component is already attached to another parent";
		return false;
	}
	if (contains(childHandle, parentHandle))
	{
		error = "component cannot contain itself";
		return false;
	}

	const size_t count = parent->children.size();
	bool allowed = false;
	switch (parent->type)
	{
		case ActionRow:
		{
			if (child->type == Button)
			{
				allowed = count < 5 && std::all_of(parent->children.begin(), parent->children.end(),
					[this](Handle handle) { const Component* c = get(handle); return c && c->type == Button; });
				if (!allowed) error = "an action row holds up to 5 buttons and cannot mix buttons with a select menu";
			}
			else if (isSelectType(child->type) || child->type == TextInput)
			{
				allowed = count == 0;
				if (!allowed) error = "a select menu or text input must be alone in its action row";
			}
			else error = "action rows only accept buttons, select menus and text inputs";
			break;
		}
		case Section:
			allowed = child->type == TextDisplay && count < 3;
			if (!allowed) error = "a section accepts 1 to 3 text displays";
			break;
		case Container:
			allowed = child->type == ActionRow || child->type == TextDisplay || child->type == Section ||
				child->type == MediaGallery || child->type == Separator || child->type == File;
			if (!allowed) error = "containers accept action rows, text displays, sections, media galleries, separators and files";
			break;
		case Label:
			allowed = count == 0 && (child->type == TextInput || isSelectType(child->type) || child->type == FileUpload);
			if (!allowed) error = "a label wraps exactly one text input, select menu or file upload";
			break;
		default:
			error = "this component type cannot have children";
			break;
	}
	if (!allowed) return false;
	parent->children.push_back(childHandle);
	child->attached = true;
	return true;
}

bool ComponentStore::setAccessory(Handle sectionHandle, Handle accessoryHandle, std::string& error)
{
	Component* section = get(sectionHandle);
	Component* accessory = get(accessoryHandle);
	if (!section || !accessory || section->type != Section)
	{
		error = "accessories can only be set on sections";
		return false;
	}
	if (accessory->type != Button && accessory->type != Thumbnail)
	{
		error = "a section accessory must be a button or a thumbnail";
		return false;
	}
	if (accessory->attached || contains(accessoryHandle, sectionHandle))
	{
		error = "component is already attached to another parent";
		return false;
	}
	if (section->accessory) destroy(section->accessory);
	get(sectionHandle)->accessory = accessoryHandle;
	get(accessoryHandle)->attached = true;
	return true;
}

bool ComponentStore::addSelectOption(Handle menuHandle, const std::string& label, const std::string& value,
	const std::string& description, const std::string& emoji, bool isDefault, std::string& error)
{
	Component* menu = get(menuHandle);
	if (!menu || menu->type != StringSelect)
	{
		error = "options can only be added to string select menus";
		return false;
	}
	if (label.empty() || label.size() > 100 || value.empty() || value.size() > 100 || description.size() > 100)
	{
		error = "select option label and value must have 1-100 characters";
		return false;
	}
	DiscordJson& options = menu->data["options"];
	if (options.size() >= 25)
	{
		error = "a select menu holds up to 25 options";
		return false;
	}
	DiscordJson option = { { "label", label }, { "value", value } };
	if (!description.empty()) option["description"] = description;
	if (!emoji.empty()) option["emoji"] = parseEmoji(emoji);
	if (isDefault) option["default"] = true;
	options.push_back(std::move(option));
	return true;
}

bool ComponentStore::addSelectDefaultValue(Handle menuHandle, const std::string& id, const std::string& type, std::string& error)
{
	Component* menu = get(menuHandle);
	if (!menu || !isSelectType(menu->type) || menu->type == StringSelect)
	{
		error = "default values are only supported by user, role, mentionable and channel select menus";
		return false;
	}
	if (!isDigits(id) || (type != "user" && type != "role" && type != "channel"))
	{
		error = "default values need a snowflake and a user, role or channel type";
		return false;
	}
	DiscordJson& values = menu->data["default_values"];
	if (!values.is_array()) values = DiscordJson::array();
	if (values.size() >= 25)
	{
		error = "a select menu holds up to 25 default values";
		return false;
	}
	values.push_back({ { "id", id }, { "type", type } });
	return true;
}

bool ComponentStore::addMediaItem(Handle galleryHandle, const std::string& url, const std::string& description, bool spoiler, std::string& error)
{
	Component* gallery = get(galleryHandle);
	if (!gallery || gallery->type != MediaGallery)
	{
		error = "items can only be added to media galleries";
		return false;
	}
	DiscordJson& items = gallery->data["items"];
	if (url.empty() || items.size() >= 10)
	{
		error = "a media gallery holds 1 to 10 items with a URL";
		return false;
	}
	DiscordJson item = { { "media", { { "url", url } } } };
	if (!description.empty()) item["description"] = description;
	if (spoiler) item["spoiler"] = true;
	items.push_back(std::move(item));
	return true;
}

DiscordJson ComponentStore::render(Handle handle) const
{
	const Component* component = get(handle);
	if (!component) return nullptr;
	DiscordJson out = component->data;
	out["type"] = component->type;
	if (component->type == Label)
	{
		if (!component->children.empty()) out["component"] = render(component->children.front());
	}
	else if (!component->children.empty())
	{
		DiscordJson children = DiscordJson::array();
		for (const Handle child : component->children) children.push_back(render(child));
		out["components"] = std::move(children);
	}
	if (component->accessory) out["accessory"] = render(component->accessory);
	return out;
}

bool ComponentStore::validate(Handle handle, std::string& error) const
{
	const Component* component = get(handle);
	if (!component)
	{
		error = "invalid component handle";
		return false;
	}
	const DiscordJson& data = component->data;
	switch (component->type)
	{
		case ActionRow:
			if (component->children.empty()) { error = "action row is empty"; return false; }
			break;
		case Button:
		{
			const int style = jsonInt(data, "style", 1);
			if (style < 1 || style > 6) { error = "button style is invalid"; return false; }
			if (style == 5 && !hasString(data, "url")) { error = "link buttons require a URL"; return false; }
			if (style == 6 && !hasString(data, "sku_id")) { error = "premium buttons require a SKU id"; return false; }
			if (style <= 4 && !hasString(data, "custom_id")) { error = "buttons require a custom id"; return false; }
			if (style != 6 && !hasString(data, "label") && data.find("emoji") == data.end()) { error = "buttons require a label or an emoji"; return false; }
			break;
		}
		case StringSelect:
			if (!hasString(data, "custom_id")) { error = "select menus require a custom id"; return false; }
			if (data["options"].empty()) { error = "string select menus require at least one option"; return false; }
			break;
		case UserSelect:
		case RoleSelect:
		case MentionableSelect:
		case ChannelSelect:
		case TextInput:
		case FileUpload:
			if (!hasString(data, "custom_id")) { error = "interactive components require a custom id"; return false; }
			break;
		case Section:
			if (component->children.empty()) { error = "sections require at least one text display"; return false; }
			if (!component->accessory) { error = "sections require a button or thumbnail accessory"; return false; }
			break;
		case TextDisplay:
			if (!hasString(data, "content")) { error = "text displays require content"; return false; }
			if (data["content"].get<std::string>().size() > MAX_TEXT_DISPLAY) { error = "text display content exceeds 4000 characters"; return false; }
			break;
		case Thumbnail:
			if (data.find("media") == data.end()) { error = "thumbnails require a media URL"; return false; }
			break;
		case MediaGallery:
			if (data["items"].empty()) { error = "media galleries require at least one item"; return false; }
			break;
		case File:
			if (data.find("file") == data.end()) { error = "file components require an attachment:// URL"; return false; }
			break;
		case Container:
			if (component->children.empty()) { error = "containers require at least one child"; return false; }
			break;
		case Label:
			if (!hasString(data, "label")) { error = "labels require a label text"; return false; }
			if (component->children.size() != 1) { error = "labels must wrap exactly one component"; return false; }
			break;
		default:
			break;
	}
	if (isSelectType(component->type))
	{
		const int minValues = jsonInt(data, "min_values", 1);
		const int maxValues = jsonInt(data, "max_values", 1);
		if (minValues < 0 || maxValues < 1 || minValues > maxValues || maxValues > 25)
		{
			error = "select menu value range is invalid";
			return false;
		}
	}
	for (const Handle child : component->children)
	{
		if (!validate(child, error)) return false;
	}
	return !component->accessory || validate(component->accessory, error);
}

size_t ComponentStore::countTree(Handle handle) const
{
	const Component* component = get(handle);
	if (!component) return 0;
	size_t count = 1;
	for (const Handle child : component->children) count += countTree(child);
	if (component->accessory) count += countTree(component->accessory);
	return count;
}

void ComponentStore::clear()
{
	components_.clear();
	next_ = 1;
}

bool renderMessage(const MessageBuilder& message, const ComponentStore& store, bool ephemeral,
	DiscordJson& out, std::string& error)
{
	out = DiscordJson::object();
	bool componentsV2 = false;
	size_t actionRows = 0;
	size_t totalComponents = 0;
	for (const Handle handle : message.components)
	{
		const Component* component = store.get(handle);
		if (!component)
		{
			error = "message references a destroyed component";
			return false;
		}
		if (!store.validate(handle, error)) return false;
		if (component->type == ActionRow) ++actionRows;
		else if (isV2LayoutType(component->type)) componentsV2 = true;
		else
		{
			error = "top-level message components must be action rows or Components V2 layout components";
			return false;
		}
		totalComponents += store.countTree(handle);
	}

	int flags = 0;
	DiscordJson components = DiscordJson::array();
	if (componentsV2)
	{
		if (!message.embeds.empty())
		{
			error = "Components V2 messages cannot contain embeds";
			return false;
		}
		if (!message.content.empty())
		{
			if (message.content.size() > MAX_TEXT_DISPLAY)
			{
				error = "message content exceeds 4000 characters";
				return false;
			}
			components.push_back({ { "type", TextDisplay }, { "content", message.content } });
			++totalComponents;
		}
		if (totalComponents > MAX_V2_COMPONENTS)
		{
			error = "Components V2 messages are limited to 40 components";
			return false;
		}
		flags |= MESSAGE_FLAG_COMPONENTS_V2;
	}
	else
	{
		if (message.content.size() > MAX_CONTENT)
		{
			error = "message content exceeds 2000 characters";
			return false;
		}
		if (actionRows > MAX_ACTION_ROWS)
		{
			error = "messages are limited to 5 action rows";
			return false;
		}
		if (message.embeds.size() > MAX_EMBEDS)
		{
			error = "messages are limited to 10 embeds";
			return false;
		}
		size_t embedCharacters = 0;
		for (const auto& embed : message.embeds) embedCharacters += embed.characterCount();
		if (embedCharacters > MAX_EMBED_CHARACTERS)
		{
			error = "embeds exceed 6000 characters in total";
			return false;
		}
		if (message.content.empty() && message.embeds.empty() && message.components.empty())
		{
			error = "message is empty";
			return false;
		}
		out["content"] = message.content;
		out["embeds"] = DiscordJson::array();
		for (const auto& embed : message.embeds) out["embeds"].push_back(embed.toJson());
	}
	for (const Handle handle : message.components) components.push_back(store.render(handle));
	// Always send the array so an edit can remove the components of an
	// existing message.
	out["components"] = std::move(components);

	if (message.tts) out["tts"] = true;
	if (message.suppressEmbeds) flags |= MESSAGE_FLAG_SUPPRESS_EMBEDS;
	if (message.silent) flags |= MESSAGE_FLAG_SUPPRESS_NOTIFICATIONS;
	if (ephemeral) flags |= MESSAGE_FLAG_EPHEMERAL;
	if (flags != 0) out["flags"] = flags;

	DiscordJson allowedMentions = message.allowedMentions ? *message.allowedMentions : DiscordJson(nullptr);
	if (!message.replyMessageId.empty())
	{
		out["message_reference"] = { { "message_id", message.replyMessageId }, { "fail_if_not_exists", false } };
		if (!message.replyMention)
		{
			if (allowedMentions.is_null()) allowedMentions = { { "parse", { "users", "roles", "everyone" } } };
			allowedMentions["replied_user"] = false;
		}
	}
	if (!allowedMentions.is_null()) out["allowed_mentions"] = std::move(allowedMentions);
	return true;
}

bool renderModal(const Modal& modal, const ComponentStore& store, DiscordJson& out, std::string& error)
{
	if (modal.customId.empty() || modal.customId.size() > 100)
	{
		error = "modal custom id must have 1-100 characters";
		return false;
	}
	if (modal.title.empty() || modal.title.size() > 45)
	{
		error = "modal title must have 1-45 characters";
		return false;
	}
	if (modal.components.empty() || modal.components.size() > MAX_MODAL_COMPONENTS)
	{
		error = "modals require 1 to 5 components";
		return false;
	}
	DiscordJson components = DiscordJson::array();
	for (const Handle handle : modal.components)
	{
		const Component* component = store.get(handle);
		if (!component)
		{
			error = "modal references a destroyed component";
			return false;
		}
		const Component* onlyChild = component->children.size() == 1 ? store.get(component->children.front()) : nullptr;
		const bool legacyRow = component->type == ActionRow && onlyChild && onlyChild->type == TextInput;
		if (component->type != Label && component->type != TextDisplay && !legacyRow)
		{
			error = "modal components must be labels, text displays or action rows holding a text input";
			return false;
		}
		if (!store.validate(handle, error)) return false;
		components.push_back(store.render(handle));
	}
	out = { { "custom_id", modal.customId }, { "title", modal.title }, { "components", std::move(components) } };
	return true;
}

bool isValidCommandName(const std::string& name, int commandType)
{
	if (name.empty() || name.size() > 32) return false;
	if (commandType != ChatInputCommand) return true;
	return std::all_of(name.begin(), name.end(), [](unsigned char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c >= 0x80;
	});
}

Handle CommandStore::createCommand(int type, const std::string& name, const std::string& description,
	const std::string& guildId, std::string& error)
{
	if (type < ChatInputCommand || type > MessageCommand)
	{
		error = "unknown command type";
		return 0;
	}
	if (!isValidCommandName(name, type))
	{
		error = type == ChatInputCommand
			? "slash command names must have 1-32 lowercase letters, digits, '-' or '_'"
			: "context menu command names must have 1-32 characters";
		return 0;
	}
	if (type == ChatInputCommand && (description.empty() || description.size() > 100))
	{
		error = "slash command descriptions must have 1-100 characters";
		return 0;
	}
	if (commands_.size() >= MAX_COMMANDS)
	{
		error = "command limit reached";
		return 0;
	}
	if (findCommand(name, type, guildId))
	{
		error = "a command with this name already exists in this scope";
		return 0;
	}
	while (nextCommand_ <= 0 || commands_.count(nextCommand_)) nextCommand_ = nextCommand_ <= 0 ? 1 : nextCommand_ + 1;
	const Handle handle = nextCommand_++;
	Command command;
	command.type = type;
	command.name = name;
	command.description = type == ChatInputCommand ? description : std::string();
	command.guildId = guildId;
	commands_.emplace(handle, std::move(command));
	return handle;
}

Command* CommandStore::getCommand(Handle handle)
{
	const auto it = commands_.find(handle);
	return it == commands_.end() ? nullptr : &it->second;
}

const Command* CommandStore::getCommand(Handle handle) const
{
	const auto it = commands_.find(handle);
	return it == commands_.end() ? nullptr : &it->second;
}

CommandOption* CommandStore::getOption(Handle handle)
{
	const auto it = options_.find(handle);
	return it == options_.end() ? nullptr : &it->second;
}

const CommandOption* CommandStore::getOption(Handle handle) const
{
	const auto it = options_.find(handle);
	return it == options_.end() ? nullptr : &it->second;
}

void CommandStore::destroyOption(Handle handle)
{
	const auto it = options_.find(handle);
	if (it == options_.end()) return;
	const std::vector<Handle> children = it->second.options;
	options_.erase(it);
	for (const Handle child : children) destroyOption(child);
}

bool CommandStore::destroyCommand(Handle handle)
{
	const auto it = commands_.find(handle);
	if (it == commands_.end()) return false;
	const std::vector<Handle> options = it->second.options;
	commands_.erase(it);
	for (const Handle option : options) destroyOption(option);
	return true;
}

Handle CommandStore::addOption(Handle commandHandle, Handle parentHandle, int type, const std::string& name,
	const std::string& description, bool required, std::string& error)
{
	Command* command = getCommand(commandHandle);
	if (!command)
	{
		error = "invalid command handle";
		return 0;
	}
	if (command->type != ChatInputCommand)
	{
		error = "only slash commands can have options";
		return 0;
	}
	if (type < SubCommandOption || type > AttachmentOption)
	{
		error = "unknown option type";
		return 0;
	}
	if (!isValidCommandName(name, ChatInputCommand) || description.empty() || description.size() > 100)
	{
		error = "option names follow slash command naming rules and need a 1-100 character description";
		return 0;
	}

	std::vector<Handle>* siblings = &command->options;
	if (parentHandle != 0)
	{
		CommandOption* parent = getOption(parentHandle);
		if (!parent || parent->command != commandHandle)
		{
			error = "parent option does not belong to this command";
			return 0;
		}
		if (parent->type == SubCommandGroupOption && type != SubCommandOption)
		{
			error = "subcommand groups can only contain subcommands";
			return 0;
		}
		if (parent->type == SubCommandOption && (type == SubCommandOption || type == SubCommandGroupOption))
		{
			error = "subcommands cannot contain other subcommands";
			return 0;
		}
		if (parent->type != SubCommandOption && parent->type != SubCommandGroupOption)
		{
			error = "only subcommands and subcommand groups can have nested options";
			return 0;
		}
		siblings = &parent->options;
	}
	if (siblings->size() >= 25)
	{
		error = "a command level holds up to 25 options";
		return 0;
	}
	const bool nested = type == SubCommandOption || type == SubCommandGroupOption;
	for (const Handle sibling : *siblings)
	{
		const CommandOption* existing = getOption(sibling);
		if (!existing) continue;
		if (existing->name == name)
		{
			error = "an option with this name already exists at this level";
			return 0;
		}
		const bool existingNested = existing->type == SubCommandOption || existing->type == SubCommandGroupOption;
		if (existingNested != nested)
		{
			error = "subcommands cannot be mixed with regular options at the same level";
			return 0;
		}
	}

	while (nextOption_ <= 0 || options_.count(nextOption_)) nextOption_ = nextOption_ <= 0 ? 1 : nextOption_ + 1;
	const Handle handle = nextOption_++;
	CommandOption option;
	option.type = type;
	option.name = name;
	option.description = description;
	option.required = !nested && required;
	option.command = commandHandle;
	option.parent = parentHandle;
	options_.emplace(handle, std::move(option));
	siblings->push_back(handle);
	return handle;
}

bool CommandStore::addChoice(Handle optionHandle, const std::string& name, const DiscordJson& value, std::string& error)
{
	CommandOption* option = getOption(optionHandle);
	if (!option)
	{
		error = "invalid option handle";
		return false;
	}
	const bool valueMatches = (option->type == StringOption && value.is_string()) ||
		(option->type == IntegerOption && value.is_number_integer()) ||
		(option->type == NumberOption && value.is_number());
	if (!valueMatches)
	{
		error = "choices are supported by string, integer and number options with a matching value type";
		return false;
	}
	if (option->autocomplete)
	{
		error = "options with autocomplete cannot have fixed choices";
		return false;
	}
	if (name.empty() || name.size() > 100 || option->choices.size() >= 25 ||
		(value.is_string() && (value.get<std::string>().empty() || value.get<std::string>().size() > 100)))
	{
		error = "an option holds up to 25 choices with 1-100 character names and values";
		return false;
	}
	option->choices.push_back({ { "name", name }, { "value", value } });
	return true;
}

bool CommandStore::setAutocomplete(Handle optionHandle, bool enabled, std::string& error)
{
	CommandOption* option = getOption(optionHandle);
	if (!option || (option->type != StringOption && option->type != IntegerOption && option->type != NumberOption))
	{
		error = "autocomplete is supported by string, integer and number options";
		return false;
	}
	if (enabled && !option->choices.empty())
	{
		error = "options with fixed choices cannot use autocomplete";
		return false;
	}
	option->autocomplete = enabled;
	return true;
}

DiscordJson CommandStore::renderOption(Handle handle) const
{
	const CommandOption* option = getOption(handle);
	if (!option) return nullptr;
	DiscordJson out = { { "type", option->type }, { "name", option->name }, { "description", option->description } };
	const bool nested = option->type == SubCommandOption || option->type == SubCommandGroupOption;
	if (!nested && option->required) out["required"] = true;
	if (!option->choices.empty()) out["choices"] = option->choices;
	if (option->autocomplete) out["autocomplete"] = true;
	if (!option->channelTypes.empty()) out["channel_types"] = option->channelTypes;
	auto numberValue = [option](double value) -> DiscordJson
	{
		if (option->type == IntegerOption) return static_cast<std::int64_t>(std::llround(value));
		return value;
	};
	if (option->minValue) out["min_value"] = numberValue(*option->minValue);
	if (option->maxValue) out["max_value"] = numberValue(*option->maxValue);
	if (option->minLength >= 0) out["min_length"] = option->minLength;
	if (option->maxLength >= 0) out["max_length"] = option->maxLength;
	if (!option->options.empty())
	{
		// Discord rejects a required option that follows an optional one.
		std::vector<Handle> ordered = option->options;
		std::stable_partition(ordered.begin(), ordered.end(), [this](Handle child)
		{
			const CommandOption* value = getOption(child);
			return value && value->required;
		});
		out["options"] = DiscordJson::array();
		for (const Handle child : ordered) out["options"].push_back(renderOption(child));
	}
	return out;
}

DiscordJson CommandStore::renderCommand(Handle handle) const
{
	const Command* command = getCommand(handle);
	if (!command) return nullptr;
	DiscordJson out = { { "name", command->name }, { "type", command->type } };
	if (command->type == ChatInputCommand) out["description"] = command->description;
	if (!command->options.empty())
	{
		std::vector<Handle> ordered = command->options;
		std::stable_partition(ordered.begin(), ordered.end(), [this](Handle child)
		{
			const CommandOption* value = getOption(child);
			return value && value->required;
		});
		out["options"] = DiscordJson::array();
		for (const Handle option : ordered) out["options"].push_back(renderOption(option));
	}
	if (command->hasPermissions) out["default_member_permissions"] = command->defaultMemberPermissions;
	if (command->nsfw) out["nsfw"] = true;
	if (!command->contexts.empty()) out["contexts"] = command->contexts;
	return out;
}

DiscordJson CommandStore::renderScope(const std::string& guildId) const
{
	DiscordJson out = DiscordJson::array();
	for (const auto& entry : commands_)
	{
		if (entry.second.guildId == guildId) out.push_back(renderCommand(entry.first));
	}
	return out;
}

std::vector<std::string> CommandStore::scopes() const
{
	std::vector<std::string> result;
	for (const auto& entry : commands_)
	{
		if (std::find(result.begin(), result.end(), entry.second.guildId) == result.end()) result.push_back(entry.second.guildId);
	}
	return result;
}

Handle CommandStore::findCommand(const std::string& name, int type, const std::string& guildId) const
{
	for (const auto& entry : commands_)
	{
		if (entry.second.name == name && entry.second.type == type && entry.second.guildId == guildId) return entry.first;
	}
	return 0;
}

void CommandStore::clear()
{
	commands_.clear();
	options_.clear();
	nextCommand_ = 1;
	nextOption_ = 1;
}

const DiscordJson* findCommandOption(const DiscordJson& data, const std::string& name)
{
	if (!data.is_object()) return nullptr;
	const auto options = data.find("options");
	if (options == data.end() || !options->is_array()) return nullptr;
	for (const auto& option : *options)
	{
		if (!option.is_object()) continue;
		const int type = jsonInt(option, "type", 0);
		if (type == SubCommandOption || type == SubCommandGroupOption)
		{
			if (const DiscordJson* nested = findCommandOption(option, name)) return nested;
			continue;
		}
		if (jsonString(option, "name") == name) return &option;
	}
	return nullptr;
}

const DiscordJson* findFocusedOption(const DiscordJson& data)
{
	if (!data.is_object()) return nullptr;
	const auto options = data.find("options");
	if (options == data.end() || !options->is_array()) return nullptr;
	for (const auto& option : *options)
	{
		if (!option.is_object()) continue;
		if (jsonBool(option, "focused", false)) return &option;
		if (const DiscordJson* nested = findFocusedOption(option)) return nested;
	}
	return nullptr;
}

std::string findSubcommand(const DiscordJson& data, bool group)
{
	if (!data.is_object()) return {};
	const auto options = data.find("options");
	if (options == data.end() || !options->is_array()) return {};
	for (const auto& option : *options)
	{
		if (!option.is_object()) continue;
		const int type = jsonInt(option, "type", 0);
		if (type == SubCommandGroupOption)
		{
			return group ? jsonString(option, "name") : findSubcommand(option, false);
		}
		if (type == SubCommandOption) return group ? std::string() : jsonString(option, "name");
	}
	return {};
}

namespace
{
void appendValues(const DiscordJson& component, std::vector<std::string>& out)
{
	const auto value = component.find("value");
	if (value != component.end() && value->is_string()) out.push_back(value->get<std::string>());
	const auto values = component.find("values");
	if (values != component.end() && values->is_array())
	{
		for (const auto& item : *values) out.push_back(jsonScalarToString(item));
	}
}

bool collectFromComponents(const DiscordJson& node, const std::string& customId, std::vector<std::string>& out)
{
	if (node.is_array())
	{
		for (const auto& item : node)
		{
			if (collectFromComponents(item, customId, out)) return true;
		}
		return false;
	}
	if (!node.is_object()) return false;
	if (jsonString(node, "custom_id") == customId)
	{
		appendValues(node, out);
		return true;
	}
	const auto nested = node.find("components");
	if (nested != node.end() && collectFromComponents(*nested, customId, out)) return true;
	const auto single = node.find("component");
	return single != node.end() && collectFromComponents(*single, customId, out);
}
}

void collectSubmittedValues(const DiscordJson& data, const std::string& customId, std::vector<std::string>& out)
{
	if (!data.is_object()) return;
	// Select menu interactions carry their values at the top level; modal
	// submissions nest them inside action rows or labels.
	if (customId.empty() || jsonString(data, "custom_id") == customId)
	{
		const auto values = data.find("values");
		if (values != data.end() && values->is_array())
		{
			appendValues(data, out);
			return;
		}
		if (customId.empty()) return;
	}
	const auto components = data.find("components");
	if (components != data.end()) collectFromComponents(*components, customId, out);
}

std::string jsonScalarToString(const DiscordJson& value)
{
	if (value.is_string()) return value.get<std::string>();
	if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
	if (value.is_null()) return {};
	return value.dump(-1, ' ', false, DiscordJson::error_handler_t::replace);
}
}
