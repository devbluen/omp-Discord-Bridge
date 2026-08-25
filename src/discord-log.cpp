/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-log.hpp"
#include <core.hpp>
#include "samp-plugin.hpp"

namespace
{
void writeLog(ICore* core, LogLevel level, const std::string& message)
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
	else
	{
		(void)level;
		(void)message;
	}
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
