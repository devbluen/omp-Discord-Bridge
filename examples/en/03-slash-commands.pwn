/*
	Example 03 - Slash commands

	What you will learn:
	  1. a simple command with its own callback;
	  2. options (parameters) with types and limits;
	  3. fixed choices and permissions;
	  4. subcommands and groups (/account view, /account password change);
	  5. autocomplete (suggestions while the user types);
	  6. a command registered in a single server;
	  7. context menu commands (right-click a user or a message).

	How registration works:
	  - create the commands whenever you want (here, in OnGameModeInit);
	  - the plugin publishes them to Discord once the bot is ready;
	  - global commands may take a few minutes to show up. While testing,
	    register them in your own server (item 6), which is instant.
*/

#include <open.mp>
#include <discord-bridge>

#define SERVER_ID "123456789012345678"

static const gVehicles[][] =
{
	"Infernus", "Turismo", "Sultan", "Elegy", "NRG-500", "Sanchez", "Hydra", "Maverick"
};

main()
{
}

public OnGameModeInit()
{
	// 1) Simple command. The callback receives (interaction, user).
	DBR_CreateCommand("ping", "Replies with pong", .callback = "Cmd_Ping");

	// 2) Options. The last parameter of DBR_AddCommandOption tells whether it is required.
	new DiscordCommand:dice = DBR_CreateCommand("dice", "Rolls a dice", .callback = "Cmd_Dice");
	new DiscordCommandOption:sides = DBR_AddCommandOption(dice, DISCORD_OPTION_INTEGER, "sides", "Number of sides (default 6)");
	DBR_SetOptionRange(sides, 2.0, 100.0);

	// 3) Fixed choices and permission: only administrators see /weather.
	new DiscordCommand:weather = DBR_CreateCommand("weather", "Changes the server weather", .callback = "Cmd_Weather");
	DBR_SetCommandPermissions(weather, DISCORD_COMMAND_ADMINISTRATORS);
	new DiscordCommandOption:kind = DBR_AddCommandOption(weather, DISCORD_OPTION_INTEGER, "kind", "Weather kind", true);
	DBR_AddOptionChoiceInt(kind, "Sunny", 1);
	DBR_AddOptionChoiceInt(kind, "Rainy", 8);
	DBR_AddOptionChoiceInt(kind, "Foggy", 9);

	// 4) Subcommands and groups. No callback: they arrive in DBR_OnCommand.
	new DiscordCommand:account = DBR_CreateCommand("account", "Manages your account");

	new DiscordCommandOption:view = DBR_AddCommandOption(account, DISCORD_OPTION_SUBCOMMAND, "view", "Shows an account");
	DBR_AddCommandOption(account, DISCORD_OPTION_STRING, "nick", "In-game nickname", true, view);

	new DiscordCommandOption:password = DBR_AddCommandOption(account, DISCORD_OPTION_SUBCOMMAND_GROUP, "password", "Account password");
	new DiscordCommandOption:change = DBR_AddCommandOption(account, DISCORD_OPTION_SUBCOMMAND, "change", "Changes the password", .parent = password);
	new DiscordCommandOption:newPassword = DBR_AddCommandOption(account, DISCORD_OPTION_STRING, "new", "New password", true, change);
	DBR_SetOptionLength(newPassword, 6, 32);

	// 5) Autocomplete: suggestions are sent in DBR_OnAutocomplete.
	new DiscordCommand:vehicle = DBR_CreateCommand("vehicle", "Shows vehicle information");
	new DiscordCommandOption:model = DBR_AddCommandOption(vehicle, DISCORD_OPTION_STRING, "model", "Vehicle name", true);
	DBR_SetOptionAutocomplete(model);

	// 6) Command in a single server only (shows up immediately).
	DBR_CreateCommand("test", "Test command", DBR_FindGuildByID(SERVER_ID), "Cmd_Test");

	// 7) Context menus: no description and no options.
	DBR_CreateCommand("View profile", .type = DISCORD_COMMAND_USER, .callback = "Ctx_ViewProfile");
	DBR_CreateCommand("Report message", .type = DISCORD_COMMAND_MESSAGE, .callback = "Ctx_Report");
	return 1;
}

// Safe text comparison: strcmp treats "" as equal to any text.
stock bool:TextEquals(const a[], const b[])
{
	return a[0] != '\0' && strcmp(a, b) == 0;
}

forward Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	return DBR_RespondInteraction(interaction, "Pong! :ping_pong:");
}

forward Cmd_Dice(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Dice(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// When the option was not filled in, the variable keeps its default value.
	new sides = 6;
	DBR_GetInteractionOptionInt(interaction, "sides", sides);

	new reply[64];
	format(reply, sizeof reply, ":game_die: You rolled **%d** on a %d-sided dice.", random(sides) + 1, sides);
	return DBR_RespondInteraction(interaction, reply);
}

forward Cmd_Weather(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Weather(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	new kind;
	DBR_GetInteractionOptionInt(interaction, "kind", kind);
	SetWeather(kind);
	return DBR_RespondInteraction(interaction, "Server weather changed.", .ephemeral = true);
}

forward Cmd_Test(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Test(DiscordInteraction:interaction, DiscordUser:user)
{
	new name[DISCORD_USERNAME_SIZE], reply[96];
	DBR_GetUserName(user, name);
	format(reply, sizeof reply, "Hi, %s! The test command works.", name);
	return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
}

// Receives the commands that have no callback of their own.
public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "account"))
	{
		new group[DISCORD_COMMAND_NAME_SIZE], subcommand[DISCORD_COMMAND_NAME_SIZE];
		DBR_GetInteractionSubGroup(interaction, group);          // "" when there is no group
		DBR_GetInteractionSubcommand(interaction, subcommand);

		if (TextEquals(subcommand, "view"))
		{
			new nick[MAX_PLAYER_NAME + 1], reply[96];
			DBR_GetInteractionOptionString(interaction, "nick", nick);
			format(reply, sizeof reply, "The account **%s** is active.", nick);
			return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
		}
		if (TextEquals(group, "password") && TextEquals(subcommand, "change"))
		{
			return DBR_RespondInteraction(interaction, "Password changed.", .ephemeral = true);
		}
	}
	else if (!strcmp(command, "vehicle"))
	{
		new model[32], reply[64];
		DBR_GetInteractionOptionString(interaction, "model", model);
		format(reply, sizeof reply, "You picked the **%s**.", model);
		return DBR_RespondInteraction(interaction, reply);
	}
	return 1;
}

// Called for every letter typed in an option with autocomplete.
public DBR_OnAutocomplete(DiscordInteraction:interaction, DiscordUser:user, const command[], const option[])
{
	#pragma unused user
	if (strcmp(command, "vehicle") || strcmp(option, "model"))
	{
		return 1;
	}

	new typed[32];
	DBR_GetInteractionOptionString(interaction, option, typed);
	for (new i = 0; i < sizeof gVehicles; i++)
	{
		if (typed[0] == '\0' || strfind(gVehicles[i], typed, true) != -1)
		{
			// Shown name and sent value. Up to 25 suggestions.
			DBR_AddAutocompleteChoice(interaction, gVehicles[i], gVehicles[i]);
		}
	}
	// The suggestions are sent automatically when this callback returns.
	return 1;
}

forward Ctx_ViewProfile(DiscordInteraction:interaction, DiscordUser:user);
public Ctx_ViewProfile(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// The target is the user that was right-clicked.
	new targetId[DISCORD_ID_SIZE], name[DISCORD_USERNAME_SIZE], reply[96];
	DBR_GetInteractionTargetID(interaction, targetId);
	DBR_GetUserName(DBR_FindUserByID(targetId), name);
	format(reply, sizeof reply, "Profile of **%s**: no punishments on record.", name);
	return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
}

forward Ctx_Report(DiscordInteraction:interaction, DiscordUser:user);
public Ctx_Report(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// In message commands the target message is already loaded.
	new DiscordMessage:message, content[128], reply[192];
	DBR_GetInteractionMessage(interaction, message);
	DBR_GetMessageContent(message, content);
	format(reply, sizeof reply, "Report filed for the message:\n> %s", content);
	return DBR_RespondInteraction(interaction, reply, .ephemeral = true);
}
