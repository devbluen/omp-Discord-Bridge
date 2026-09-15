/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <string>

struct ICore;

// Safe to call from any thread.  Messages from the network threads are queued
// and written by DiscordLogFlush on the server thread: SA-MP's logprintf is not
// thread-safe, and writing from another thread can crash other plugins.
void DiscordLogWarning(ICore* core, const std::string& message);
void DiscordLogMessage(ICore* core, const std::string& message);

// Marks the calling thread as the server thread.  Call it once while loading.
void DiscordLogSetMainThread();
// Writes the queued messages.  Call it on every server tick.
void DiscordLogFlush(ICore* core);
