/*
	Example 01 - Getting started

	What you will learn:
	  - how to know the bot is connected (DBR_OnReady);
	  - how to send a message to a channel;
	  - how to use a callback to know whether the message was sent;
	  - how to read chat messages and reply (!ping);
	  - how to find out why something failed (DBR_OnActionFail).

	Before running:
	  1. Configure the bot token (see the main README).
	  2. Replace GENERAL_CHANNEL with the ID of a channel in your Discord server
	     (Discord > Settings > Advanced > Developer Mode, then right-click the
	     channel > Copy ID).
	  3. To read message text, enable the "Message Content" intent in the
	     Discord Developer Portal.
*/

#include <open.mp>
#include <discord-bridge>

#define GENERAL_CHANNEL "123456789012345678"

new DiscordChannel:gGeneralChannel = DISCORD_INVALID_CHANNEL;

main()
{
}

public OnGameModeInit()
{
	// A channel handle can be created from its ID at any time, even before the
	// bot has finished connecting.
	gGeneralChannel = DBR_FindChannelByID(GENERAL_CHANNEL);
	return 1;
}

// Called once, when the bot is connected and the servers have been loaded.
public DBR_OnReady()
{
	print("[discord] Bot connected!");

	// A plain message: just text.
	DBR_SendChannelMessage(gGeneralChannel, "The server just started! :tada:");
	return 1;
}

public OnPlayerConnect(playerid)
{
	new name[MAX_PLAYER_NAME + 1], text[96];
	GetPlayerName(playerid, name, sizeof name);
	format(text, sizeof text, "**%s** joined the server.", name);

	// A message with a callback: when Discord answers, "OnJoinNoticeSent" is
	// called. The first parameter is always the created message; your own
	// values follow, described by the format ("i" = one integer).
	DBR_SendChannelMessage(gGeneralChannel, text, "OnJoinNoticeSent", "i", playerid);
	return 1;
}

forward OnJoinNoticeSent(DiscordMessage:message, playerid);
public OnJoinNoticeSent(DiscordMessage:message, playerid)
{
	// DISCORD_INVALID_MESSAGE means sending failed.
	if (message == DISCORD_INVALID_MESSAGE)
	{
		printf("[discord] Could not announce player %d.", playerid);
		return 1;
	}

	new id[DISCORD_ID_SIZE];
	DBR_GetMessageID(message, id);
	printf("[discord] Player %d announced (message %s).", playerid, id);
	return 1;
}

// Called for every new message in any channel the bot can see.
public DBR_OnMessageCreate(DiscordMessage:message)
{
	// Ignore messages from bots, including the bot itself.
	new DiscordUser:author, bool:isBot;
	DBR_GetMessageAuthor(message, author);
	DBR_IsUserBot(author, isBot);
	if (isBot)
	{
		return 1;
	}

	new content[128];
	DBR_GetMessageContent(message, content);
	if (content[0] != '!')
	{
		return 1;
	}

	if (!strcmp(content, "!ping", true))
	{
		DBR_ReplyMessage(message, "Pong! :ping_pong:");
	}
	else if (!strcmp(content, "!players", true))
	{
		new reply[64], online;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i))
			{
				online++;
			}
		}
		format(reply, sizeof reply, "There are **%d** player(s) online.", online);
		DBR_ReplyMessage(message, reply);
	}
	return 1;
}

// Every action Discord rejects ends up here: missing permissions, unknown
// channel, rate limits...
public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	printf("[discord] %s failed (HTTP %d, code %d): %s", action, http_status, error_code, message);
	return 1;
}
