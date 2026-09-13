/*
	Example 08 - Responding to interactions

	An "interaction" is any use of a command, button, menu or modal. They all
	need a response. This example shows the ways to respond:

	  /simple    regular response, everyone sees it;
	  /private   response only the user sees (ephemeral);
	  /slow      "thinking..." and a response later (DBR_DeferInteraction);
	  /followup  response and then another message (follow-up);
	  /edit      response that is edited later;
	  /delete    response that deletes itself;
	  /rich      response with an embed and a button (message builder).

	It also shows DBR_OnInteraction, called before everything else, used here
	to block usage outside the server (in direct messages).

	Good to know:
	  - if you do not respond, the plugin acknowledges the interaction when the
	    callback returns (shows "thinking..." for commands);
	  - the interaction handle lasts 15 minutes, so you can respond after a
	    timer, a database query and so on;
	  - responding again is not an error: it becomes a follow-up message.
*/

#include <open.mp>
#include <discord-bridge>

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("simple", "Regular response");
	DBR_CreateCommand("private", "Response only you see");
	DBR_CreateCommand("slow", "Response after a few seconds");
	DBR_CreateCommand("followup", "Response with an extra message");
	DBR_CreateCommand("edit", "Response that changes later");
	DBR_CreateCommand("delete", "Response that deletes itself");
	DBR_CreateCommand("rich", "Response with an embed and a button");
	return 1;
}

// Called before any other callback. Return 0 to stop here.
public DBR_OnInteraction(DiscordInteraction:interaction, DiscordUser:user, DiscordInteractionType:type)
{
	#pragma unused user, type
	new DiscordGuild:server;
	DBR_GetInteractionGuild(interaction, server);
	if (server == DISCORD_INVALID_GUILD)
	{
		DBR_RespondInteraction(interaction, "Use the commands inside the server.", .ephemeral = true);
		return 0;
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "simple"))
	{
		return DBR_RespondInteraction(interaction, "Everyone can see this response.");
	}
	if (!strcmp(command, "private"))
	{
		return DBR_RespondInteraction(interaction, "Only you can see this message.", .ephemeral = true);
	}
	if (!strcmp(command, "slow"))
	{
		// Show "thinking..." now and respond in 3 seconds.
		DBR_DeferInteraction(interaction);
		SetTimerEx("RespondLater", 3000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "followup"))
	{
		DBR_RespondInteraction(interaction, "First response.");
		// A follow-up is sent with a message builder.
		DBR_SendInteractionFollowup(interaction, DBR_CreateMessageBuilder("And this is an extra message, just for you."), .ephemeral = true);
		return 1;
	}
	if (!strcmp(command, "edit"))
	{
		DBR_RespondInteraction(interaction, "Loading data...");
		SetTimerEx("EditResponse", 2000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "delete"))
	{
		DBR_RespondInteraction(interaction, "This message disappears in 5 seconds.");
		SetTimerEx("DeleteResponse", 5000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "rich"))
	{
		new DiscordEmbed:embed = DBR_CreateEmbed("Rich response", "With an embed and a button.", .colour = 0x57F287);

		new DiscordComponent:row = DBR_CreateActionRow();
		DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_LINK, "Documentation", "https://open.mp"));

		new DiscordMessageBuilder:message = DBR_CreateMessageBuilder();
		DBR_AddBuilderEmbed(message, embed);
		DBR_AddBuilderComponent(message, row);
		return DBR_RespondInteractionMessage(interaction, message);
	}
	return 1;
}

forward RespondLater(DiscordInteraction:interaction);
public RespondLater(DiscordInteraction:interaction)
{
	// After a defer, DBR_RespondInteraction replaces the "thinking...".
	return DBR_RespondInteraction(interaction, "Done! Processing finished.");
}

forward EditResponse(DiscordInteraction:interaction);
public EditResponse(DiscordInteraction:interaction)
{
	return DBR_EditInteractionResponse(interaction, DBR_CreateMessageBuilder("Data loaded: 42 registered players."));
}

forward DeleteResponse(DiscordInteraction:interaction);
public DeleteResponse(DiscordInteraction:interaction)
{
	return DBR_DeleteInteractionResponse(interaction);
}
