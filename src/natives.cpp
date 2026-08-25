/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "natives.hpp"
#include "discord-bot.hpp"
#include "discord-component.hpp"
#include "discord-channel.hpp"
#include "discord-guild.hpp"
#include "discord-http.hpp"
#include "discord-log.hpp"
#include "discord-message.hpp"
#include "discord-role.hpp"
#include "discord-user.hpp"
#include "discord-json.hpp"
#include "utils.hpp"
#include <algorithm>
#include <amx/amx.h>
#include <amx/amx2.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <cctype>
#include <cstdint>
#include <atomic>

namespace
{
using NativeFunc = cell (*)(AMX*, cell*);

using NativePawnScript = IPawnScript;
using NativePawnComponent = IPawnComponent;

DiscordBot* nativeBot();
DiscordBridgeComponent* component();

std::unordered_map<cell, std::string> g_channelHandleToId;
std::unordered_map<std::string, cell> g_channelIdToHandle;
cell g_nextChannelHandle = 1;
std::unordered_map<cell, std::string> g_userHandleToId;
std::unordered_map<std::string, cell> g_userIdToHandle;
cell g_nextUserHandle = 1;
std::unordered_map<cell, std::string> g_guildHandleToId;
std::unordered_map<std::string, cell> g_guildIdToHandle;
cell g_nextGuildHandle = 1;
std::unordered_map<cell, std::string> g_messageHandleToId;
std::unordered_map<std::string, cell> g_messageIdToHandle;
cell g_nextMessageHandle = 1;
std::unordered_map<cell, std::string> g_roleHandleToId;
std::unordered_map<std::string, cell> g_roleIdToHandle;
cell g_nextRoleHandle = 1;
std::unordered_map<cell, std::string> g_emojiHandleToToken;
cell g_nextEmojiHandle = 1;

struct EmbedData
{
	std::string title;
	std::string description;
	std::string url;
	std::string timestamp;
	std::string footerText;
	std::string footerIconUrl;
	std::string thumbnailUrl;
	std::string imageUrl;
	int color = 0;
	struct Field { std::string name; std::string value; bool inlineField = false; };
	std::vector<Field> fields;
};

struct CommandData
{
	std::string discordId;
	std::string guildId;
	std::string name;
	std::string description;
	std::string callback;
	NativePawnScript* callbackScript = nullptr;
	std::shared_ptr<std::atomic_bool> creationCancelled;
};

struct InteractionData
{
	std::string id;
	std::string token;
	std::string content;
	std::string channelId;
	std::string guildId;
	std::vector<cell> mentions;
	bool responded = false;
};

std::unordered_map<cell, EmbedData> g_embeds;
std::unordered_map<cell, CommandData> g_commands;
std::unordered_map<cell, InteractionData> g_interactions;
std::unordered_set<std::string> g_loggedCommandWarnings;
cell g_nextEmbedHandle = 1;
cell g_nextCommandHandle = 1;
cell g_nextInteractionHandle = 1;
cell g_createdMessageHandle = 0;
cell g_createdGuildChannelHandle = 0;
cell g_createdPrivateChannelHandle = 0;
cell g_createdGuildRoleHandle = 0;

DiscordBridgeComponent* component()
{
	return DiscordBridgeComponent::getInstance();
}

NativePawnScript* pawnScriptFor(AMX* amx)
{
	DiscordBridgeComponent* bridge = component();
	NativePawnComponent* pawn = bridge ? bridge->getPawnComponent() : nullptr;
	return pawn ? pawn->getScript(amx) : nullptr;
}

int pawnGetAddr(AMX* amx, cell address, cell** physicalAddress)
{
	NativePawnScript* script = pawnScriptFor(amx);
	return script ? script->GetAddr(address, physicalAddress) : AMX_ERR_INIT;
}

int pawnPushAddress(NativePawnScript& script, cell* physicalAddress)
{
	cell amxAddress = 0;
	if (script.MakeAddr(physicalAddress, &amxAddress) != AMX_ERR_NONE)
	{
		return AMX_ERR_GENERAL;
	}
	return script.Push(amxAddress);
}

void resetNativeHandles()
{
	g_channelHandleToId.clear();
	g_channelIdToHandle.clear();
	g_nextChannelHandle = 1;

	g_userHandleToId.clear();
	g_userIdToHandle.clear();
	g_nextUserHandle = 1;

	g_guildHandleToId.clear();
	g_guildIdToHandle.clear();
	g_nextGuildHandle = 1;

	g_messageHandleToId.clear();
	g_messageIdToHandle.clear();
	g_nextMessageHandle = 1;

	g_roleHandleToId.clear();
	g_roleIdToHandle.clear();
	g_nextRoleHandle = 1;

	g_emojiHandleToToken.clear();
	g_nextEmojiHandle = 1;

	g_embeds.clear();
	g_commands.clear();
	g_interactions.clear();
	g_loggedCommandWarnings.clear();
	g_nextEmbedHandle = 1;
	g_nextCommandHandle = 1;
	g_nextInteractionHandle = 1;
	g_createdMessageHandle = 0;
	g_createdGuildChannelHandle = 0;
	g_createdPrivateChannelHandle = 0;
	g_createdGuildRoleHandle = 0;
}

cell assignChannelHandle(StringView channelId)
{
	const std::string id(channelId.data(), channelId.length());
	const auto it = g_channelIdToHandle.find(id);
	if (it != g_channelIdToHandle.end())
	{
		return it->second;
	}

	const cell handle = g_nextChannelHandle++;
	g_channelHandleToId.emplace(handle, id);
	g_channelIdToHandle.emplace(id, handle);
	return handle;
}

cell assignUserHandle(StringView userId)
{
	const std::string id(userId.data(), userId.length());
	const auto it = g_userIdToHandle.find(id);
	if (it != g_userIdToHandle.end())
	{
		return it->second;
	}

	const cell handle = g_nextUserHandle++;
	g_userHandleToId.emplace(handle, id);
	g_userIdToHandle.emplace(id, handle);
	return handle;
}

cell assignGuildHandle(StringView guildId)
{
	const std::string id(guildId.data(), guildId.length());
	const auto it = g_guildIdToHandle.find(id);
	if (it != g_guildIdToHandle.end())
	{
		return it->second;
	}

	const cell handle = g_nextGuildHandle++;
	g_guildHandleToId.emplace(handle, id);
	g_guildIdToHandle.emplace(id, handle);
	return handle;
}

cell assignMessageHandle(StringView messageId)
{
	const std::string id(messageId.data(), messageId.length());
	const auto it = g_messageIdToHandle.find(id);
	if (it != g_messageIdToHandle.end())
	{
		return it->second;
	}

	const cell handle = g_nextMessageHandle++;
	g_messageHandleToId.emplace(handle, id);
	g_messageIdToHandle.emplace(id, handle);
	return handle;
}

cell assignRoleHandle(StringView roleId)
{
	const std::string id(roleId.data(), roleId.length());
	const auto it = g_roleIdToHandle.find(id);
	if (it != g_roleIdToHandle.end())
	{
		return it->second;
	}

	const cell handle = g_nextRoleHandle++;
	g_roleHandleToId.emplace(handle, id);
	g_roleIdToHandle.emplace(id, handle);
	return handle;
}

cell assignEmojiHandle(const std::string& emojiToken)
{
	for (const auto& entry : g_emojiHandleToToken)
	{
		if (entry.second == emojiToken)
		{
			return entry.first;
		}
	}

	const cell handle = g_nextEmojiHandle++;
	g_emojiHandleToToken.emplace(handle, emojiToken);
	return handle;
}

cell assignCommandHandle(CommandData command)
{
	const cell handle = g_nextCommandHandle++;
	g_commands.emplace(handle, std::move(command));
	return handle;
}

void cacheApplicationCommands(StringView guildId, const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_array()) return;
	const std::string guild(guildId.data(), guildId.length());
	for (const auto& item : data)
	{
		if (!item.is_object()) continue;
		const std::string id = item.value("id", std::string());
		const std::string name = item.value("name", std::string());
		if (id.empty() || name.empty()) continue;

		bool found = false;
		for (auto& entry : g_commands)
		{
			CommandData& command = entry.second;
			if (command.guildId == guild && command.name == name)
			{
				command.discordId = id;
				if (command.description.empty()) command.description = item.value("description", std::string());
				found = true;
				break;
			}
		}
		if (!found)
		{
			CommandData command;
			command.discordId = id;
			command.guildId = guild;
			command.name = name;
			command.description = item.value("description", std::string());
			command.creationCancelled = std::make_shared<std::atomic_bool>(false);
			assignCommandHandle(std::move(command));
		}
	}
}

cell assignInteractionHandle(InteractionData interaction)
{
	const cell handle = g_nextInteractionHandle++;
	g_interactions.emplace(handle, std::move(interaction));
	return handle;
}

void collectInteractionTextMentions(InteractionData& interaction)
{
	if (interaction.guildId.empty() || interaction.content.empty()) return;

	for (size_t position = 0; position < interaction.content.size();)
	{
		const size_t marker = interaction.content.find("<@", position);
		if (marker == std::string::npos) break;
		size_t cursor = marker + 2;
		if (cursor < interaction.content.size() && interaction.content[cursor] == '!') ++cursor;
		const size_t idStart = cursor;
		while (cursor < interaction.content.size() && std::isdigit(static_cast<unsigned char>(interaction.content[cursor]))) ++cursor;
		if (cursor > idStart && cursor < interaction.content.size() && interaction.content[cursor] == '>')
		{
			const std::string userId = interaction.content.substr(idStart, cursor - idStart);
			if (component()->findUserById(userId)) interaction.mentions.push_back(assignUserHandle(userId));
		}
		position = marker + 2;
	}
}

std::string getAmxString(AMX* amx, cell amxParam)
{
	NativePawnScript* script = pawnScriptFor(amx);
	if (!script)
	{
		return {};
	}

	cell* addr = nullptr;
	if (script->GetAddr(amxParam, &addr) != AMX_ERR_NONE || !addr)
	{
		return {};
	}

	int len = 0;
	if (script->StrLen(addr, &len) != AMX_ERR_NONE || len < 0)
	{
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(len) + 1, '\0');
	if (script->GetString(buffer.data(), addr, false, buffer.size()) != AMX_ERR_NONE)
	{
		return {};
	}

	return std::string(buffer.data());
}

bool setAmxString(AMX* amx, cell amxParam, const std::string& value, size_t maxSize)
{
	NativePawnScript* script = pawnScriptFor(amx);
	if (!script)
	{
		return false;
	}

	cell* addr = nullptr;
	if (script->GetAddr(amxParam, &addr) != AMX_ERR_NONE || !addr)
	{
		return false;
	}

	return script->SetString(addr, StringView(value), false, false, maxSize) == AMX_ERR_NONE;
}

bool setAmxString(AMX* amx, cell amxParam, const std::string& value, cell maxSize)
{
	if (maxSize < 0) return false;
	return setAmxString(amx, amxParam, value, static_cast<size_t>(maxSize));
}

bool pawnScriptHasPublic(NativePawnScript* script, const char* name)
{
	if (!script) return false;
	int publicIndex = -1;
	return script->FindPublic(name, &publicIndex) == AMX_ERR_NONE &&
		publicIndex >= 0 && publicIndex != INT_MAX;
}

NativePawnScript* findPawnScriptWithPublic(const char* name, NativePawnScript* preferred = nullptr)
{
	DiscordBridgeComponent* bridge = component();
	if (!bridge || !bridge->getPawnComponent()) return nullptr;
	NativePawnComponent* pawn = bridge->getPawnComponent();

	// Do not dereference a cached script pointer until it has been verified
	// against the current script set. This keeps command registrations safe
	// when a filterscript is unloaded and another one reuses its address.
	if (preferred)
	{
		if (pawn->mainScript() == preferred && pawnScriptHasPublic(preferred, name)) return preferred;
		for (NativePawnScript* script : pawn->sideScripts())
		{
			if (script == preferred && pawnScriptHasPublic(preferred, name)) return preferred;
		}
	}

	// The legacy connector's CallFirst semantics stop at the first AMX that
	// actually exports the public. Calling every script duplicates callbacks
	// when a side script happens to contain a public with the same name.
	NativePawnScript* main = pawn->mainScript();
	if (pawnScriptHasPublic(main, name)) return main;
	for (NativePawnScript* script : pawn->sideScripts())
	{
		if (pawnScriptHasPublic(script, name)) return script;
	}
	return nullptr;
}

template <typename... Args>
bool callPawnPublicOnScript(NativePawnScript* script, const char* name, Args... args)
{
	if (!script) return false;
	int publicIndex = -1;
	if (script->FindPublic(name, &publicIndex) != AMX_ERR_NONE || publicIndex < 0 || publicIndex == INT_MAX)
	{
		return false;
	}

	cell result = DefaultReturnValue_True;
	const int error = script->CallChecked(publicIndex, result, args...);
	if (error != AMX_ERR_NONE) script->PrintError(error);
	return true;
}

template <typename... Args>
bool callPawnPublic(const char* name, Args... args)
{
	NativePawnScript* script = findPawnScriptWithPublic(name);
	return script && callPawnPublicOnScript(script, name, args...);
}

template <typename... Args>
bool callPawnPublicFromScript(const char* name, NativePawnScript* preferred, Args... args)
{
	NativePawnScript* script = findPawnScriptWithPublic(name, preferred);
	return script && callPawnPublicOnScript(script, name, args...);
}

void logCommandWarning(const std::string& message)
{
	DiscordBridgeComponent* bridge = component();
	DiscordLogWarning(bridge ? bridge->getCore() : nullptr, message);
}

void logCommandWarningOnce(const std::string& key, const std::string& message)
{
	if (g_loggedCommandWarnings.emplace(key).second)
	{
		logCommandWarning(message);
	}
}

void logCommandInfo(const std::string& message)
{
	DiscordBridgeComponent* bridge = component();
	DiscordLogMessage(bridge ? bridge->getCore() : nullptr, message);
}

void logCommandResponseFailure(DiscordBot* bot, const char* operation, const std::string& name,
	const DiscordHTTP::Response& response)
{
	std::string detail = response.body.empty() ? "network or TLS failure" : response.body;
	for (char& character : detail)
	{
		if (character == '\r' || character == '\n') character = ' ';
	}
	if (detail.size() > 180) detail.resize(180);
	if (!bot || !bot->getComponent()) return;
	DiscordLogWarning(bot->getComponent()->getCore(),
		std::string("[DiscordBridge] Discord command '") + name + "' " + operation +
		" failed (HTTP " + std::to_string(response.statusCode) + "): " + detail);
}

struct PawnCallbackArg
{
	enum class Type { Cell, String, Array, Reference };
	Type type = Type::Cell;
	cell value = 0;
	cell referenceAddress = 0;
	std::string text;
	std::vector<cell> array;
};

struct PreparedPawnCallback
{
	int scriptId = -1;
	std::string name;
	std::vector<PawnCallbackArg> args;
};

constexpr size_t MAX_CALLBACK_ARRAY_CELLS = 4096;

NativePawnScript* pawnScriptForId(int scriptId)
{
	DiscordBridgeComponent* bridge = component();
	NativePawnComponent* pawn = bridge ? bridge->getPawnComponent() : nullptr;
	if (!pawn) return nullptr;

	NativePawnScript* main = pawn->mainScript();
	if (main && main->IsLoaded() && main->GetID() == scriptId) return main;
	for (NativePawnScript* script : pawn->sideScripts())
	{
		if (script && script->IsLoaded() && script->GetID() == scriptId) return script;
	}
	return nullptr;
}

bool pawnArrayRangeValid(NativePawnScript& script, cell address, size_t cells)
{
	if (address < 0 || cells == 0 || cells > MAX_CALLBACK_ARRAY_CELLS) return false;
	const uint64_t end = static_cast<uint64_t>(static_cast<uint32_t>(address)) + cells;
	return end <= static_cast<uint64_t>(script.GetSTP());
}

bool executePawnCallback(const PreparedPawnCallback& prepared)
{
	if (prepared.name.empty()) return true;
	NativePawnScript* script = pawnScriptForId(prepared.scriptId);
	if (!script) return false;

	int publicIndex = -1;
	if (script->FindPublic(prepared.name.c_str(), &publicIndex) != AMX_ERR_NONE || publicIndex < 0)
	{
		return false;
	}

	const cell heap = script->GetHEA();
	for (auto it = prepared.args.rbegin(); it != prepared.args.rend(); ++it)
	{
		int error = AMX_ERR_NONE;
		switch (it->type)
		{
			case PawnCallbackArg::Type::Cell:
				error = script->Push(it->value);
				break;
			case PawnCallbackArg::Type::String:
				error = script->PushString(nullptr, nullptr, StringView(it->text), false, false);
				break;
			case PawnCallbackArg::Type::Array:
				error = script->PushArray(nullptr, nullptr, it->array.data(), static_cast<int>(it->array.size()));
				break;
			case PawnCallbackArg::Type::Reference:
			{
				cell* reference = nullptr;
				if (script->GetAddr(it->referenceAddress, &reference) != AMX_ERR_NONE || !reference)
				{
					error = AMX_ERR_MEMACCESS;
				}
				else
				{
					error = pawnPushAddress(*script, reference);
				}
				break;
			}
		}
		if (error != AMX_ERR_NONE)
		{
			script->Release(heap);
			return false;
		}
	}

	cell result = 0;
	const int error = script->Exec(&result, publicIndex);
	script->Release(heap);
	return error == AMX_ERR_NONE;
}

bool callbackParametersValid(AMX* amx, cell callbackParam, cell formatParam, cell* params, size_t firstParam)
{
	const std::string callback = getAmxString(amx, callbackParam);
	// An empty callback is explicitly supported by the legacy API.  Its format
	// and variadic arguments are ignored in that case.
	if (callback.empty()) return true;
	if (callback.size() > 31) return false;

	DiscordBridgeComponent* bridge = component();
	NativePawnComponent* pawn = bridge ? bridge->getPawnComponent() : nullptr;
	NativePawnScript* script = pawn ? pawn->getScript(amx) : nullptr;
	if (!script) return false;
	int publicIndex = -1;
	if (script->FindPublic(callback.c_str(), &publicIndex) != AMX_ERR_NONE || publicIndex < 0) return false;

	const std::string format = getAmxString(amx, formatParam);
	if (params[0] < 0 || params[0] % static_cast<cell>(sizeof(cell)) != 0) return false;
	const size_t supplied = params[0] > 0 ? static_cast<size_t>(params[0]) / sizeof(cell) : 0;
	if (firstParam == 0 || firstParam - 1 > supplied || format.size() != supplied - (firstParam - 1)) return false;

	size_t pendingArray = static_cast<size_t>(-1);
	for (size_t i = 0; i < format.size(); ++i)
	{
		const char kind = format[i];
		const cell parameter = params[firstParam + i];
		if (kind == 'd' || kind == 'i' || kind == 'f' || kind == 'b')
		{
			if (pendingArray != static_cast<size_t>(-1))
			{
				if (parameter <= 0 || static_cast<size_t>(parameter) > MAX_CALLBACK_ARRAY_CELLS) return false;
				NativePawnScript* arrayScript = pawnScriptFor(amx);
				if (!arrayScript || !pawnArrayRangeValid(*arrayScript, params[firstParam + i - 1], static_cast<size_t>(parameter))) return false;
				cell* arrayAddress = nullptr;
				if (pawnGetAddr(amx, params[firstParam + i - 1], &arrayAddress) != AMX_ERR_NONE || !arrayAddress) return false;
				pendingArray = static_cast<size_t>(-1);
			}
			continue;
		}
		if (kind == 's')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			cell* stringAddress = nullptr;
			if (pawnGetAddr(amx, parameter, &stringAddress) != AMX_ERR_NONE || !stringAddress) return false;
			continue;
		}
		if (kind == 'a')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			NativePawnScript* arrayScript = pawnScriptFor(amx);
			if (!arrayScript || !pawnArrayRangeValid(*arrayScript, parameter, 1)) return false;
			cell* arrayAddress = nullptr;
			if (pawnGetAddr(amx, parameter, &arrayAddress) != AMX_ERR_NONE || !arrayAddress) return false;
			pendingArray = i;
			continue;
		}
		if (kind == 'r')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			NativePawnScript* referenceScript = pawnScriptFor(amx);
			// A reference into the active stack belongs to the native caller and
			// may be gone by the time the REST completion runs.  Only retain
			// references in static/data memory; their AMX address is re-resolved
			// when the callback executes.
			if (!referenceScript || parameter < 0 || parameter >= referenceScript->GetHEA()
				|| !pawnArrayRangeValid(*referenceScript, parameter, 1)) return false;
			cell* reference = nullptr;
			if (pawnGetAddr(amx, parameter, &reference) != AMX_ERR_NONE || !reference) return false;
			continue;
		}
		return false;
	}

	return pendingArray == static_cast<size_t>(-1);
}

bool capturePawnCallback(AMX* amx, cell callbackParam, cell formatParam, cell* params, size_t firstParam,
	std::shared_ptr<PreparedPawnCallback>& prepared)
{
	prepared.reset();
	if (!callbackParametersValid(amx, callbackParam, formatParam, params, firstParam)) return false;

	const std::string callback = getAmxString(amx, callbackParam);
	if (callback.empty()) return true;
	const std::string format = getAmxString(amx, formatParam);
	if (params[0] < 0 || params[0] % static_cast<cell>(sizeof(cell)) != 0) return false;
	const size_t supplied = params[0] > 0 ? static_cast<size_t>(params[0]) / sizeof(cell) : 0;
	if (firstParam == 0 || firstParam - 1 > supplied || format.size() != supplied - (firstParam - 1)) return false;

	prepared = std::make_shared<PreparedPawnCallback>();
	NativePawnScript* script = pawnScriptFor(amx);
	if (!script) return false;
	prepared->scriptId = script->GetID();
	prepared->name = callback;
	prepared->args.reserve(format.size());

	size_t pendingArray = static_cast<size_t>(-1);
	for (size_t i = 0; i < format.size(); ++i)
	{
		const char kind = format[i];
		const cell parameter = params[firstParam + i];
		if (kind == 'd' || kind == 'i' || kind == 'f' || kind == 'b')
		{
			PawnCallbackArg arg;
			arg.value = parameter;
			if (pendingArray != static_cast<size_t>(-1))
			{
				if (parameter <= 0 || static_cast<size_t>(parameter) > MAX_CALLBACK_ARRAY_CELLS) return false;
				if (!pawnArrayRangeValid(*script, params[firstParam + i - 1], static_cast<size_t>(parameter))) return false;
				cell* arrayAddress = nullptr;
				if (pawnGetAddr(amx, params[firstParam + i - 1], &arrayAddress) != AMX_ERR_NONE || !arrayAddress) return false;
				prepared->args[pendingArray].array.assign(arrayAddress, arrayAddress + parameter);
				pendingArray = static_cast<size_t>(-1);
			}
			prepared->args.push_back(std::move(arg));
			continue;
		}
		if (kind == 's')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			PawnCallbackArg arg;
			arg.type = PawnCallbackArg::Type::String;
			arg.text = getAmxString(amx, parameter);
			prepared->args.push_back(std::move(arg));
			continue;
		}
		if (kind == 'a')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			if (!pawnArrayRangeValid(*script, parameter, 1)) return false;
			cell* arrayAddress = nullptr;
			if (pawnGetAddr(amx, parameter, &arrayAddress) != AMX_ERR_NONE || !arrayAddress) return false;
			PawnCallbackArg arg;
			arg.type = PawnCallbackArg::Type::Array;
			prepared->args.push_back(std::move(arg));
			pendingArray = prepared->args.size() - 1;
			continue;
		}
		if (kind == 'r')
		{
			if (pendingArray != static_cast<size_t>(-1)) return false;
			if (parameter < 0 || parameter >= script->GetHEA() || !pawnArrayRangeValid(*script, parameter, 1)) return false;
			cell* reference = nullptr;
			if (pawnGetAddr(amx, parameter, &reference) != AMX_ERR_NONE || !reference) return false;
			PawnCallbackArg arg;
			arg.type = PawnCallbackArg::Type::Reference;
			arg.referenceAddress = parameter;
			prepared->args.push_back(std::move(arg));
			continue;
		}
		return false;
	}

	return pendingArray == static_cast<size_t>(-1);
}

DiscordJson embedToJson(const EmbedData& embed)
{
	DiscordJson result = DiscordJson::object();
	if (!embed.title.empty()) result["title"] = embed.title;
	if (!embed.description.empty()) result["description"] = embed.description;
	if (!embed.url.empty()) result["url"] = embed.url;
	if (!embed.timestamp.empty()) result["timestamp"] = embed.timestamp;
	if (embed.color != 0) result["color"] = embed.color;
	if (!embed.footerText.empty())
	{
		result["footer"] = { { "text", embed.footerText } };
		if (!embed.footerIconUrl.empty()) result["footer"]["icon_url"] = embed.footerIconUrl;
	}
	if (!embed.thumbnailUrl.empty()) result["thumbnail"] = { { "url", embed.thumbnailUrl } };
	if (!embed.imageUrl.empty()) result["image"] = { { "url", embed.imageUrl } };
	if (!embed.fields.empty())
	{
		result["fields"] = DiscordJson::array();
		for (const auto& field : embed.fields)
		{
			result["fields"].push_back({
				{ "name", field.name },
				{ "value", field.value },
				{ "inline", field.inlineField }
			});
		}
	}
	return result;
}

std::string messagePayload(const std::string& content, const EmbedData* embed)
{
	DiscordJson body = DiscordJson::object();
	if (!content.empty()) body["content"] = content;
	if (embed) body["embeds"] = DiscordJson::array({ embedToJson(*embed) });
	return body.dump();
}

void completeMessageResponse(const std::string& responseBody,
	const std::shared_ptr<PreparedPawnCallback>& callback, bool cacheWithoutCallback)
{
	if (!callback && !cacheWithoutCallback) return;
	DiscordBridgeComponent* bridge = component();
	if (!bridge) return;
	DiscordMessage* created = bridge->upsertMessageFromJson(responseBody);
	if (!created) return;

	const std::string createdMessageId(created->getMessageId().data(), created->getMessageId().length());
	if (callback)
	{
		// DCC_GetCreatedMessage() is only valid while the callback is running.
		// Cleanup must happen after the callback; deleting the temporary message
		// first makes DCC_CacheChannelMessage callbacks observe an invalid handle.
		g_createdMessageHandle = assignMessageHandle(created->getMessageId());
		executePawnCallback(*callback);
	}
	if (auto* current = static_cast<DiscordMessage*>(bridge->findMessageById(createdMessageId));
		current && !current->isPersistent())
	{
		bridge->removeMessage(createdMessageId);
	}
	if (callback) g_createdMessageHandle = 0;
}

DiscordChannel* resolveChannelByHandle(cell handle)
{
	auto it = g_channelHandleToId.find(handle);
	if (it == g_channelHandleToId.end())
	{
		return nullptr;
	}

	auto* channel = component()->findChannelById(it->second);
	return static_cast<DiscordChannel*>(channel);
}

std::string channelIdForHandle(cell handle)
{
	const auto it = g_channelHandleToId.find(handle);
	return it == g_channelHandleToId.end() ? std::string() : it->second;
}

bool isDiscordSnowflake(const std::string& value)
{
	if (value.size() < 17 || value.size() > 20) return false;
	return std::all_of(value.begin(), value.end(), [](unsigned char character)
	{
		return std::isdigit(character) != 0;
	});
}

DiscordUser* resolveUserByHandle(cell handle)
{
	auto it = g_userHandleToId.find(handle);
	if (it == g_userHandleToId.end())
	{
		return nullptr;
	}

	auto* user = component()->findUserById(it->second);
	return static_cast<DiscordUser*>(user);
}

DiscordGuild* resolveGuildByHandle(cell handle)
{
	auto it = g_guildHandleToId.find(handle);
	if (it == g_guildHandleToId.end())
	{
		return nullptr;
	}

	auto* guild = component()->findGuildById(it->second);
	return static_cast<DiscordGuild*>(guild);
}

DiscordMessage* resolveMessageByHandle(cell handle)
{
	auto it = g_messageHandleToId.find(handle);
	if (it == g_messageHandleToId.end())
	{
		return nullptr;
	}

	auto* message = component()->findMessageById(it->second);
	return static_cast<DiscordMessage*>(message);
}

DiscordRole* resolveRoleByHandle(cell handle)
{
	auto it = g_roleHandleToId.find(handle);
	if (it == g_roleHandleToId.end())
	{
		return nullptr;
	}

	return component()->findRoleByIdInternal(it->second);
}

std::string resolveEmojiToken(cell handle)
{
	const auto it = g_emojiHandleToToken.find(handle);
	return it == g_emojiHandleToToken.end() ? std::string() : it->second;
}

cell AMX_NATIVE_CALL Native_InvalidRegistration(AMX*, cell*)
{
	return 0;
}

cell AMX_NATIVE_CALL Native_ConnectDiscordBot(AMX* amx, cell* params)
{
	if (params[0] < static_cast<cell>(2 * sizeof(cell)))
	{
		return 0;
	}

	const std::string token = getAmxString(amx, params[1]);
	const int intents = (params[0] >= static_cast<cell>(3 * sizeof(cell))) ? static_cast<int>(params[2]) : DCC_DEFAULT_INTENTS;
	if (token.empty())
	{
		return 0;
	}

	return component()->connectBot(token, intents) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_IsDiscordConnected(AMX*, cell*)
{
	auto* bot = component()->getBot();
	return (bot && bot->isConnected()) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindChannelById(AMX* amx, cell* params)
{
	const std::string channelId = getAmxString(amx, params[1]);
	// Preserve the old connector's stable-handle behavior for scripts that
	// resolve IDs during OnGameModeInit. The actual channel object may arrive
	// later through GUILD_CREATE, but the ID handle is already safe to queue
	// outbound work against.
	return isDiscordSnowflake(channelId) ? assignChannelHandle(channelId) : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindChannelByName(AMX* amx, cell* params)
{
	const std::string name = getAmxString(amx, params[1]);
	auto* channel = static_cast<DiscordChannel*>(component()->findChannelByName(name));
	if (!channel)
	{
		return 0;
	}

	return assignChannelHandle(channel->getChannelId());
}

cell AMX_NATIVE_CALL Native_FindDiscordConfiguredChannel(AMX*, cell*)
{
	if (component())
	{
		const StringView configuredId = component()->configuredChannelId();
		if (!configuredId.empty())
		{
			const std::string id(configuredId.data(), configuredId.length());
			return isDiscordSnowflake(id) ? assignChannelHandle(id) : 0;
		}
	}

	auto* channel = static_cast<DiscordChannel*>(component()->findConfiguredChannel());
	if (!channel)
	{
		return 0;
	}

	return assignChannelHandle(channel->getChannelId());
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelId(AMX* amx, cell* params)
{
	if (params[0] < static_cast<cell>(3 * sizeof(cell)))
	{
		return 0;
	}

	const std::string channelId = channelIdForHandle(params[1]);
	if (channelId.empty())
	{
		return 0;
	}

	if (params[3] < 0) return 0;
	size_t maxSize = static_cast<size_t>(params[3]);
	if (maxSize == 0)
	{
		maxSize = 21;
	}

	return setAmxString(amx, params[2], channelId, maxSize) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelName(AMX* amx, cell* params)
{
	if (params[0] < static_cast<cell>(3 * sizeof(cell)))
	{
		return 0;
	}

	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	if (!channel)
	{
		return 0;
	}

	if (params[3] < 0) return 0;
	size_t maxSize = static_cast<size_t>(params[3]);
	const std::string name(channel->getChannelName().data(), channel->getChannelName().length());
	return setAmxString(amx, params[2], name, maxSize) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelTopic(AMX* amx, cell* params)
{
	if (params[0] < static_cast<cell>(3 * sizeof(cell)))
	{
		return 0;
	}

	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	if (!channel)
	{
		return 0;
	}

	if (params[3] < 0) return 0;
	size_t maxSize = static_cast<size_t>(params[3]);
	const std::string topic(channel->getTopic().data(), channel->getTopic().length());
	return setAmxString(amx, params[2], topic, maxSize) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelType(AMX* amx, cell* params)
{
	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	if (!channel)
	{
		return 0;
	}

	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}

	*out = static_cast<cell>(channel->getChannelType());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SendChannelMessage(AMX* amx, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	if (channelId.empty())
	{
		return 0;
	}

	const std::string message = getAmxString(amx, params[2]);
	if (message.size() > 2000)
	{
		return 0;
	}
	const size_t supplied = params[0] > 0 ? static_cast<size_t>(params[0]) / sizeof(cell) : 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	// The friendly SendDiscordChannelMessage native has only two parameters.
	// Do not inspect the optional DCC callback slots unless the caller actually
	// supplied the legacy callback/format arguments.
	if (supplied > 2)
	{
		if (supplied < 4 || !capturePawnCallback(amx, params[3], params[4], params, 5, callback)) return 0;
	}
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	if (!bot->submitRestTask([bot, channelId, message, callback](DiscordHTTP& http) mutable
	{
		const auto response = http.sendMessage(channelId, message);
		if (!response.success || !callback) return;
		bot->enqueueCompletion([response, callback]()
		{
			completeMessageResponse(response.body, callback, false);
		});
	})) return 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetChannelName(AMX* amx, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	const std::string name = getAmxString(amx, params[2]);
	if (name.size() < 2 || name.size() > 100)
	{
		return 0;
	}

	DiscordBot* bot = nativeBot();
	if (channelId.empty() || !bot) return 0;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("name", name) + "}";
	return bot->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetChannelTopic(AMX* amx, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	const std::string topic = getAmxString(amx, params[2]);
	if (topic.size() > 1024)
	{
		return 0;
	}

	DiscordBot* bot = nativeBot();
	if (channelId.empty() || !bot) return 0;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("topic", topic) + "}";
	return bot->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteChannel(AMX* amx, cell* params)
{
	(void)amx;
	const std::string channelId = channelIdForHandle(params[1]);
	DiscordBot* bot = nativeBot();
	if (channelId.empty() || !bot) return 0;
	return bot->submitRestTask([channelId](DiscordHTTP& http)
	{
		http.deleteChannel(channelId);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindUserById(AMX* amx, cell* params)
{
	const std::string userId = getAmxString(amx, params[1]);
	return isDiscordSnowflake(userId) ? assignUserHandle(userId) : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindUserByName(AMX* amx, cell* params)
{
	const std::string name = getAmxString(amx, params[1]);
	const std::string disc = getAmxString(amx, params[2]);

	auto* user = component()->findUserByNameAndDiscriminator(name, disc);
	if (!user)
	{
		return 0;
	}

	return assignUserHandle(user->getUserId());
}

cell AMX_NATIVE_CALL Native_DCC_GetUserId(AMX* amx, cell* params)
{
	const auto it = g_userHandleToId.find(params[1]);
	if (it == g_userHandleToId.end())
	{
		return 0;
	}
	return setAmxString(amx, params[2], it->second, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetUserName(AMX* amx, cell* params)
{
	DiscordUser* user = resolveUserByHandle(params[1]);
	if (!user)
	{
		return 0;
	}
	const std::string name(user->getUsername().data(), user->getUsername().length());
	return setAmxString(amx, params[2], name, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetUserDiscriminator(AMX* amx, cell* params)
{
	DiscordUser* user = resolveUserByHandle(params[1]);
	if (!user)
	{
		return 0;
	}
	const std::string disc(user->getDiscriminator().data(), user->getDiscriminator().length());
	return setAmxString(amx, params[2], disc, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_IsUserBot(AMX* amx, cell* params)
{
	DiscordUser* user = resolveUserByHandle(params[1]);
	if (!user)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = user->isBot() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_FindGuildById(AMX* amx, cell* params)
{
	const std::string guildId = getAmxString(amx, params[1]);
	return isDiscordSnowflake(guildId) ? assignGuildHandle(guildId) : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindGuildByName(AMX* amx, cell* params)
{
	const std::string name = getAmxString(amx, params[1]);
	auto* guild = static_cast<DiscordGuild*>(component()->findGuildByName(name));
	if (!guild)
	{
		return 0;
	}
	return assignGuildHandle(guild->getGuildId());
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildId(AMX* amx, cell* params)
{
	const auto it = g_guildHandleToId.find(params[1]);
	if (it == g_guildHandleToId.end())
	{
		return 0;
	}
	return setAmxString(amx, params[2], it->second, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildName(AMX* amx, cell* params)
{
	DiscordGuild* guild = resolveGuildByHandle(params[1]);
	if (!guild)
	{
		return 0;
	}
	const std::string name(guild->getGuildName().data(), guild->getGuildName().length());
	return setAmxString(amx, params[2], name, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildOwnerId(AMX* amx, cell* params)
{
	DiscordGuild* guild = resolveGuildByHandle(params[1]);
	if (!guild)
	{
		return 0;
	}
	const std::string owner(guild->getOwnerId().data(), guild->getOwnerId().length());
	return setAmxString(amx, params[2], owner, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CacheChannelMessage(AMX* amx, cell* params)
{
	const std::string channelId = getAmxString(amx, params[1]);
	const std::string messageId = getAmxString(amx, params[2]);
	if (channelId.empty() || messageId.empty()) return 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	if (!capturePawnCallback(amx, params[3], params[4], params, 5, callback)) return 0;
	if (component()->findMessageById(messageId)) return 0;
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	if (!bot->submitRestTask([bot, channelId, messageId, callback](DiscordHTTP& http)
	{
		const auto response = http.getMessage(channelId, messageId);
		if (!response.success) return;
		bot->enqueueCompletion([response, callback]()
		{
			completeMessageResponse(response.body, callback, true);
		});
	})) return 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageId(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	const std::string id(message->getMessageId().data(), message->getMessageId().length());
	return setAmxString(amx, params[2], id, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageChannel(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}

	const std::string channelId(message->getChannelId().data(), message->getChannelId().length());
	*out = isDiscordSnowflake(channelId) ? assignChannelHandle(channelId) : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageAuthor(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}

	auto* user = static_cast<DiscordUser*>(component()->findUserById(message->getAuthorId()));
	if (!user)
	{
		*out = 0;
		return 1;
	}

	*out = assignUserHandle(user->getUserId());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageContent(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	const std::string content(message->getContent().data(), message->getContent().length());
	return setAmxString(amx, params[2], content, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_IsMessageTts(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = message->isTTS() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_IsMessageMentioningEveryone(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = message->mentionsEveryone() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteMessage(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	return message->deleteMessage() ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_EditMessage(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	const std::string content = getAmxString(amx, params[2]);
	if (content.size() > 2000) return 0;
	if (params[3] == 0) return message->editMessage(content) ? 1 : 0;
	const auto embedIt = g_embeds.find(params[3]);
	if (embedIt == g_embeds.end()) return 0;
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string channelId(message->getChannelId().data(), message->getChannelId().length());
	const std::string messageId(message->getMessageId().data(), message->getMessageId().length());
	const std::string body = messagePayload(content, &embedIt->second);
	const bool queued = bot->submitRestTask([channelId, messageId, body](DiscordHTTP& http)
	{
		http.editMessagePayload(channelId, messageId, body);
	});
	if (queued) g_embeds.erase(embedIt);
	return queued ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindRoleById(AMX* amx, cell* params)
{
	const std::string roleId = getAmxString(amx, params[1]);
	return isDiscordSnowflake(roleId) ? assignRoleHandle(roleId) : 0;
}

cell AMX_NATIVE_CALL Native_DCC_FindRoleByName(AMX* amx, cell* params)
{
	const cell guildHandle = params[1];
	const std::string roleName = getAmxString(amx, params[2]);
	if (roleName.empty())
	{
		return 0;
	}

	std::string guildId;
	auto guildIt = g_guildHandleToId.find(guildHandle);
	if (guildIt != g_guildHandleToId.end())
	{
		guildId = guildIt->second;
	}

	DiscordRole* role = component()->findRoleByNameInternal(roleName, guildId);
	if (!role)
	{
		return 0;
	}
	return assignRoleHandle(role->getRoleId());
}

cell AMX_NATIVE_CALL Native_DCC_GetRoleId(AMX* amx, cell* params)
{
	const auto it = g_roleHandleToId.find(params[1]);
	if (it == g_roleHandleToId.end())
	{
		return 0;
	}
	return setAmxString(amx, params[2], it->second, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetRoleName(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	if (!role)
	{
		return 0;
	}
	const std::string name(role->getRoleName().data(), role->getRoleName().length());
	return setAmxString(amx, params[2], name, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetRoleColor(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	if (!role)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = static_cast<cell>(role->getColor());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetRolePermissions(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	cell* high = nullptr;
	cell* low = nullptr;
	if (pawnGetAddr(amx, params[2], &high) != AMX_ERR_NONE || !high)
	{
		return 0;
	}
	if (pawnGetAddr(amx, params[3], &low) != AMX_ERR_NONE || !low)
	{
		return 0;
	}
	if (!role) return 0;
	const uint64_t permissions = role->getPermissions();
	*high = static_cast<cell>((permissions >> 32U) & 0xFFFFFFFFULL);
	*low = static_cast<cell>(permissions & 0xFFFFFFFFULL);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_IsRoleHoist(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	if (!role)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = role->isHoisted() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetRolePosition(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	if (!role)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = static_cast<cell>(role->getPosition());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_IsRoleMentionable(AMX* amx, cell* params)
{
	DiscordRole* role = resolveRoleByHandle(params[1]);
	if (!role)
	{
		return 0;
	}
	cell* out = nullptr;
	if (pawnGetAddr(amx, params[2], &out) != AMX_ERR_NONE || !out)
	{
		return 0;
	}
	*out = role->isMentionable() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_CreateEmoji(AMX* amx, cell* params)
{
	const std::string name = getAmxString(amx, params[1]);
	const std::string snowflake = getAmxString(amx, params[2]);
	if (name.empty())
	{
		return 0;
	}
	return CreateDiscordEmojiHandle(name, snowflake);
}

cell AMX_NATIVE_CALL Native_DCC_DeleteEmoji(AMX* amx, cell* params)
{
	const cell handle = params[1];
	return g_emojiHandleToToken.erase(handle) > 0 ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetEmojiName(AMX* amx, cell* params)
{
	const std::string token = resolveEmojiToken(params[1]);
	if (token.empty())
	{
		return 0;
	}
	const size_t pos = token.find(':');
	const std::string name = pos == std::string::npos ? token : token.substr(0, pos);
	if (!setAmxString(amx, params[2], name, params[3])) return -1;
	return static_cast<cell>(name.length());
}

cell AMX_NATIVE_CALL Native_DCC_CreateReaction(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	const std::string token = resolveEmojiToken(params[2]);
	if (token.empty())
	{
		return 0;
	}
	const bool success = message->addReaction(token);
	// DCC_CreateReaction consumes the temporary emoji handle just like the
	// legacy connector did.  Keeping it alive makes later callbacks observe a
	// handle that should already be invalid.
	DeleteDiscordEmojiHandle(params[2]);
	return success ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteMessageReaction(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message)
	{
		return 0;
	}
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;

	const std::string channelId(message->getChannelId().data(), message->getChannelId().length());
	const std::string messageId(message->getMessageId().data(), message->getMessageId().length());
	const cell emojiHandle = params[2];
	if (emojiHandle == 0)
	{
		return bot->submitRestTask([channelId, messageId](DiscordHTTP& http)
		{
			http.deleteAllReactions(channelId, messageId);
		}) ? 1 : 0;
	}
	const std::string token = resolveEmojiToken(emojiHandle);
	if (token.empty())
	{
		return 0;
	}
	return bot->submitRestTask([channelId, messageId, token](DiscordHTTP& http)
	{
		http.deleteEmojiReactions(channelId, messageId, token);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_AddGuildMemberRole(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	auto roleIt = g_roleHandleToId.find(params[3]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end() || roleIt == g_roleHandleToId.end())
	{
		return 0;
	}

	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	const std::string roleId = roleIt->second;
	return bot->submitRestTask([guildId, userId, roleId](DiscordHTTP& http)
	{
		http.addGuildMemberRole(guildId, userId, roleId);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_RemoveGuildMemberRole(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	auto roleIt = g_roleHandleToId.find(params[3]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end() || roleIt == g_roleHandleToId.end())
	{
		return 0;
	}

	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	const std::string roleId = roleIt->second;
	return bot->submitRestTask([guildId, userId, roleId](DiscordHTTP& http)
	{
		http.removeGuildMemberRole(guildId, userId, roleId);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildMemberNickname(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end())
	{
		return 0;
	}
	const std::string nick = getAmxString(amx, params[3]);
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("nick", nick) + "}";

	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	return bot->submitRestTask([guildId, userId, body](DiscordHTTP& http)
	{
		http.modifyGuildMember(guildId, userId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildMemberVoiceChannel(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	auto channelIt = g_channelHandleToId.find(params[3]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end() || channelIt == g_channelHandleToId.end())
	{
		return 0;
	}
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("channel_id", channelIt->second) + "}";

	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	return bot->submitRestTask([guildId, userId, body](DiscordHTTP& http)
	{
		http.modifyGuildMember(guildId, userId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_RemoveGuildMember(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end())
	{
		return 0;
	}
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	return bot->submitRestTask([guildId, userId](DiscordHTTP& http)
	{
		http.removeGuildMember(guildId, userId);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CreateGuildMemberBan(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end())
	{
		return 0;
	}
	const std::string reason = getAmxString(amx, params[3]);
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	return bot->submitRestTask([guildId, userId, reason](DiscordHTTP& http)
	{
		http.createGuildMemberBan(guildId, userId, reason);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_RemoveGuildMemberBan(AMX* amx, cell* params)
{
	auto guildIt = g_guildHandleToId.find(params[1]);
	auto userIt = g_userHandleToId.find(params[2]);
	if (guildIt == g_guildHandleToId.end() || userIt == g_userHandleToId.end())
	{
		return 0;
	}
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string guildId = guildIt->second;
	const std::string userId = userIt->second;
	return bot->submitRestTask([guildId, userId](DiscordHTTP& http)
	{
		http.removeGuildMemberBan(guildId, userId);
	}) ? 1 : 0;
}

cell* nativeRef(AMX* amx, cell address)
{
	cell* out = nullptr;
	return pawnGetAddr(amx, address, &out) == AMX_ERR_NONE ? out : nullptr;
}

DiscordBot* nativeBot()
{
	if (!component() || !component()->getBot()) return nullptr;
	return static_cast<DiscordBot*>(component()->getBot());
}

DiscordGuild* guildForHandle(cell handle)
{
	return resolveGuildByHandle(handle);
}

DiscordRole* roleForHandle(cell handle)
{
	return resolveRoleByHandle(handle);
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelGuild(AMX* amx, cell* params)
{
	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!channel || !out) return 0;
	const std::string guildId(channel->getGuildId().data(), channel->getGuildId().length());
	*out = guildId.empty() ? 0 : assignGuildHandle(guildId);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelPosition(AMX* amx, cell* params)
{
	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!channel || !out) return 0;
	*out = static_cast<cell>(channel->getPosition());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_IsChannelNsfw(AMX* amx, cell* params)
{
	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!channel || !out) return 0;
	*out = channel->isNSFW() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetChannelParentCategory(AMX* amx, cell* params)
{
	DiscordChannel* channel = resolveChannelByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!channel || !out) return 0;
	const std::string parent(channel->getParentId().data(), channel->getParentId().length());
	*out = parent.empty() ? 0 : assignChannelHandle(parent);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetChannelPosition(AMX*, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	DiscordBot* bot = nativeBot();
	if (channelId.empty() || !bot) return 0;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("position", static_cast<int64_t>(params[2])) + "}";
	return bot->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetChannelNsfw(AMX*, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	DiscordBot* bot = nativeBot();
	if (channelId.empty() || !bot) return 0;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("nsfw", params[2] != 0) + "}";
	return bot->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetChannelParentCategory(AMX*, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	const std::string parentId = channelIdForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (channelId.empty() || parentId.empty() || !bot) return 0;
	if (auto* parent = resolveChannelByHandle(params[2]); parent && parent->getChannelType() != EDiscordChannelType::GuildCategory) return 0;
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("parent_id", parentId) + "}";
	return bot->submitRestTask([channelId, body](DiscordHTTP& http)
	{
		http.modifyChannel(channelId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetCreatedMessage(AMX*, cell*)
{
	return g_createdMessageHandle;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageUserMentionCount(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!message || !out) return 0;
	*out = static_cast<cell>(message->getUserMentionIds().size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageUserMention(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	cell* out = nativeRef(amx, params[3]);
	if (!message || !out || params[2] < 0 || static_cast<size_t>(params[2]) >= message->getUserMentionIds().size()) return 0;
	*out = assignUserHandle(message->getUserMentionIds()[static_cast<size_t>(params[2])]);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageRoleMentionCount(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!message || !out) return 0;
	*out = static_cast<cell>(message->getRoleMentionIds().size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetMessageRoleMention(AMX* amx, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	cell* out = nativeRef(amx, params[3]);
	if (!message || !out || params[2] < 0 || static_cast<size_t>(params[2]) >= message->getRoleMentionIds().size()) return 0;
	*out = assignRoleHandle(message->getRoleMentionIds()[static_cast<size_t>(params[2])]);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_IsUserVerified(AMX* amx, cell* params)
{
	DiscordUser* user = resolveUserByHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!user || !out) return 0;
	*out = user->isVerified() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildRole(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !out || params[2] < 0) return 0;
	const std::string* id = guild->getRoleIdAt(static_cast<size_t>(params[2]));
	if (!id) return 0;
	*out = assignRoleHandle(*id);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildRoleCount(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!guild || !out) return 0;
	*out = static_cast<cell>(guild->getRoleIds().size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMember(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !out || params[2] < 0) return 0;
	const std::string* id = guild->getMemberIdAt(static_cast<size_t>(params[2]));
	if (!id) return 0;
	if (!component()->findUserById(*id)) return 0;
	*out = assignUserHandle(*id);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberCount(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!guild || !out) return 0;
	*out = static_cast<cell>(guild->getMemberIds().size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberVoiceChannel(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !user || !out) return 0;
	const std::string userId(user->getUserId().data(), user->getUserId().length());
	DiscordGuild::Member* member = guild->findMember(userId);
	if (!member) return 0;
	*out = member->voiceChannelId.empty() ? 0 : assignChannelHandle(member->voiceChannelId);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberNickname(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	if (!guild || !user) return 0;
	DiscordGuild::Member* member = guild->findMember(user->getUserId());
	if (!member) return 0;
	return setAmxString(amx, params[3], member->nickname, params[4]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberRole(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	cell* out = nativeRef(amx, params[4]);
	if (!guild || !user || !out || params[3] < 0) return 0;
	DiscordGuild::Member* member = guild->findMember(user->getUserId());
	if (!member || params[3] < 0 || static_cast<size_t>(params[3]) >= member->roleIds.size()) return 0;
	*out = assignRoleHandle(member->roleIds[static_cast<size_t>(params[3])]);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberRoleCount(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !user || !out) return 0;
	DiscordGuild::Member* member = guild->findMember(user->getUserId());
	if (!member) return 0;
	*out = static_cast<cell>(member->roleIds.size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_HasGuildMemberRole(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	DiscordRole* role = roleForHandle(params[3]);
	cell* out = nativeRef(amx, params[4]);
	if (!guild || !user || !role || !out) return 0;
	DiscordGuild::Member* member = guild->findMember(user->getUserId());
	if (!member) return 0;
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	*out = std::find(member->roleIds.begin(), member->roleIds.end(), roleId) != member->roleIds.end() ? 1 : 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildMemberStatus(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordUser* user = resolveUserByHandle(params[2]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !user || !out) return 0;
	DiscordGuild::Member* member = guild->findMember(user->getUserId());
	if (!member) return 0;
	*out = static_cast<cell>(member->presenceStatus);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildChannel(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[3]);
	if (!guild || !out || params[2] < 0) return 0;
	const std::string* id = guild->getChannelIdAt(static_cast<size_t>(params[2]));
	if (!id) return 0;
	*out = assignChannelHandle(*id);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetGuildChannelCount(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	cell* out = nativeRef(amx, params[2]);
	if (!guild || !out) return 0;
	*out = static_cast<cell>(guild->getChannelIds().size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetAllGuilds(AMX* amx, cell* params)
{
	cell* out = nativeRef(amx, params[1]);
	if (!out || params[2] < 0) return 0;
	const size_t maxSize = static_cast<size_t>(params[2]);
	const std::vector<std::string> guildIds = component()->getKnownGuildIds();
	const size_t count = std::min(maxSize, guildIds.size());
	for (size_t i = 0; i < count; ++i) out[i] = assignGuildHandle(guildIds[i]);
	return static_cast<cell>(count);
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildName(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	if (!guild) return 0;
	const std::string name = getAmxString(amx, params[2]);
	if (name.size() < 2 || name.size() > 100) return 0;
	return guild->setName(name) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CreateGuildChannel(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	if (!guild) return 0;
	const std::string name = getAmxString(amx, params[2]);
	const int type = static_cast<int>(params[3]);
	if (name.size() < 2 || name.size() > 100) return 0;
	if (type != static_cast<int>(EDiscordChannelType::GuildCategory) &&
		type != static_cast<int>(EDiscordChannelType::GuildText) &&
		type != static_cast<int>(EDiscordChannelType::GuildVoice)) return 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	if (!capturePawnCallback(amx, params[4], params[5], params, 6, callback)) return 0;
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("name", name) + "," + DiscordUtils::buildJsonPair("type", static_cast<int64_t>(type)) + "}";
	if (!bot->submitRestTask([bot, guildId, body, callback](DiscordHTTP& rest)
	{
		const auto response = rest.createGuildChannel(guildId, body);
		if (!response.success) return;
		bot->enqueueCompletion([response, callback]()
		{
			DiscordBridgeComponent* bridge = component();
			if (!bridge) return;
			DiscordChannel* channel = bridge->upsertChannelFromJson(response.body);
			if (!channel || !callback) return;
			g_createdGuildChannelHandle = assignChannelHandle(channel->getChannelId());
			executePawnCallback(*callback);
			g_createdGuildChannelHandle = 0;
		});
	})) return 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetCreatedGuildChannel(AMX*, cell*)
{
	return g_createdGuildChannelHandle;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRolePosition(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const int position = static_cast<int>(params[3]);
	return bot->submitRestTask([guildId, roleId, position](DiscordHTTP& http)
	{
		http.modifyGuildRolePosition(guildId, roleId, position);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRoleName(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const std::string name = getAmxString(amx, params[3]);
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("name", name) + "}";
	return bot->submitRestTask([guildId, roleId, body](DiscordHTTP& http)
	{
		http.modifyGuildRole(guildId, roleId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRolePermissions(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const uint64_t high = static_cast<uint32_t>(params[3]);
	const uint64_t low = static_cast<uint32_t>(params[4]);
	const uint64_t permissions = (high << 32U) | low;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("permissions", std::to_string(permissions)) + "}";
	return bot->submitRestTask([guildId, roleId, body](DiscordHTTP& http)
	{
		http.modifyGuildRole(guildId, roleId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRoleColor(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const int color = static_cast<int>(params[3]);
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("color", static_cast<int64_t>(color)) + "}";
	return bot->submitRestTask([guildId, roleId, body](DiscordHTTP& http)
	{
		http.modifyGuildRole(guildId, roleId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRoleHoist(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const bool value = params[3] != 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("hoist", value) + "}";
	return bot->submitRestTask([guildId, roleId, body](DiscordHTTP& http)
	{
		http.modifyGuildRole(guildId, roleId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetGuildRoleMentionable(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const bool value = params[3] != 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("mentionable", value) + "}";
	return bot->submitRestTask([guildId, roleId, body](DiscordHTTP& http)
	{
		http.modifyGuildRole(guildId, roleId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CreateGuildRole(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	if (!guild) return 0;
	const std::string name = getAmxString(amx, params[2]);
	if (name.size() < 2 || name.size() > 100) return 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	if (!capturePawnCallback(amx, params[3], params[4], params, 5, callback)) return 0;
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("name", name) + "}";
	if (!bot->submitRestTask([bot, guildId, body, callback](DiscordHTTP& rest)
	{
		const auto response = rest.createGuildRole(guildId, body);
		if (!response.success) return;
		bot->enqueueCompletion([response, guildId, callback]()
		{
			DiscordBridgeComponent* bridge = component();
			if (!bridge) return;
			DiscordRole* role = bridge->upsertRoleFromJson(response.body, guildId);
			if (!role || !callback) return;
			g_createdGuildRoleHandle = assignRoleHandle(role->getRoleId());
			executePawnCallback(*callback);
			g_createdGuildRoleHandle = 0;
		});
	})) return 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetCreatedGuildRole(AMX*, cell*)
{
	return g_createdGuildRoleHandle;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteGuildRole(AMX*, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordRole* role = roleForHandle(params[2]);
	DiscordBot* bot = nativeBot();
	if (!guild || !role || !bot) return 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string roleId(role->getRoleId().data(), role->getRoleId().length());
	return bot->submitRestTask([guildId, roleId](DiscordHTTP& http)
	{
		http.deleteGuildRole(guildId, roleId);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteInternalMessage(AMX*, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message) return 0;
	component()->removeMessage(message->getMessageId());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetMessagePersistent(AMX*, cell* params)
{
	DiscordMessage* message = resolveMessageByHandle(params[1]);
	if (!message) return 0;
	message->setPersistent(params[2] != 0);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_TriggerBotTypingIndicator(AMX*, cell* params)
{
	const std::string id = channelIdForHandle(params[1]);
	DiscordBot* bot = nativeBot();
	if (id.empty() || !bot) return 0;
	return bot->submitRestTask([id](DiscordHTTP& http)
	{
		http.triggerTyping(id);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetBotPresenceStatus(AMX*, cell*)
{
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	const int status = bot->getPresenceStatus();
	// DCC's public enum is online=1, idle=2, dnd=3, invisible=4, offline=5;
	return status == 0 ? 1 : status == 2 ? 2 : status == 1 ? 3 : status == 3 ? 4 : 5;
}

cell AMX_NATIVE_CALL Native_DCC_SetBotPresenceStatus(AMX*, cell* params)
{
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	EDiscordPresenceStatus status;
	switch (params[1])
	{
		case 1: status = EDiscordPresenceStatus::Online; break;
		case 2: status = EDiscordPresenceStatus::Idle; break;
		case 3: status = EDiscordPresenceStatus::DoNotDisturb; break;
		case 4: status = EDiscordPresenceStatus::Invisible; break;
		case 5: status = EDiscordPresenceStatus::Offline; break;
		default: return 0;
	}
	return bot->setPresenceStatus(status) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetBotActivity(AMX* amx, cell* params)
{
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	return bot->setActivity(EDiscordActivityType::Playing, getAmxString(amx, params[1])) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SetBotNickname(AMX* amx, cell* params)
{
	DiscordGuild* guild = guildForHandle(params[1]);
	DiscordBot* bot = nativeBot();
	if (!guild || !bot || bot->getBotId().empty()) return 0;
	const std::string nickname = getAmxString(amx, params[2]);
	if (!nickname.empty() && (nickname.size() < 2 || nickname.size() > 32 ||
		nickname == "discordtag" || nickname == "everyone" || nickname == "here" ||
		nickname.front() == '@' || nickname.front() == '#' || nickname.front() == ':' ||
		nickname.rfind("```", 0) == 0)) return 0;
	const std::string guildId(guild->getGuildId().data(), guild->getGuildId().length());
	const std::string botId(bot->getBotId().data(), bot->getBotId().length());
	const std::string body = std::string("{") + DiscordUtils::buildJsonPair("nick", nickname) + "}";
	return bot->submitRestTask([guildId, botId, body](DiscordHTTP& http)
	{
		http.modifyGuildMember(guildId, botId, body);
	}) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CreatePrivateChannel(AMX* amx, cell* params)
{
	DiscordUser* user = resolveUserByHandle(params[1]);
	if (!user) return 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	if (!capturePawnCallback(amx, params[2], params[3], params, 4, callback)) return 0;
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	const std::string userId(user->getUserId().data(), user->getUserId().length());
	if (!bot->submitRestTask([bot, userId, callback](DiscordHTTP& rest)
	{
		const auto response = rest.createDM(userId);
		if (!response.success) return;
		bot->enqueueCompletion([response, callback]()
		{
			DiscordBridgeComponent* bridge = component();
			if (!bridge) return;
			DiscordChannel* channel = bridge->upsertChannelFromJson(response.body);
			if (!channel || !callback) return;
			g_createdPrivateChannelHandle = assignChannelHandle(channel->getChannelId());
			executePawnCallback(*callback);
			g_createdPrivateChannelHandle = 0;
		});
	})) return 0;
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetCreatedPrivateChannel(AMX*, cell*)
{
	return g_createdPrivateChannelHandle;
}

cell AMX_NATIVE_CALL Native_DCC_EscapeMarkdown(AMX* amx, cell* params)
{
	const std::string source = getAmxString(amx, params[1]);
	std::string escaped;
	escaped.reserve(source.size() * 2);
	bool escapedByUser = false;
	for (char ch : source)
	{
		if (ch == '\\')
		{
			escapedByUser = true;
			escaped.push_back(ch);
			continue;
		}
		if (std::string("_*~`|").find(ch) != std::string::npos && !escapedByUser) escaped.push_back('\\');
		escaped.push_back(ch);
		escapedByUser = false;
	}
	if (!setAmxString(amx, params[2], escaped, params[3])) return 0;
	return static_cast<cell>(escaped.size());
}

cell AMX_NATIVE_CALL Native_DCC_CreateEmbed(AMX* amx, cell* params)
{
	EmbedData embed;
	embed.title = getAmxString(amx, params[1]);
	embed.description = getAmxString(amx, params[2]);
	embed.url = getAmxString(amx, params[3]);
	embed.timestamp = getAmxString(amx, params[4]);
	embed.color = static_cast<int>(params[5]);
	embed.footerText = getAmxString(amx, params[6]);
	embed.footerIconUrl = getAmxString(amx, params[7]);
	embed.thumbnailUrl = getAmxString(amx, params[8]);
	embed.imageUrl = getAmxString(amx, params[9]);
	const cell handle = g_nextEmbedHandle++;
	g_embeds.emplace(handle, std::move(embed));
	return handle;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteEmbed(AMX*, cell* params)
{
	return g_embeds.erase(params[1]) > 0 ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_AddEmbedField(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]);
	if (it == g_embeds.end()) return 0;
	EmbedData::Field field;
	field.name = getAmxString(amx, params[2]);
	field.value = getAmxString(amx, params[3]);
	field.inlineField = params[4] != 0;
	if (field.name.empty() || field.value.empty() || field.name.size() > 256 || field.value.size() > 1024 || it->second.fields.size() >= 25) return 0;
	it->second.fields.push_back(std::move(field));
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedTitle(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.title = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedDescription(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.description = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedUrl(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.url = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedTimestamp(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.timestamp = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedColor(AMX*, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.color = static_cast<int>(params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedFooter(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.footerText = getAmxString(amx, params[2]);
	it->second.footerIconUrl = getAmxString(amx, params[3]);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedThumbnail(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.thumbnailUrl = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SetEmbedImage(AMX* amx, cell* params)
{
	auto it = g_embeds.find(params[1]); if (it == g_embeds.end()) return 0;
	it->second.imageUrl = getAmxString(amx, params[2]); return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SendChannelEmbedMessage(AMX* amx, cell* params)
{
	const std::string channelId = channelIdForHandle(params[1]);
	const auto embedIt = g_embeds.find(params[2]);
	if (channelId.empty() || embedIt == g_embeds.end()) return 0;
	const std::string content = getAmxString(amx, params[3]);
	if (content.size() > 2000) return 0;
	std::shared_ptr<PreparedPawnCallback> callback;
	if (!capturePawnCallback(amx, params[4], params[5], params, 6, callback)) return 0;
	const std::string body = messagePayload(content, &embedIt->second);
	DiscordBot* bot = component() && component()->getBot() ? static_cast<DiscordBot*>(component()->getBot()) : nullptr;
	if (!bot) return 0;
	const bool queued = bot->submitRestTask([bot, channelId, body, callback](DiscordHTTP& rest)
	{
		const auto response = rest.sendMessagePayload(channelId, body);
		if (!response.success || !callback) return;
		bot->enqueueCompletion([response, callback]()
		{
			completeMessageResponse(response.body, callback, false);
		});
	});
	if (queued) g_embeds.erase(embedIt);
	return queued ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_CreateCommand(AMX* amx, cell* params)
{
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	const std::string name = getAmxString(amx, params[1]);
	const std::string description = getAmxString(amx, params[2]);
	const std::string callback = getAmxString(amx, params[3]);
	if (name.empty() || name.size() > 32 || description.empty() || description.size() > 100 || callback.empty()) return 0;
	NativePawnScript* callbackScript = findPawnScriptWithPublic(callback.c_str(), pawnScriptFor(amx));
	if (!callbackScript)
	{
		logCommandWarning("[DiscordBridge] DCC_CreateCommand callback '" + callback + "' is not exported by any loaded Pawn script");
	}
	std::string guildId;
	if (params[5] != 0)
	{
		auto guildIt = g_guildHandleToId.find(params[5]);
		if (guildIt == g_guildHandleToId.end()) return 0;
		guildId = guildIt->second;
	}
	for (auto& entry : g_commands)
	{
		CommandData& existing = entry.second;
		if (existing.name == name && existing.guildId == guildId)
		{
			// The legacy manager updates the callback on duplicate creation and
			// returns the existing handle instead of registering another command.
			existing.callback = callback;
			existing.callbackScript = callbackScript;
			logCommandInfo("[DiscordBridge] local Discord command '" + name + "' uses Pawn callback '" + callback + "' (handle " + std::to_string(entry.first) + ")");
			return entry.first;
		}
	}
	const auto creationCancelled = std::make_shared<std::atomic_bool>(false);
	const CommandData command {
		{}, guildId, name, description, callback, callbackScript, creationCancelled
	};
	const cell handle = assignCommandHandle(command);
	const char* callbackStatus = callbackScript ? "callback found" : "callback pending";
	logCommandInfo("[DiscordBridge] local Discord command '" + name + "' uses Pawn callback '" + callback + "' (handle " + std::to_string(handle) + ", " + callbackStatus + ")");
	DiscordJson body = {
		{ "name", name },
		{ "description", description },
		{ "type", 1 },
		{ "default_member_permissions", params[4] ? DiscordJson(nullptr) : DiscordJson("0") },
		{ "options", DiscordJson::array({
			{
				{ "type", 3 },
				{ "name", "arguments" },
				{ "description", "just type" },
				{ "required", false }
			}
		}) }
	};
	const std::string bodyJson = body.dump();
	if (!bot->submitRestTask([bot, handle, guildId, name, bodyJson, creationCancelled](DiscordHTTP& http)
	{
		if (creationCancelled->load(std::memory_order_acquire)) return;
		// Loading the remote command list lazily preserves the legacy
		// duplicate-by-name behavior without making command creation block Pawn.
		const auto existingCommands = http.getApplicationCommands(guildId);
		if (!existingCommands.success)
		{
			logCommandResponseFailure(bot, "lookup", name, existingCommands);
			return;
		}

		const DiscordJson existingData = DiscordJson::parse(existingCommands.body, nullptr, false);
		if (existingData.is_array())
		{
			for (const auto& item : existingData)
			{
				if (!item.is_object() || item.value("name", std::string()) != name) continue;
				const std::string existingId = item.value("id", std::string());
				if (existingId.empty()) continue;
				if (creationCancelled->load(std::memory_order_acquire))
				{
					const auto deleteResponse = http.deleteApplicationCommand(guildId, existingId);
					if (!deleteResponse.success) logCommandResponseFailure(bot, "cleanup", name, deleteResponse);
					return;
				}
				bot->enqueueCompletion([handle, guildId, responseBody = existingCommands.body, creationCancelled]()
				{
					if (creationCancelled->load(std::memory_order_acquire) || g_commands.find(handle) == g_commands.end()) return;
					cacheApplicationCommands(guildId, responseBody);
				});
				logCommandInfo("[DiscordBridge] Discord command '" + name + "' reuses an existing remote command");
				return;
			}
		}

		const auto response = http.createApplicationCommand(guildId, bodyJson);
		if (!response.success)
		{
			logCommandResponseFailure(bot, "creation", name, response);
			return;
		}
		const DiscordJson data = DiscordJson::parse(response.body, nullptr, false);
		if (!data.is_object())
		{
			logCommandWarning("[DiscordBridge] Discord returned an invalid command creation response for '" + name + "'");
			return;
		}
		const std::string discordId = data.value("id", std::string());
		if (discordId.empty())
		{
			logCommandWarning("[DiscordBridge] Discord command creation response for '" + name + "' has no id");
			return;
		}
		if (creationCancelled->load(std::memory_order_acquire))
		{
			const auto deleteResponse = http.deleteApplicationCommand(guildId, discordId);
			if (!deleteResponse.success) logCommandResponseFailure(bot, "cleanup", name, deleteResponse);
			return;
		}
		bot->enqueueCompletion([handle, discordId, creationCancelled]()
		{
			if (creationCancelled->load(std::memory_order_acquire)) return;
			auto it = g_commands.find(handle);
			if (it != g_commands.end()) it->second.discordId = discordId;
		});
		logCommandInfo("[DiscordBridge] Discord command '" + name + "' was created remotely");
	}))
	{
		g_commands.erase(handle);
		logCommandWarning("[DiscordBridge] DCC_CreateCommand could not queue command '" + name + "'");
		return 0;
	}
	return handle;
}

cell AMX_NATIVE_CALL Native_DCC_DeleteCommand(AMX*, cell* params)
{
	auto it = g_commands.find(params[1]);
	if (it == g_commands.end()) return 0;
	DiscordBot* bot = nativeBot();
	if (!bot) return 0;
	if (it->second.creationCancelled) it->second.creationCancelled->store(true, std::memory_order_release);
	const std::string guildId = it->second.guildId;
	const std::string discordId = it->second.discordId;
	const std::string name = it->second.name;
	bool queued = true;
	if (!discordId.empty())
	{
		queued = bot->submitRestTask([bot, guildId, discordId, name](DiscordHTTP& http)
		{
			const auto response = http.deleteApplicationCommand(guildId, discordId);
			if (!response.success) logCommandResponseFailure(bot, "deletion", name, response);
		});
	}
	else
	{
		// The remote id may not have reached the component tick yet.  Look up
		// the name in the same serialized REST queue so deleting a just-created
		// command cannot leave an orphan on Discord.
		queued = bot->submitRestTask([bot, guildId, name](DiscordHTTP& http)
		{
			const auto list = http.getApplicationCommands(guildId);
			if (!list.success)
			{
				logCommandResponseFailure(bot, "deletion lookup", name, list);
				return;
			}
			const DiscordJson data = DiscordJson::parse(list.body, nullptr, false);
			if (!data.is_array()) return;
			for (const auto& item : data)
			{
				if (!item.is_object() || item.value("name", std::string()) != name) continue;
				const std::string id = item.value("id", std::string());
				if (id.empty()) continue;
				const auto response = http.deleteApplicationCommand(guildId, id);
				if (!response.success) logCommandResponseFailure(bot, "deletion", name, response);
				break;
			}
		});
	}
	if (!queued) return 0;
	g_commands.erase(it);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetInteractionMentionCount(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]); cell* out = nativeRef(amx, params[2]);
	if (it == g_interactions.end() || !out) return 0;
	*out = static_cast<cell>(it->second.mentions.size());
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetInteractionMention(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]); cell* out = nativeRef(amx, params[3]);
	if (it == g_interactions.end() || !out || params[2] < 0 || static_cast<size_t>(params[2]) >= it->second.mentions.size()) return 0;
	*out = it->second.mentions[static_cast<size_t>(params[2])]; return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetInteractionContent(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]); if (it == g_interactions.end()) return 0;
	return setAmxString(amx, params[2], it->second.content, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_GetInteractionChannel(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]); cell* out = nativeRef(amx, params[2]);
	if (it == g_interactions.end() || !out) return 0;
	*out = it->second.channelId.empty() ? 0 : assignChannelHandle(it->second.channelId);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_GetInteractionGuild(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]); cell* out = nativeRef(amx, params[2]);
	if (it == g_interactions.end() || !out) return 0;
	*out = it->second.guildId.empty() ? 0 : assignGuildHandle(it->second.guildId);
	return 1;
}

cell AMX_NATIVE_CALL Native_DCC_SendInteractionMessage(AMX* amx, cell* params)
{
	auto it = g_interactions.find(params[1]);
	DiscordBot* bot = nativeBot();
	if (it == g_interactions.end() || !bot) return 0;
	const std::string content = getAmxString(amx, params[2]); if (content.size() > 2000) return 0;
	const bool responded = it->second.responded;
	const std::string interactionId = it->second.id;
	const std::string token = it->second.token;
	const std::string payload = responded
		? messagePayload(content, nullptr)
		: DiscordJson { { "type", 4 }, { "data", { { "content", content } } } }.dump();
	const bool queued = bot->submitRestTask([responded, interactionId, token, payload](DiscordHTTP& http)
	{
		if (responded) http.editOriginalInteractionResponse(token, payload);
		else http.createInteractionResponse(interactionId, token, payload);
	});
	if (queued) it->second.responded = true;
	return queued ? 1 : 0;
}

cell AMX_NATIVE_CALL Native_DCC_SendInteractionEmbed(AMX* amx, cell* params)
{
	auto interactionIt = g_interactions.find(params[1]); auto embedIt = g_embeds.find(params[2]);
	DiscordBot* bot = nativeBot();
	if (interactionIt == g_interactions.end() || embedIt == g_embeds.end() || !bot) return 0;
	const std::string content = getAmxString(amx, params[3]); if (content.size() > 2000) return 0;
	const std::string payload = messagePayload(content, &embedIt->second);
	const bool responded = interactionIt->second.responded;
	const std::string interactionId = interactionIt->second.id;
	const std::string token = interactionIt->second.token;
	const bool queued = bot->submitRestTask([responded, interactionId, token, payload](DiscordHTTP& http)
	{
		if (responded) http.editOriginalInteractionResponse(token, payload);
		else http.createInteractionResponse(interactionId, token, DiscordJson { { "type", 4 }, { "data", DiscordJson::parse(payload) } }.dump());
	});
	if (queued)
	{
		g_embeds.erase(embedIt);
		interactionIt->second.responded = true;
	}
	return queued ? 1 : 0;
}

void handleDiscordInteractionPayloadInternal(const std::string& json)
{
	const DiscordJson data = DiscordJson::parse(json, nullptr, false);
	if (data.is_discarded() || !data.is_object()) return;
	const int type = data.value("type", 0);
	if (type != 1 && type != 2 && type != 3 && type != 4 && type != 5) return;

	InteractionData interaction;
	interaction.id = data.value("id", std::string());
	interaction.token = data.value("token", std::string());
	interaction.channelId = data.value("channel_id", std::string());
	interaction.guildId = data.value("guild_id", std::string());
	DiscordBot* bot = nativeBot();
	if (type == 1)
	{
		const std::string id = data.value("id", std::string());
		const std::string token = data.value("token", std::string());
		if (bot && !id.empty() && !token.empty())
		{
			bot->submitRestTask([id, token](DiscordHTTP& http)
			{
				http.createInteractionResponse(id, token, DiscordJson { { "type", 1 } }.dump());
			});
		}
		return;
	}
	const DiscordJson commandData = data.value("data", DiscordJson::object());
	if (commandData.is_object())
	{
		if ((commandData.find("options") != commandData.end()) && commandData["options"].is_array() && !commandData["options"].empty())
		{
			const auto& option = commandData["options"][0];
			if (option.is_object() && (option.find("value") != option.end()))
			{
				if (option["value"].is_string()) interaction.content = option["value"].get<std::string>();
				else interaction.content = option["value"].dump();
			}
		}
	}
	if (interaction.id.empty() || interaction.token.empty()) return;
	const cell interactionHandle = assignInteractionHandle(interaction);
	InteractionData* storedInteraction = nullptr;
	if (auto stored = g_interactions.find(interactionHandle); stored != g_interactions.end()) storedInteraction = &stored->second;
	if (!storedInteraction) return;

	const DiscordJson actor = (data.find("member") != data.end()) && data["member"].is_object() ? data["member"].value("user", DiscordJson::object()) : data.value("user", DiscordJson::object());
	DiscordUser* user = nullptr;
	if (actor.is_object() && (actor.find("id") != actor.end()) && actor["id"].is_string())
	{
		user = component()->upsertUserFromJson(actor.dump());
	}
	const cell userHandle = user ? assignUserHandle(user->getUserId()) : 0;

	const DiscordJson resolved = commandData.is_object() ? commandData.value("resolved", DiscordJson::object()) : DiscordJson::object();
	if (resolved.is_object() && (resolved.find("users") != resolved.end()) && resolved["users"].is_object())
	{
		for (auto item = resolved["users"].begin(); item != resolved["users"].end(); ++item)
		{
			if (item.value().is_object())
			{
				if (DiscordUser* mention = component()->upsertUserFromJson(item.value().dump())) storedInteraction->mentions.push_back(assignUserHandle(mention->getUserId()));
			}
		}
	}
	collectInteractionTextMentions(*storedInteraction);
	if (bot)
	{
		// A gateway interaction must be acknowledged promptly.  Application
		// commands and modal submits use a deferred channel response, message
		// components use a deferred update, and autocomplete gets an empty
		// choices response until a richer option API is added.
		DiscordJson acknowledgement = { { "type", type == 3 ? 6 : type == 4 ? 8 : 5 } };
		if (type == 4) acknowledgement["data"] = { { "choices", DiscordJson::array() } };
		const std::string interactionId = storedInteraction->id;
		const std::string token = storedInteraction->token;
		storedInteraction->responded = bot->submitRestTask([interactionId, token, acknowledgement = acknowledgement.dump()](DiscordHTTP& http)
		{
			http.createInteractionResponse(interactionId, token, acknowledgement);
		});
	}

	const std::string commandName = type == 2 && commandData.is_object() ? commandData.value("name", std::string()) : std::string();
	bool matchedCommand = false;
	for (const auto& entry : g_commands)
	{
		const CommandData& command = entry.second;
		if (command.name != commandName || (!command.guildId.empty() && command.guildId != storedInteraction->guildId)) continue;
		matchedCommand = true;
		if (userHandle != 0 && !callPawnPublicFromScript(command.callback.c_str(), command.callbackScript, interactionHandle, userHandle))
		{
			logCommandWarningOnce("missing-callback:" + command.guildId + ":" + command.name + ":" + command.callback,
				"[DiscordBridge] no loaded Pawn callback '" + command.callback + "' for Discord command '" + command.name + "'");
		}
		break;
	}
	if (!matchedCommand && !commandName.empty())
	{
		logCommandWarningOnce("unknown-command:" + storedInteraction->guildId + ":" + commandName,
			"[DiscordBridge] received Discord command '" + commandName + "' with no local DCC_CreateCommand registration");
	}
	g_interactions.erase(interactionHandle);
}

NativeFunc findImplemented(const std::string& name)
{
	static const std::unordered_map<std::string, NativeFunc> kImplemented {
		{ "ConnectDiscordBot", Native_ConnectDiscordBot },
		{ "IsDiscordConnected", Native_IsDiscordConnected },
		{ "DCC_FindChannelById", Native_DCC_FindChannelById },
		{ "DCC_FindChannelByName", Native_DCC_FindChannelByName },
		{ "DCC_GetChannelId", Native_DCC_GetChannelId },
		{ "DCC_GetChannelName", Native_DCC_GetChannelName },
		{ "DCC_GetChannelTopic", Native_DCC_GetChannelTopic },
		{ "DCC_GetChannelType", Native_DCC_GetChannelType },
		{ "DCC_GetChannelGuild", Native_DCC_GetChannelGuild },
		{ "DCC_GetChannelPosition", Native_DCC_GetChannelPosition },
		{ "DCC_IsChannelNsfw", Native_DCC_IsChannelNsfw },
		{ "DCC_GetChannelParentCategory", Native_DCC_GetChannelParentCategory },
		{ "DCC_SendChannelMessage", Native_DCC_SendChannelMessage },
		{ "DCC_SetChannelName", Native_DCC_SetChannelName },
		{ "DCC_SetChannelTopic", Native_DCC_SetChannelTopic },
		{ "DCC_SetChannelPosition", Native_DCC_SetChannelPosition },
		{ "DCC_SetChannelNsfw", Native_DCC_SetChannelNsfw },
		{ "DCC_SetChannelParentCategory", Native_DCC_SetChannelParentCategory },
		{ "DCC_DeleteChannel", Native_DCC_DeleteChannel },
		{ "DCC_FindUserById", Native_DCC_FindUserById },
		{ "DCC_FindUserByName", Native_DCC_FindUserByName },
		{ "DCC_GetUserId", Native_DCC_GetUserId },
		{ "DCC_GetUserName", Native_DCC_GetUserName },
		{ "DCC_GetUserDiscriminator", Native_DCC_GetUserDiscriminator },
		{ "DCC_IsUserBot", Native_DCC_IsUserBot },
		{ "DCC_IsUserVerified", Native_DCC_IsUserVerified },
		{ "DCC_FindGuildById", Native_DCC_FindGuildById },
		{ "DCC_FindGuildByName", Native_DCC_FindGuildByName },
		{ "DCC_FindRoleById", Native_DCC_FindRoleById },
		{ "DCC_FindRoleByName", Native_DCC_FindRoleByName },
		{ "DCC_GetGuildId", Native_DCC_GetGuildId },
		{ "DCC_GetGuildName", Native_DCC_GetGuildName },
		{ "DCC_GetGuildOwnerId", Native_DCC_GetGuildOwnerId },
		{ "DCC_GetGuildRole", Native_DCC_GetGuildRole },
		{ "DCC_GetGuildRoleCount", Native_DCC_GetGuildRoleCount },
		{ "DCC_GetGuildMember", Native_DCC_GetGuildMember },
		{ "DCC_GetGuildMemberCount", Native_DCC_GetGuildMemberCount },
		{ "DCC_GetGuildMemberVoiceChannel", Native_DCC_GetGuildMemberVoiceChannel },
		{ "DCC_GetGuildMemberNickname", Native_DCC_GetGuildMemberNickname },
		{ "DCC_GetGuildMemberRole", Native_DCC_GetGuildMemberRole },
		{ "DCC_GetGuildMemberRoleCount", Native_DCC_GetGuildMemberRoleCount },
		{ "DCC_HasGuildMemberRole", Native_DCC_HasGuildMemberRole },
		{ "DCC_GetGuildMemberStatus", Native_DCC_GetGuildMemberStatus },
		{ "DCC_GetGuildChannel", Native_DCC_GetGuildChannel },
		{ "DCC_GetGuildChannelCount", Native_DCC_GetGuildChannelCount },
		{ "DCC_GetAllGuilds", Native_DCC_GetAllGuilds },
		{ "DCC_SetGuildName", Native_DCC_SetGuildName },
		{ "DCC_CreateGuildChannel", Native_DCC_CreateGuildChannel },
		{ "DCC_GetCreatedGuildChannel", Native_DCC_GetCreatedGuildChannel },
		{ "DCC_CacheChannelMessage", Native_DCC_CacheChannelMessage },
		{ "DCC_GetMessageId", Native_DCC_GetMessageId },
		{ "DCC_GetMessageChannel", Native_DCC_GetMessageChannel },
		{ "DCC_GetMessageAuthor", Native_DCC_GetMessageAuthor },
		{ "DCC_GetMessageContent", Native_DCC_GetMessageContent },
		{ "DCC_IsMessageTts", Native_DCC_IsMessageTts },
		{ "DCC_IsMessageMentioningEveryone", Native_DCC_IsMessageMentioningEveryone },
		{ "DCC_GetMessageUserMentionCount", Native_DCC_GetMessageUserMentionCount },
		{ "DCC_GetMessageUserMention", Native_DCC_GetMessageUserMention },
		{ "DCC_GetMessageRoleMentionCount", Native_DCC_GetMessageRoleMentionCount },
		{ "DCC_GetMessageRoleMention", Native_DCC_GetMessageRoleMention },
		{ "DCC_GetCreatedMessage", Native_DCC_GetCreatedMessage },
		{ "DCC_DeleteInternalMessage", Native_DCC_DeleteInternalMessage },
		{ "DCC_SetMessagePersistent", Native_DCC_SetMessagePersistent },
		{ "DCC_DeleteMessage", Native_DCC_DeleteMessage },
		{ "DCC_EditMessage", Native_DCC_EditMessage },
		{ "DCC_GetRoleId", Native_DCC_GetRoleId },
		{ "DCC_GetRoleName", Native_DCC_GetRoleName },
		{ "DCC_GetRoleColor", Native_DCC_GetRoleColor },
		{ "DCC_GetRolePermissions", Native_DCC_GetRolePermissions },
		{ "DCC_IsRoleHoist", Native_DCC_IsRoleHoist },
		{ "DCC_GetRolePosition", Native_DCC_GetRolePosition },
		{ "DCC_IsRoleMentionable", Native_DCC_IsRoleMentionable },
		{ "DCC_SetGuildRolePosition", Native_DCC_SetGuildRolePosition },
		{ "DCC_SetGuildRoleName", Native_DCC_SetGuildRoleName },
		{ "DCC_SetGuildRolePermissions", Native_DCC_SetGuildRolePermissions },
		{ "DCC_SetGuildRoleColor", Native_DCC_SetGuildRoleColor },
		{ "DCC_SetGuildRoleHoist", Native_DCC_SetGuildRoleHoist },
		{ "DCC_SetGuildRoleMentionable", Native_DCC_SetGuildRoleMentionable },
		{ "DCC_CreateGuildRole", Native_DCC_CreateGuildRole },
		{ "DCC_GetCreatedGuildRole", Native_DCC_GetCreatedGuildRole },
		{ "DCC_DeleteGuildRole", Native_DCC_DeleteGuildRole },
		{ "DCC_CreateEmoji", Native_DCC_CreateEmoji },
		{ "DCC_DeleteEmoji", Native_DCC_DeleteEmoji },
		{ "DCC_GetEmojiName", Native_DCC_GetEmojiName },
		{ "DCC_CreateReaction", Native_DCC_CreateReaction },
		{ "DCC_DeleteMessageReaction", Native_DCC_DeleteMessageReaction },
		{ "DCC_AddGuildMemberRole", Native_DCC_AddGuildMemberRole },
		{ "DCC_RemoveGuildMemberRole", Native_DCC_RemoveGuildMemberRole },
		{ "DCC_SetGuildMemberNickname", Native_DCC_SetGuildMemberNickname },
		{ "DCC_SetGuildMemberVoiceChannel", Native_DCC_SetGuildMemberVoiceChannel },
		{ "DCC_RemoveGuildMember", Native_DCC_RemoveGuildMember },
		{ "DCC_CreateGuildMemberBan", Native_DCC_CreateGuildMemberBan },
		{ "DCC_RemoveGuildMemberBan", Native_DCC_RemoveGuildMemberBan },
		{ "DCC_GetBotPresenceStatus", Native_DCC_GetBotPresenceStatus },
		{ "DCC_TriggerBotTypingIndicator", Native_DCC_TriggerBotTypingIndicator },
		{ "DCC_SetBotNickname", Native_DCC_SetBotNickname },
		{ "DCC_CreatePrivateChannel", Native_DCC_CreatePrivateChannel },
		{ "DCC_GetCreatedPrivateChannel", Native_DCC_GetCreatedPrivateChannel },
		{ "DCC_SetBotPresenceStatus", Native_DCC_SetBotPresenceStatus },
		{ "DCC_SetBotActivity", Native_DCC_SetBotActivity },
		{ "DCC_EscapeMarkdown", Native_DCC_EscapeMarkdown },
		{ "DCC_CreateEmbed", Native_DCC_CreateEmbed },
		{ "DCC_DeleteEmbed", Native_DCC_DeleteEmbed },
		{ "DCC_SendChannelEmbedMessage", Native_DCC_SendChannelEmbedMessage },
		{ "DCC_AddEmbedField", Native_DCC_AddEmbedField },
		{ "DCC_SetEmbedTitle", Native_DCC_SetEmbedTitle },
		{ "DCC_SetEmbedDescription", Native_DCC_SetEmbedDescription },
		{ "DCC_SetEmbedUrl", Native_DCC_SetEmbedUrl },
		{ "DCC_SetEmbedTimestamp", Native_DCC_SetEmbedTimestamp },
		{ "DCC_SetEmbedColor", Native_DCC_SetEmbedColor },
		{ "DCC_SetEmbedFooter", Native_DCC_SetEmbedFooter },
		{ "DCC_SetEmbedThumbnail", Native_DCC_SetEmbedThumbnail },
		{ "DCC_SetEmbedImage", Native_DCC_SetEmbedImage },
		{ "DCC_CreateCommand", Native_DCC_CreateCommand },
		{ "DCC_DeleteCommand", Native_DCC_DeleteCommand },
		{ "DCC_GetInteractionMentionCount", Native_DCC_GetInteractionMentionCount },
		{ "DCC_GetInteractionMention", Native_DCC_GetInteractionMention },
		{ "DCC_GetInteractionContent", Native_DCC_GetInteractionContent },
		{ "DCC_GetInteractionChannel", Native_DCC_GetInteractionChannel },
		{ "DCC_GetInteractionGuild", Native_DCC_GetInteractionGuild },
		{ "DCC_SendInteractionEmbed", Native_DCC_SendInteractionEmbed },
		{ "DCC_SendInteractionMessage", Native_DCC_SendInteractionMessage },

		{ "FindDiscordChannelByID", Native_DCC_FindChannelById },
		{ "FindDiscordChannelByName", Native_DCC_FindChannelByName },
		{ "FindDiscordConfiguredChannel", Native_FindDiscordConfiguredChannel },
		{ "GetDiscordChannelID", Native_DCC_GetChannelId },
		{ "GetDiscordChannelName", Native_DCC_GetChannelName },
		{ "GetDiscordChannelTopic", Native_DCC_GetChannelTopic },
		{ "GetDiscordChannelType", Native_DCC_GetChannelType },
		{ "SendDiscordChannelMessage", Native_DCC_SendChannelMessage },
		{ "SetDiscordChannelName", Native_DCC_SetChannelName },
		{ "SetDiscordChannelTopic", Native_DCC_SetChannelTopic },
		{ "DeleteDiscordChannel", Native_DCC_DeleteChannel },
		{ "FindDiscordUserByID", Native_DCC_FindUserById },
		{ "FindDiscordUserByName", Native_DCC_FindUserByName },
		{ "GetDiscordUserID", Native_DCC_GetUserId },
		{ "GetDiscordUserName", Native_DCC_GetUserName },
		{ "GetDiscordUserDiscriminator", Native_DCC_GetUserDiscriminator },
		{ "IsDiscordUserBot", Native_DCC_IsUserBot },
		{ "FindDiscordGuildByID", Native_DCC_FindGuildById },
		{ "FindDiscordGuildByName", Native_DCC_FindGuildByName },
		{ "FindDiscordRoleByID", Native_DCC_FindRoleById },
		{ "FindDiscordRoleByName", Native_DCC_FindRoleByName },
		{ "GetDiscordGuildID", Native_DCC_GetGuildId },
		{ "GetDiscordGuildName", Native_DCC_GetGuildName },
		{ "GetDiscordGuildOwnerID", Native_DCC_GetGuildOwnerId },
		{ "CacheDiscordChannelMessage", Native_DCC_CacheChannelMessage },
		{ "GetDiscordMessageID", Native_DCC_GetMessageId },
		{ "GetDiscordMessageChannel", Native_DCC_GetMessageChannel },
		{ "GetDiscordMessageAuthor", Native_DCC_GetMessageAuthor },
		{ "GetDiscordMessageContent", Native_DCC_GetMessageContent },
		{ "IsDiscordMessageTTS", Native_DCC_IsMessageTts },
		{ "IsDiscordMessageMentioningEveryone", Native_DCC_IsMessageMentioningEveryone },
		{ "DeleteDiscordMessage", Native_DCC_DeleteMessage },
		{ "EditDiscordMessage", Native_DCC_EditMessage },
		{ "GetDiscordRoleID", Native_DCC_GetRoleId },
		{ "GetDiscordRoleName", Native_DCC_GetRoleName },
		{ "GetDiscordRoleColour", Native_DCC_GetRoleColor },
		{ "GetDiscordRolePermissions", Native_DCC_GetRolePermissions },
		{ "IsDiscordRoleHoist", Native_DCC_IsRoleHoist },
		{ "GetDiscordRolePosition", Native_DCC_GetRolePosition },
		{ "IsDiscordRoleMentionable", Native_DCC_IsRoleMentionable },
		{ "CreateDiscordEmoji", Native_DCC_CreateEmoji },
		{ "DeleteDiscordEmoji", Native_DCC_DeleteEmoji },
		{ "GetDiscordEmojiName", Native_DCC_GetEmojiName },
		{ "CreateDiscordReaction", Native_DCC_CreateReaction },
		{ "DeleteDiscordMessageReaction", Native_DCC_DeleteMessageReaction },
		{ "AddDiscordGuildMemberRole", Native_DCC_AddGuildMemberRole },
		{ "RemoveDiscordGuildMemberRole", Native_DCC_RemoveGuildMemberRole },
		{ "SetDiscordGuildMemberNickname", Native_DCC_SetGuildMemberNickname },
		{ "SetDiscordGuildMemberVoiceChannel", Native_DCC_SetGuildMemberVoiceChannel },
		{ "RemoveDiscordGuildMember", Native_DCC_RemoveGuildMember },
		{ "CreateDiscordGuildMemberBan", Native_DCC_CreateGuildMemberBan },
		{ "RemoveDiscordGuildMemberBan", Native_DCC_RemoveGuildMemberBan },
		{ "GetDiscordChannelGuild", Native_DCC_GetChannelGuild },
		{ "GetDiscordChannelPosition", Native_DCC_GetChannelPosition },
		{ "IsDiscordChannelNsfw", Native_DCC_IsChannelNsfw },
		{ "GetDiscordChannelParentCategory", Native_DCC_GetChannelParentCategory },
		{ "SetDiscordChannelPosition", Native_DCC_SetChannelPosition },
		{ "SetDiscordChannelNsfw", Native_DCC_SetChannelNsfw },
		{ "SetDiscordChannelParentCategory", Native_DCC_SetChannelParentCategory },
		{ "GetDiscordMessageUserMentionCount", Native_DCC_GetMessageUserMentionCount },
		{ "GetDiscordMessageUserMention", Native_DCC_GetMessageUserMention },
		{ "GetDiscordMessageRoleMentionCount", Native_DCC_GetMessageRoleMentionCount },
		{ "GetDiscordMessageRoleMention", Native_DCC_GetMessageRoleMention },
		{ "GetDiscordCreatedMessage", Native_DCC_GetCreatedMessage },
		{ "DeleteDiscordInternalMessage", Native_DCC_DeleteInternalMessage },
		{ "SetDiscordMessagePersistent", Native_DCC_SetMessagePersistent },
		{ "IsDiscordUserVerified", Native_DCC_IsUserVerified },
		{ "GetDiscordGuildRole", Native_DCC_GetGuildRole },
		{ "GetDiscordGuildRoleCount", Native_DCC_GetGuildRoleCount },
		{ "GetDiscordGuildMember", Native_DCC_GetGuildMember },
		{ "GetDiscordGuildMemberCount", Native_DCC_GetGuildMemberCount },
		{ "GetDiscordGuildMemberVoiceChannel", Native_DCC_GetGuildMemberVoiceChannel },
		{ "GetDiscordGuildMemberNickname", Native_DCC_GetGuildMemberNickname },
		{ "GetDiscordGuildMemberRole", Native_DCC_GetGuildMemberRole },
		{ "GetDiscordGuildMemberRoleCount", Native_DCC_GetGuildMemberRoleCount },
		{ "HasDiscordGuildMemberRole", Native_DCC_HasGuildMemberRole },
		{ "GetDiscordGuildMemberStatus", Native_DCC_GetGuildMemberStatus },
		{ "GetDiscordGuildChannel", Native_DCC_GetGuildChannel },
		{ "GetDiscordGuildChannelCount", Native_DCC_GetGuildChannelCount },
		{ "GetDiscordAllGuilds", Native_DCC_GetAllGuilds },
		{ "SetDiscordGuildName", Native_DCC_SetGuildName },
		{ "CreateDiscordGuildChannel", Native_DCC_CreateGuildChannel },
		{ "GetDiscordCreatedGuildChannel", Native_DCC_GetCreatedGuildChannel },
		{ "SetDiscordGuildRolePosition", Native_DCC_SetGuildRolePosition },
		{ "SetDiscordGuildRoleName", Native_DCC_SetGuildRoleName },
		{ "SetDiscordGuildRolePermissions", Native_DCC_SetGuildRolePermissions },
		{ "SetDiscordGuildRoleColour", Native_DCC_SetGuildRoleColor },
		{ "SetDiscordGuildRoleHoist", Native_DCC_SetGuildRoleHoist },
		{ "SetDiscordGuildRoleMentionable", Native_DCC_SetGuildRoleMentionable },
		{ "CreateDiscordGuildRole", Native_DCC_CreateGuildRole },
		{ "GetDiscordCreatedGuildRole", Native_DCC_GetCreatedGuildRole },
		{ "DeleteDiscordGuildRole", Native_DCC_DeleteGuildRole },
		{ "GetDiscordBotPresenceStatus", Native_DCC_GetBotPresenceStatus },
		{ "TriggerDiscordBotTypingIndicator", Native_DCC_TriggerBotTypingIndicator },
		{ "SetDiscordBotNickname", Native_DCC_SetBotNickname },
		{ "CreateDiscordPrivateChannel", Native_DCC_CreatePrivateChannel },
		{ "GetDiscordCreatedPrivateChannel", Native_DCC_GetCreatedPrivateChannel },
		{ "SetDiscordBotPresenceStatus", Native_DCC_SetBotPresenceStatus },
		{ "SetDiscordBotActivity", Native_DCC_SetBotActivity },
		{ "EscapeDiscordMarkdown", Native_DCC_EscapeMarkdown },
		{ "CreateDiscordEmbed", Native_DCC_CreateEmbed },
		{ "DeleteDiscordEmbed", Native_DCC_DeleteEmbed },
		{ "SendDiscordChannelEmbedMessage", Native_DCC_SendChannelEmbedMessage },
		{ "AddDiscordEmbedField", Native_DCC_AddEmbedField },
		{ "SetDiscordEmbedTitle", Native_DCC_SetEmbedTitle },
		{ "SetDiscordEmbedDescription", Native_DCC_SetEmbedDescription },
		{ "SetDiscordEmbedURL", Native_DCC_SetEmbedUrl },
		{ "SetDiscordEmbedTimestamp", Native_DCC_SetEmbedTimestamp },
		{ "SetDiscordEmbedColour", Native_DCC_SetEmbedColor },
		{ "SetDiscordEmbedFooter", Native_DCC_SetEmbedFooter },
		{ "SetDiscordEmbedThumbnail", Native_DCC_SetEmbedThumbnail },
		{ "SetDiscordEmbedImage", Native_DCC_SetEmbedImage },
		{ "CreateDiscordCommand", Native_DCC_CreateCommand },
		{ "DeleteDiscordCommand", Native_DCC_DeleteCommand },
		{ "GetDiscordInteractionMentionCount", Native_DCC_GetInteractionMentionCount },
		{ "GetDiscordInteractionMention", Native_DCC_GetInteractionMention },
		{ "GetDiscordInteractionContent", Native_DCC_GetInteractionContent },
		{ "GetDiscordInteractionChannel", Native_DCC_GetInteractionChannel },
		{ "GetDiscordInteractionGuild", Native_DCC_GetInteractionGuild },
		{ "SendDiscordInteractionEmbed", Native_DCC_SendInteractionEmbed },
		{ "SendDiscordInteractionMessage", Native_DCC_SendInteractionMessage },
	};

	auto it = kImplemented.find(name);
	return it == kImplemented.end() ? Native_InvalidRegistration : it->second;
}

std::vector<AMX_NATIVE_INFO> buildNativeList()
{
	std::vector<AMX_NATIVE_INFO> nativeList;
	nativeList.reserve(240);

	auto addNative = [&](const char* name)
	{
		nativeList.push_back(AMX_NATIVE_INFO { name, findImplemented(name) });
	};

	addNative("DCC_AddEmbedField");
	addNative("DCC_AddGuildMemberRole");
	addNative("DCC_CacheChannelMessage");
	addNative("DCC_CreateCommand");
	addNative("DCC_CreateEmbed");
	addNative("DCC_CreateEmoji");
	addNative("DCC_CreateGuildChannel");
	addNative("DCC_CreateGuildMemberBan");
	addNative("DCC_CreateGuildRole");
	addNative("DCC_CreatePrivateChannel");
	addNative("DCC_CreateReaction");
	addNative("DCC_DeleteChannel");
	addNative("DCC_DeleteCommand");
	addNative("DCC_DeleteEmbed");
	addNative("DCC_DeleteEmoji");
	addNative("DCC_DeleteGuildRole");
	addNative("DCC_DeleteInternalMessage");
	addNative("DCC_DeleteMessage");
	addNative("DCC_DeleteMessageReaction");
	addNative("DCC_EditMessage");
	addNative("DCC_EscapeMarkdown");
	addNative("DCC_FindChannelById");
	addNative("DCC_FindChannelByName");
	addNative("DCC_FindGuildById");
	addNative("DCC_FindGuildByName");
	addNative("DCC_FindRoleById");
	addNative("DCC_FindRoleByName");
	addNative("DCC_FindUserById");
	addNative("DCC_FindUserByName");
	addNative("DCC_GetAllGuilds");
	addNative("DCC_GetBotPresenceStatus");
	addNative("DCC_GetChannelGuild");
	addNative("DCC_GetChannelId");
	addNative("DCC_GetChannelName");
	addNative("DCC_GetChannelParentCategory");
	addNative("DCC_GetChannelPosition");
	addNative("DCC_GetChannelTopic");
	addNative("DCC_GetChannelType");
	addNative("DCC_GetCreatedGuildChannel");
	addNative("DCC_GetCreatedGuildRole");
	addNative("DCC_GetCreatedMessage");
	addNative("DCC_GetCreatedPrivateChannel");
	addNative("DCC_GetEmojiName");
	addNative("DCC_GetGuildChannel");
	addNative("DCC_GetGuildChannelCount");
	addNative("DCC_GetGuildId");
	addNative("DCC_GetGuildMember");
	addNative("DCC_GetGuildMemberCount");
	addNative("DCC_GetGuildMemberNickname");
	addNative("DCC_GetGuildMemberRole");
	addNative("DCC_GetGuildMemberRoleCount");
	addNative("DCC_GetGuildMemberStatus");
	addNative("DCC_GetGuildMemberVoiceChannel");
	addNative("DCC_GetGuildName");
	addNative("DCC_GetGuildOwnerId");
	addNative("DCC_GetGuildRole");
	addNative("DCC_GetGuildRoleCount");
	addNative("DCC_GetInteractionChannel");
	addNative("DCC_GetInteractionContent");
	addNative("DCC_GetInteractionGuild");
	addNative("DCC_GetInteractionMention");
	addNative("DCC_GetInteractionMentionCount");
	addNative("DCC_GetMessageAuthor");
	addNative("DCC_GetMessageChannel");
	addNative("DCC_GetMessageContent");
	addNative("DCC_GetMessageId");
	addNative("DCC_GetMessageRoleMention");
	addNative("DCC_GetMessageRoleMentionCount");
	addNative("DCC_GetMessageUserMention");
	addNative("DCC_GetMessageUserMentionCount");
	addNative("DCC_GetRoleColor");
	addNative("DCC_GetRoleId");
	addNative("DCC_GetRoleName");
	addNative("DCC_GetRolePermissions");
	addNative("DCC_GetRolePosition");
	addNative("DCC_GetUserDiscriminator");
	addNative("DCC_GetUserId");
	addNative("DCC_GetUserName");
	addNative("DCC_HasGuildMemberRole");
	addNative("DCC_IsChannelNsfw");
	addNative("DCC_IsMessageMentioningEveryone");
	addNative("DCC_IsMessageTts");
	addNative("DCC_IsRoleHoist");
	addNative("DCC_IsRoleMentionable");
	addNative("DCC_IsUserBot");
	addNative("DCC_IsUserVerified");
	addNative("DCC_RemoveGuildMember");
	addNative("DCC_RemoveGuildMemberBan");
	addNative("DCC_RemoveGuildMemberRole");
	addNative("DCC_SendChannelEmbedMessage");
	addNative("DCC_SendChannelMessage");
	addNative("DCC_SendInteractionEmbed");
	addNative("DCC_SendInteractionMessage");
	addNative("DCC_SetBotActivity");
	addNative("DCC_SetBotNickname");
	addNative("DCC_SetBotPresenceStatus");
	addNative("DCC_SetChannelName");
	addNative("DCC_SetChannelNsfw");
	addNative("DCC_SetChannelParentCategory");
	addNative("DCC_SetChannelPosition");
	addNative("DCC_SetChannelTopic");
	addNative("DCC_SetEmbedColor");
	addNative("DCC_SetEmbedDescription");
	addNative("DCC_SetEmbedFooter");
	addNative("DCC_SetEmbedImage");
	addNative("DCC_SetEmbedThumbnail");
	addNative("DCC_SetEmbedTimestamp");
	addNative("DCC_SetEmbedTitle");
	addNative("DCC_SetEmbedUrl");
	addNative("DCC_SetGuildMemberNickname");
	addNative("DCC_SetGuildMemberVoiceChannel");
	addNative("DCC_SetGuildName");
	addNative("DCC_SetGuildRoleColor");
	addNative("DCC_SetGuildRoleHoist");
	addNative("DCC_SetGuildRoleMentionable");
	addNative("DCC_SetGuildRoleName");
	addNative("DCC_SetGuildRolePermissions");
	addNative("DCC_SetGuildRolePosition");
	addNative("DCC_SetMessagePersistent");
	addNative("DCC_TriggerBotTypingIndicator");

	addNative("ConnectDiscordBot");
	addNative("IsDiscordConnected");
	addNative("FindDiscordChannelByID");
	addNative("FindDiscordChannelByName");
	addNative("FindDiscordConfiguredChannel");
	addNative("GetDiscordChannelID");
	addNative("GetDiscordChannelName");
	addNative("GetDiscordChannelTopic");
	addNative("GetDiscordChannelType");
	addNative("SendDiscordChannelMessage");
	addNative("SetDiscordChannelName");
	addNative("SetDiscordChannelTopic");
	addNative("DeleteDiscordChannel");
	addNative("FindDiscordUserByID");
	addNative("FindDiscordUserByName");
	addNative("GetDiscordUserID");
	addNative("GetDiscordUserName");
	addNative("GetDiscordUserDiscriminator");
	addNative("IsDiscordUserBot");
	addNative("FindDiscordGuildByID");
	addNative("FindDiscordGuildByName");
	addNative("FindDiscordRoleByID");
	addNative("FindDiscordRoleByName");
	addNative("GetDiscordGuildID");
	addNative("GetDiscordGuildName");
	addNative("GetDiscordGuildOwnerID");
	addNative("CacheDiscordChannelMessage");
	addNative("GetDiscordMessageID");
	addNative("GetDiscordMessageChannel");
	addNative("GetDiscordMessageAuthor");
	addNative("GetDiscordMessageContent");
	addNative("IsDiscordMessageTTS");
	addNative("IsDiscordMessageMentioningEveryone");
	addNative("DeleteDiscordMessage");
	addNative("EditDiscordMessage");
	addNative("GetDiscordRoleID");
	addNative("GetDiscordRoleName");
	addNative("GetDiscordRoleColour");
	addNative("GetDiscordRolePermissions");
	addNative("IsDiscordRoleHoist");
	addNative("GetDiscordRolePosition");
	addNative("IsDiscordRoleMentionable");
	addNative("CreateDiscordEmoji");
	addNative("DeleteDiscordEmoji");
	addNative("GetDiscordEmojiName");
	addNative("CreateDiscordReaction");
	addNative("DeleteDiscordMessageReaction");
	addNative("AddDiscordGuildMemberRole");
	addNative("RemoveDiscordGuildMemberRole");
	addNative("SetDiscordGuildMemberNickname");
	addNative("SetDiscordGuildMemberVoiceChannel");
	addNative("RemoveDiscordGuildMember");
	addNative("CreateDiscordGuildMemberBan");
	addNative("RemoveDiscordGuildMemberBan");

	addNative("GetDiscordChannelGuild");
	addNative("GetDiscordChannelPosition");
	addNative("IsDiscordChannelNsfw");
	addNative("GetDiscordChannelParentCategory");
	addNative("SetDiscordChannelPosition");
	addNative("SetDiscordChannelNsfw");
	addNative("SetDiscordChannelParentCategory");
	addNative("GetDiscordMessageUserMentionCount");
	addNative("GetDiscordMessageUserMention");
	addNative("GetDiscordMessageRoleMentionCount");
	addNative("GetDiscordMessageRoleMention");
	addNative("GetDiscordCreatedMessage");
	addNative("DeleteDiscordInternalMessage");
	addNative("SetDiscordMessagePersistent");
	addNative("IsDiscordUserVerified");
	addNative("GetDiscordGuildRole");
	addNative("GetDiscordGuildRoleCount");
	addNative("GetDiscordGuildMember");
	addNative("GetDiscordGuildMemberCount");
	addNative("GetDiscordGuildMemberVoiceChannel");
	addNative("GetDiscordGuildMemberNickname");
	addNative("GetDiscordGuildMemberRole");
	addNative("GetDiscordGuildMemberRoleCount");
	addNative("HasDiscordGuildMemberRole");
	addNative("GetDiscordGuildMemberStatus");
	addNative("GetDiscordGuildChannel");
	addNative("GetDiscordGuildChannelCount");
	addNative("GetDiscordAllGuilds");
	addNative("SetDiscordGuildName");
	addNative("CreateDiscordGuildChannel");
	addNative("GetDiscordCreatedGuildChannel");
	addNative("SetDiscordGuildRolePosition");
	addNative("SetDiscordGuildRoleName");
	addNative("SetDiscordGuildRolePermissions");
	addNative("SetDiscordGuildRoleColour");
	addNative("SetDiscordGuildRoleHoist");
	addNative("SetDiscordGuildRoleMentionable");
	addNative("CreateDiscordGuildRole");
	addNative("GetDiscordCreatedGuildRole");
	addNative("DeleteDiscordGuildRole");
	addNative("GetDiscordBotPresenceStatus");
	addNative("TriggerDiscordBotTypingIndicator");
	addNative("SetDiscordBotNickname");
	addNative("CreateDiscordPrivateChannel");
	addNative("GetDiscordCreatedPrivateChannel");
	addNative("SetDiscordBotPresenceStatus");
	addNative("SetDiscordBotActivity");
	addNative("EscapeDiscordMarkdown");
	addNative("CreateDiscordEmbed");
	addNative("DeleteDiscordEmbed");
	addNative("SendDiscordChannelEmbedMessage");
	addNative("AddDiscordEmbedField");
	addNative("SetDiscordEmbedTitle");
	addNative("SetDiscordEmbedDescription");
	addNative("SetDiscordEmbedURL");
	addNative("SetDiscordEmbedTimestamp");
	addNative("SetDiscordEmbedColour");
	addNative("SetDiscordEmbedFooter");
	addNative("SetDiscordEmbedThumbnail");
	addNative("SetDiscordEmbedImage");
	addNative("CreateDiscordCommand");
	addNative("DeleteDiscordCommand");
	addNative("GetDiscordInteractionMentionCount");
	addNative("GetDiscordInteractionMention");
	addNative("GetDiscordInteractionContent");
	addNative("GetDiscordInteractionChannel");
	addNative("GetDiscordInteractionGuild");
	addNative("SendDiscordInteractionEmbed");
	addNative("SendDiscordInteractionMessage");

	return nativeList;
}
}

void HandleDiscordInteractionPayload(const std::string& json)
{
	handleDiscordInteractionPayloadInternal(json);
}

int RegisterDiscordNatives(IPawnScript& script)
{
	static const std::vector<AMX_NATIVE_INFO> kNativeList = buildNativeList();
	return script.Register(kNativeList.data(), static_cast<int>(kNativeList.size()));
}

void ResetDiscordNativeHandles()
{
	resetNativeHandles();
}

void ForgetDiscordNativeScript(IPawnScript& script)
{
	for (auto& entry : g_commands)
	{
		if (entry.second.callbackScript == &script) entry.second.callbackScript = nullptr;
	}
}

cell GetOrCreateDiscordChannelHandle(StringView channelId)
{
	return assignChannelHandle(channelId);
}

cell GetOrCreateDiscordGuildHandle(StringView guildId)
{
	return assignGuildHandle(guildId);
}

cell GetOrCreateDiscordUserHandle(StringView userId)
{
	return assignUserHandle(userId);
}

cell GetOrCreateDiscordMessageHandle(StringView messageId)
{
	return assignMessageHandle(messageId);
}

cell GetOrCreateDiscordRoleHandle(StringView roleId)
{
	return assignRoleHandle(roleId);
}

cell GetOrCreateDiscordEmojiHandle(StringView name, StringView snowflake)
{
	std::string token(name.data(), name.length());
	if (!snowflake.empty())
	{
		token += ":";
		token.append(snowflake.data(), snowflake.length());
	}
	return assignEmojiHandle(token);
}

cell CreateDiscordEmojiHandle(StringView name, StringView snowflake)
{
	std::string token(name.data(), name.length());
	if (!snowflake.empty())
	{
		token += ":";
		token.append(snowflake.data(), snowflake.length());
	}
	if (token.empty()) return 0;

	const cell handle = g_nextEmojiHandle++;
	g_emojiHandleToToken.emplace(handle, std::move(token));
	return handle;
}

void DeleteDiscordEmojiHandle(cell handle)
{
	if (handle != 0) g_emojiHandleToToken.erase(handle);
}
