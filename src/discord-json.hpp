/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

// Discord payloads are parsed with the bundled nlohmann/json instead of
// substring searches.  Discord's payloads are nested and may contain escaped strings,
// arrays, nulls, and fields added over time.
#include <json.hpp>

using DiscordJson = nlohmann::json;
