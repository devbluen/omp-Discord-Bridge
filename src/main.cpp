/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include <sdk.hpp>
#include "discord-interface.hpp"
#include "discord-component.hpp"

COMPONENT_ENTRY_POINT()
{
	return DiscordBridgeComponent::getInstance();
}
