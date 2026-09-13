/*
	Example 12 - Managing messages

	What you will learn:
	  - replying to, reacting to, editing and deleting messages;
	  - pinning messages from the context menu;
	  - deleting many messages at once (/clear);
	  - deleting a message by ID;
	  - sending a direct message (DM) to a user;
	  - tracking reactions.

	To read message text, enable the "Message Content" intent in the Discord
	Developer Portal.
*/

#include <open.mp>
#include <discord-bridge>

#define NOTICE_CHANNEL "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:clear = DBR_CreateCommand("clear", "Deletes recent messages in this channel");
	DBR_SetCommandPermissions(clear, DISCORD_COMMAND_MODERATORS);
	new DiscordCommandOption:amount = DBR_AddCommandOption(clear, DISCORD_OPTION_INTEGER, "amount", "From 1 to 100", true);
	DBR_SetOptionRange(amount, 1.0, 100.0);

	new DiscordCommand:remove = DBR_CreateCommand("remove", "Deletes a message by ID");
	DBR_SetCommandPermissions(remove, DISCORD_COMMAND_MODERATORS);
	DBR_AddCommandOption(remove, DISCORD_OPTION_STRING, "id", "Message ID", true);

	new DiscordCommand:dm = DBR_CreateCommand("dm", "Sends a direct message");
	DBR_SetCommandPermissions(dm, DISCORD_COMMAND_ADMINISTRATORS);
	DBR_AddCommandOption(dm, DISCORD_OPTION_USER, "user", "Recipient", true);
	DBR_AddCommandOption(dm, DISCORD_OPTION_STRING, "text", "Message", true);

	// Right-click a message > Apps > Pin/Unpin.
	DBR_CreateCommand("Pin", .type = DISCORD_COMMAND_MESSAGE);
	DBR_CreateCommand("Unpin", .type = DISCORD_COMMAND_MESSAGE);
	return 1;
}

public DBR_OnReady()
{
	// Send a notice, wait for Discord to create it and edit it afterwards.
	DBR_SendChannelMessage(DBR_FindChannelByID(NOTICE_CHANNEL), "Server starting...", "OnNoticeSent");
	return 1;
}

forward OnNoticeSent(DiscordMessage:message);
public OnNoticeSent(DiscordMessage:message)
{
	if (message != DISCORD_INVALID_MESSAGE)
	{
		SetTimerEx("FinishNotice", 5000, false, "i", _:message);
	}
	return 1;
}

forward FinishNotice(DiscordMessage:message);
public FinishNotice(DiscordMessage:message)
{
	DBR_EditMessage(message, "Server **online**! :white_check_mark:");
	return 1;
}

public DBR_OnMessageCreate(DiscordMessage:message)
{
	new DiscordUser:author, bool:isBot, content[128];
	DBR_GetMessageAuthor(message, author);
	DBR_IsUserBot(author, isBot);
	if (isBot)
	{
		return 1;
	}

	DBR_GetMessageContent(message, content);
	if (content[0] == '\0')
	{
		return 1;
	}

	if (!strcmp(content, "!hi", true))
	{
		// Reply quoting the message, without pinging the author.
		DBR_ReplyMessage(message, "Hi! How are you?", .mention_author = false);
		// React with an emoji. Unicode emojis work when the file is saved as
		// UTF-8; server emojis use DBR_CreateEmoji("name", "id").
		DBR_CreateReaction(message, DBR_CreateEmoji("👋"));
	}
	else if (strfind(content, "badword", true) != -1)
	{
		DBR_DeleteMessage(message);
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "clear"))
	{
		new amount, DiscordChannel:channel;
		DBR_GetInteractionOptionInt(interaction, "amount", amount);
		DBR_GetInteractionChannel(interaction, channel);
		DBR_DeferInteraction(interaction, .ephemeral = true);
		// Messages older than 14 days cannot be bulk deleted.
		return DBR_BulkDeleteMessages(channel, amount, "OnCleared", "i", _:interaction);
	}
	if (!strcmp(command, "remove"))
	{
		new id[DISCORD_ID_SIZE], DiscordChannel:channel;
		DBR_GetInteractionOptionString(interaction, "id", id);
		DBR_GetInteractionChannel(interaction, channel);
		DBR_DeleteMessageByID(channel, id, "Deleted by command");
		return DBR_RespondInteraction(interaction, "Delete request sent.", .ephemeral = true);
	}
	if (!strcmp(command, "dm"))
	{
		new DiscordUser:recipient, text[1000];
		DBR_GetInteractionOptionUser(interaction, "user", recipient);
		DBR_GetInteractionOptionString(interaction, "text", text);
		DBR_SendDirectMessage(recipient, DBR_CreateMessageBuilder(text), "OnDirectMessageSent", "i", _:interaction);
		return DBR_DeferInteraction(interaction, .ephemeral = true);
	}
	if (!strcmp(command, "Pin") || !strcmp(command, "Unpin"))
	{
		new DiscordMessage:target;
		DBR_GetInteractionMessage(interaction, target);
		if (!strcmp(command, "Pin"))
		{
			DBR_PinMessage(target);
		}
		else
		{
			DBR_UnpinMessage(target);
		}
		return DBR_RespondInteraction(interaction, "Done!", .ephemeral = true);
	}
	return 1;
}

forward OnCleared(deleted, DiscordInteraction:interaction);
public OnCleared(deleted, DiscordInteraction:interaction)
{
	new text[64];
	format(text, sizeof text, ":broom: %d message(s) deleted.", deleted);
	return DBR_RespondInteraction(interaction, text);
}

forward OnDirectMessageSent(DiscordMessage:message, DiscordInteraction:interaction);
public OnDirectMessageSent(DiscordMessage:message, DiscordInteraction:interaction)
{
	// DMs fail when the user blocks messages from server members.
	if (message == DISCORD_INVALID_MESSAGE)
	{
		return DBR_RespondInteraction(interaction, "Could not send the DM.");
	}
	return DBR_RespondInteraction(interaction, "Message sent!");
}

public DBR_OnMessageReaction(DiscordMessage:message, DiscordUser:reaction_user, DiscordEmoji:emoji, DiscordMessageReactionType:reaction_type)
{
	#pragma unused message
	if (reaction_type != DISCORD_REACTION_ADD)
	{
		return 1;
	}
	new name[DISCORD_USERNAME_SIZE], emojiName[DISCORD_EMOJI_NAME_SIZE];
	DBR_GetUserName(reaction_user, name);
	DBR_GetEmojiName(emoji, emojiName);
	printf("[discord] %s reacted with %s", name, emojiName);
	return 1;
}
