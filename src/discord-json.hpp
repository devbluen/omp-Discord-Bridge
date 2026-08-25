/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

// The original connector bundled nlohmann/json.  Keep using that known-good
// parser instead of attempting to interpret Discord payloads with substring
// searches.  Discord's payloads are nested and may contain escaped strings,
// arrays, nulls, and fields added over time.
#include <json.hpp>

using DiscordJson = nlohmann::json;
