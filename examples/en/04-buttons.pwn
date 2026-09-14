/*
	Example 04 - Buttons

	What you will learn:
	  - the button styles (primary, secondary, success, danger and link);
	  - replying to a click in DBR_OnButton;
	  - using a handler for a custom id prefix ("counter:");
	  - updating the button's own message (a counter that goes up);
	  - disabling a button.

	The "custom id" is any text you choose (up to 100 characters) that Discord
	sends back when someone clicks. Use it to tell which button was used and
	even to store data, like the counter value below.
*/

#include <open.mp>
#include <discord-bridge>

#define BUTTONS_CHANNEL "123456789012345678"
#define COUNTER_LIMIT 10

// Functions returning a tag must be declared before they are used.
forward DiscordComponent:CreateCounterRow(value);

main()
{
}

public OnGameModeInit()
{
	// Every click whose custom id starts with "counter:" goes to OnCounterClick
	// instead of DBR_OnButton.
	DBR_RegisterHandler(DISCORD_INTERACTION_COMPONENT, "counter:", "OnCounterClick", true);
	return 1;
}

public DBR_OnReady()
{
	// Buttons live inside rows (action rows). Each row holds up to 5.
	new DiscordComponent:row = DBR_CreateActionRow();
	DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_PRIMARY, "Say hi", "hello"));
	DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_SECONDARY, "Server time", "time"));
	DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_DANGER, "Delete message", "delete"));
	// Link buttons take a URL instead of a custom id and do not trigger clicks.
	DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_LINK, "open.mp website", "https://open.mp"));

	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("Try the buttons below:");
	DBR_AddBuilderComponent(message, row);
	DBR_AddBuilderComponent(message, CreateCounterRow(0));
	DBR_SendMessage(DBR_FindChannelByID(BUTTONS_CHANNEL), message);
	return 1;
}

DiscordComponent:CreateCounterRow(value)
{
	new label[32], customId[DISCORD_CUSTOM_ID_SIZE];
	format(label, sizeof label, "Clicks: %d", value);
	format(customId, sizeof customId, "counter:%d", value);

	new DiscordComponent:button = DBR_CreateButton(DISCORD_BUTTON_SUCCESS, label, customId);
	if (value >= COUNTER_LIMIT)
	{
		DBR_SetComponentDisabled(button, true);
	}

	new DiscordComponent:row = DBR_CreateActionRow();
	DBR_AddComponent(row, button);
	return row;
}

// Receives the clicks no handler captured.
public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	if (!strcmp(custom_id, "hello"))
	{
		new name[DISCORD_USERNAME_SIZE], reply[64];
		DBR_GetUserName(user, name);
		format(reply, sizeof reply, "Hi, %s! :wave:", name);
		// ephemeral = only the person who clicked sees the reply.
		return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
	}
	if (!strcmp(custom_id, "time"))
	{
		new hour, minute, second, reply[48];
		gettime(hour, minute, second);
		format(reply, sizeof reply, "Server time is %02d:%02d.", hour, minute);
		return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
	}
	if (!strcmp(custom_id, "delete"))
	{
		// The message the button belongs to.
		new DiscordMessage:message;
		DBR_GetInteractionMessage(interaction, message);
		DBR_DeleteMessage(message);
		// No reply: the plugin acknowledges the click to Discord by itself.
	}
	return 1;
}

forward OnCounterClick(DiscordInteraction:interaction, DiscordUser:user, const custom_id[]);
public OnCounterClick(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	// "counter:3" -> 3 (the number starts after the 8 characters of the prefix).
	new value = strval(custom_id[8]) + 1;

	// Replace the content of the message where the button was clicked.
	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("The counter was updated:");
	DBR_AddBuilderComponent(message, CreateCounterRow(value));
	return DBR_UpdateInteractionMessage(interaction, message);
}
