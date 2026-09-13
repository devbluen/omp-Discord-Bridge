/*
	Example 05 - Select menus

	What you will learn:
	  - a menu with your own options that allows picking several;
	  - Discord's ready-made menus: users, roles and channels;
	  - reading the picked values in DBR_OnSelectMenu.

	Each menu takes a whole row (action row).
*/

#include <open.mp>
#include <discord-bridge>

#define MENUS_CHANNEL "123456789012345678"

// Functions returning a tag must be declared before they are used.
forward DiscordComponent:InRow(DiscordComponent:component);

main()
{
}

public DBR_OnReady()
{
	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("Pick below:");

	// 1) Your own options: pick 1 to 3 activities.
	new DiscordComponent:activities = DBR_CreateSelectMenu(DISCORD_SELECT_STRING, "activities", "What do you enjoy on the server?", 1, 3);
	//                                  label         value      description
	DBR_AddSelectMenuOption(activities, "Racing",     "race",    "Racing events");
	DBR_AddSelectMenuOption(activities, "Roleplay",   "rp",      "Stories and characters");
	DBR_AddSelectMenuOption(activities, "Shooting",   "shoot",   "Combat modes");
	DBR_AddSelectMenuOption(activities, "Mapping",    "map",     "Maps and objects");
	DBR_AddBuilderComponent(message, InRow(activities));

	// 2) Server users.
	DBR_AddBuilderComponent(message, InRow(DBR_CreateSelectMenu(DISCORD_SELECT_USER, "friend", "Invite a friend")));

	// 3) Server roles.
	DBR_AddBuilderComponent(message, InRow(DBR_CreateSelectMenu(DISCORD_SELECT_ROLE, "role", "Pick a role")));

	// 4) Channels, text channels only.
	new DiscordComponent:channels = DBR_CreateSelectMenu(DISCORD_SELECT_CHANNEL, "channel", "Where should announcements go?");
	DBR_AddSelectMenuChannelType(channels, DISCORD_GUILD_TEXT);
	DBR_AddBuilderComponent(message, InRow(channels));

	DBR_SendMessage(DBR_FindChannelByID(MENUS_CHANNEL), message);
	return 1;
}

DiscordComponent:InRow(DiscordComponent:component)
{
	new DiscordComponent:row = DBR_CreateActionRow();
	DBR_AddComponent(row, component);
	return row;
}

public DBR_OnSelectMenu(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	// For menus, leave the custom id empty ("") to read the picked values.
	new total = DBR_GetInteractionValueCount(interaction);
	new value[DISCORD_ID_SIZE], reply[256];

	if (!strcmp(custom_id, "activities"))
	{
		reply = "You picked:";
		for (new i = 0; i < total; i++)
		{
			DBR_GetInteractionValue(interaction, "", value, sizeof value, i);
			format(reply, sizeof reply, "%s `%s`", reply, value);
		}
	}
	else if (!strcmp(custom_id, "friend"))
	{
		// User menus return user IDs.
		new name[DISCORD_USERNAME_SIZE];
		DBR_GetInteractionValue(interaction, "", value);
		DBR_GetUserName(DBR_FindUserByID(value), name);
		format(reply, sizeof reply, "Thanks for inviting **%s**!", name);
	}
	else if (!strcmp(custom_id, "role"))
	{
		new name[64];
		DBR_GetInteractionValue(interaction, "", value);
		DBR_GetRoleName(DBR_FindRoleByID(value), name);
		format(reply, sizeof reply, "Picked role: **%s**.", name);
	}
	else if (!strcmp(custom_id, "channel"))
	{
		DBR_GetInteractionValue(interaction, "", value);
		format(reply, sizeof reply, "Announcements will go to <#%s>.", value);
	}
	else
	{
		return 1;
	}
	return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
}
