/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <Server/Components/Pawn/pawn.hpp>
#include "samp-pawn.hpp"
#include <sdk.hpp>
#include <string>

class DiscordBot;

int RegisterDiscordNatives(IPawnScript& script);
void ForgetDiscordNativeScript(IPawnScript& script);
void QueuePendingDiscordCommands(DiscordBot* bot);

void ResetDiscordNativeHandles();

cell GetOrCreateDiscordChannelHandle(StringView channelId);
cell GetOrCreateDiscordGuildHandle(StringView guildId);
cell GetOrCreateDiscordUserHandle(StringView userId);
cell GetOrCreateDiscordMessageHandle(StringView messageId);
cell GetOrCreateDiscordRoleHandle(StringView roleId);
cell GetOrCreateDiscordEmojiHandle(StringView name, StringView snowflake = {});
cell CreateDiscordEmojiHandle(StringView name, StringView snowflake = {});
void DeleteDiscordEmojiHandle(cell handle);

// Gateway-side interaction dispatch.  The bot keeps network I/O off the
// server thread, then hands decoded interaction payloads back here so Pawn
// callbacks always execute on the component tick thread.
void HandleDiscordInteractionPayload(const std::string& json);
