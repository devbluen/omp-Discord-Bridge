/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <Server/Components/Pawn/pawn.hpp>
#include "samp-pawn.hpp"
#include <sdk.hpp>
#include <string>

int RegisterDiscordNatives(IPawnScript& script);
void ForgetDiscordNativeScript(IPawnScript& script);
void ResetDiscordNativeHandles();

// Runs on every component tick: expires stored interactions and publishes
// application commands that changed since the last deployment.
void ServiceDiscordNatives();
// Called right after DBR_OnReady so commands created by scripts during
// startup are published once the bot identity is known.
void NotifyDiscordNativesReady();
// Called after DBR_DisconnectBot tore the bot down.  Commands stay registered
// and are published again on the next connection.
void NotifyDiscordNativesDisconnected();

cell GetOrCreateDiscordChannelHandle(StringView channelId);
cell GetOrCreateDiscordGuildHandle(StringView guildId);
cell GetOrCreateDiscordUserHandle(StringView userId);
cell GetOrCreateDiscordMessageHandle(StringView messageId, StringView channelId = {});
cell GetOrCreateDiscordRoleHandle(StringView roleId);
cell GetOrCreateDiscordEmojiHandle(StringView name, StringView snowflake = {});
cell CreateDiscordEmojiHandle(StringView name, StringView snowflake = {});
void DeleteDiscordEmojiHandle(cell handle);

// Gateway-side interaction dispatch.  The bot keeps network I/O off the
// server thread, then hands decoded interaction payloads back here so Pawn
// callbacks always execute on the component tick thread.
void HandleDiscordInteractionPayload(const std::string& json);
