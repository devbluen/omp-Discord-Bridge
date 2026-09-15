/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-log.hpp"
#include <core.hpp>
#include "samp-plugin.hpp"
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
constexpr std::size_t MAX_QUEUED_MESSAGES = 512;

std::mutex g_queueMutex;
std::deque<std::pair<LogLevel, std::string>> g_queue;
std::size_t g_droppedMessages = 0;
std::thread::id g_mainThread;
bool g_mainThreadKnown = false;

void writeNow(ICore* core, LogLevel level, const std::string& message)
{
	if (core)
	{
		core->logLn(level, "%s", message.c_str());
		return;
	}
	if (logprintf)
	{
		logprintf("%s", message.c_str());
	}
}

void writeLog(ICore* core, LogLevel level, const std::string& message)
{
	if (g_mainThreadKnown && std::this_thread::get_id() != g_mainThread)
	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		if (g_queue.size() < MAX_QUEUED_MESSAGES) g_queue.emplace_back(level, message);
		else ++g_droppedMessages;
		return;
	}
	writeNow(core, level, message);
}
}

void DiscordLogWarning(ICore* core, const std::string& message)
{
	writeLog(core, LogLevel::Warning, message);
}

void DiscordLogMessage(ICore* core, const std::string& message)
{
	writeLog(core, LogLevel::Message, message);
}

void DiscordLogSetMainThread()
{
	g_mainThread = std::this_thread::get_id();
	g_mainThreadKnown = true;
}

void DiscordLogFlush(ICore* core)
{
	std::deque<std::pair<LogLevel, std::string>> pending;
	std::size_t dropped = 0;
	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		if (g_queue.empty() && g_droppedMessages == 0) return;
		pending.swap(g_queue);
		dropped = g_droppedMessages;
		g_droppedMessages = 0;
	}
	for (const auto& entry : pending)
	{
		writeNow(core, entry.first, entry.second);
	}
	if (dropped > 0)
	{
		writeNow(core, LogLevel::Warning, "[DiscordBridge] " + std::to_string(dropped) + " log message(s) were dropped");
	}
}
