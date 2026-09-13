/*
	Example 11 - Bot profile

	What you will learn:
	  - status (online, idle, do not disturb, invisible);
	  - activity ("Playing", "Watching", "Listening", "Competing", custom
	    status and "Streaming" with a link);
	  - an activity that rotates by itself showing online players;
	  - changing name, avatar, banner, description and nickname by command.

	Note: Discord limits name and avatar changes (a few per hour). Status and
	activity can change as often as you like.
*/

#include <open.mp>
#include <discord-bridge>

new gRotation;

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:bot = DBR_CreateCommand("bot", "Configures the bot profile");
	DBR_SetCommandPermissions(bot, DISCORD_COMMAND_ADMINISTRATORS);

	new DiscordCommandOption:sub;

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "name", "Changes the bot name");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "value", "New name", true, sub);

	// The file is looked up in the server folder and in scriptfiles/.
	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "avatar", "Changes the avatar (.png/.jpg/.gif file)");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "file", "e.g. bot/avatar.png", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "banner", "Changes the banner (file)");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "file", "e.g. bot/banner.png", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "description", "Changes the bot 'about me'");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "value", "New description", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "nickname", "Bot nickname in this server");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "value", "New nickname (empty removes it)", false, sub);
	return 1;
}

public DBR_OnReady()
{
	// Who is the bot?
	new name[DISCORD_USERNAME_SIZE];
	DBR_GetUserName(DBR_GetBotUser(), name);
	printf("[discord] Connected as %s", name);

	DBR_SetBotPresenceStatus(DISCORD_BOT_PRESENCE_ONLINE);
	DBR_SetBotActivity("San Andreas Multiplayer", DISCORD_ACTIVITY_PLAYING);

	// Rotate the activity every 30 seconds.
	SetTimer("RotateActivity", 30000, true);
	return 1;
}

forward RotateActivity();
public RotateActivity()
{
	new online, text[64];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i))
		{
			online++;
		}
	}

	switch (gRotation++ % 5)
	{
		case 0:
		{
			format(text, sizeof text, "%d players online", online);
			DBR_SetBotActivity(text, DISCORD_ACTIVITY_WATCHING);           // "Watching ..."
		}
		case 1: DBR_SetBotActivity("the city radio", DISCORD_ACTIVITY_LISTENING);     // "Listening to ..."
		case 2: DBR_SetBotActivity("the racing championship", DISCORD_ACTIVITY_COMPETING);
		case 3: DBR_SetBotActivity("Accepting new players!", DISCORD_ACTIVITY_CUSTOM); // custom status
		case 4: DBR_SetBotActivity("live event", DISCORD_ACTIVITY_STREAMING, "https://twitch.tv/openmp");
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (strcmp(command, "bot"))
	{
		return 1;
	}

	new subcommand[DISCORD_COMMAND_NAME_SIZE], value[400];
	DBR_GetInteractionSubcommand(interaction, subcommand);

	if (!strcmp(subcommand, "name"))
	{
		DBR_GetInteractionOptionString(interaction, "value", value);
		DBR_SetBotUsername(value);
	}
	else if (!strcmp(subcommand, "avatar"))
	{
		DBR_GetInteractionOptionString(interaction, "file", value);
		DBR_SetBotAvatar(value);
	}
	else if (!strcmp(subcommand, "banner"))
	{
		DBR_GetInteractionOptionString(interaction, "file", value);
		DBR_SetBotBanner(value);
	}
	else if (!strcmp(subcommand, "description"))
	{
		DBR_GetInteractionOptionString(interaction, "value", value);
		DBR_SetBotDescription(value);
	}
	else if (!strcmp(subcommand, "nickname"))
	{
		new DiscordGuild:server;
		DBR_GetInteractionGuild(interaction, server);
		DBR_GetInteractionOptionString(interaction, "value", value);
		DBR_SetBotNickname(server, value);
	}
	// When Discord rejects the change (file not found, rate limit...),
	// DBR_OnActionFail receives the reason.
	return DBR_RespondInteraction(interaction, "Request sent to Discord.", .ephemeral = true);
}

public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	printf("[discord] %s failed (HTTP %d, code %d): %s", action, http_status, error_code, message);
	return 1;
}
