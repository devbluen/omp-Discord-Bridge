/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <string>

struct ICore;

void DiscordLogWarning(ICore* core, const std::string& message);
void DiscordLogMessage(ICore* core, const std::string& message);
