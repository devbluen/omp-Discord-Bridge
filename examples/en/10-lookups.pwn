/*
	Example 10 - Looking up servers, members, users, roles and channels

	There are two ways to get information:

	  1. Getters (DBR_Get...): read what the plugin already has in memory.
	     They are instant, but return 0 when the data is not loaded yet.

	  2. Fetch (DBR_Fetch...): load up-to-date data from Discord. The answer
	     arrives in a callback; after that the getters work too.

	Commands in this example:
	  /server          server details (DBR_FetchGuild)
	  /profile member  member profile (DBR_FetchGuildMember)
	  /role role       role details (DBR_FetchRole)
	  /channel channel channel details (DBR_FetchChannel)
	  /user id         any Discord user by ID (DBR_FetchUser)

	It also shows a welcome message that only uses getters.
*/

#include <open.mp>
#include <discord-bridge>

#define WELCOME_CHANNEL "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("server", "Shows the server details");

	new DiscordCommand:profile = DBR_CreateCommand("profile", "Shows a member profile");
	DBR_AddCommandOption(profile, DISCORD_OPTION_USER, "member", "Member", true);

	new DiscordCommand:role = DBR_CreateCommand("role", "Shows role details");
	DBR_AddCommandOption(role, DISCORD_OPTION_ROLE, "role", "Role", true);

	new DiscordCommand:channel = DBR_CreateCommand("channel", "Shows channel details");
	DBR_AddCommandOption(channel, DISCORD_OPTION_CHANNEL, "channel", "Channel", true);

	new DiscordCommand:userCommand = DBR_CreateCommand("user", "Looks up a user by ID");
	DBR_AddCommandOption(userCommand, DISCORD_OPTION_STRING, "id", "User ID", true);
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	// Every lookup takes a moment: show "thinking..." and pass the interaction
	// on to the callback so it can respond ("i" = one integer).
	DBR_DeferInteraction(interaction, .ephemeral = true);

	if (!strcmp(command, "server"))
	{
		new DiscordGuild:server;
		DBR_GetInteractionGuild(interaction, server);
		return DBR_FetchGuild(server, "OnServerLoaded", "i", _:interaction);
	}
	if (!strcmp(command, "profile"))
	{
		new DiscordGuild:server, DiscordUser:member;
		DBR_GetInteractionGuild(interaction, server);
		DBR_GetInteractionOptionUser(interaction, "member", member);
		return DBR_FetchGuildMember(server, member, "OnMemberLoaded", "i", _:interaction);
	}
	if (!strcmp(command, "role"))
	{
		new DiscordGuild:server, DiscordRole:role;
		DBR_GetInteractionGuild(interaction, server);
		DBR_GetInteractionOptionRole(interaction, "role", role);
		return DBR_FetchRole(server, role, "OnRoleLoaded", "i", _:interaction);
	}
	if (!strcmp(command, "channel"))
	{
		new DiscordChannel:channel;
		DBR_GetInteractionOptionChannel(interaction, "channel", channel);
		return DBR_FetchChannel(channel, "OnChannelLoaded", "i", _:interaction);
	}
	if (!strcmp(command, "user"))
	{
		new id[DISCORD_ID_SIZE];
		DBR_GetInteractionOptionString(interaction, "id", id);
		new DiscordUser:target = DBR_FindUserByID(id);
		if (target == DISCORD_INVALID_USER)
		{
			return DBR_RespondInteraction(interaction, "That does not look like a valid ID.");
		}
		return DBR_FetchUser(target, "OnUserLoaded", "i", _:interaction);
	}
	return 1;
}

// Fetch callbacks: first comes what was loaded (0 on failure), then the values
// you passed.

forward OnServerLoaded(DiscordGuild:server, DiscordInteraction:interaction);
public OnServerLoaded(DiscordGuild:server, DiscordInteraction:interaction)
{
	if (server == DISCORD_INVALID_GUILD)
	{
		return DBR_RespondInteraction(interaction, "Could not load the server.");
	}

	new name[100], description[256], icon[DISCORD_URL_SIZE], owner[DISCORD_ID_SIZE], members, text[32];
	DBR_GetGuildName(server, name);
	DBR_GetGuildDescription(server, description);
	DBR_GetGuildIconURL(server, icon);
	DBR_GetGuildOwnerID(server, owner);
	DBR_GetGuildTotalMembers(server, members);

	new DiscordEmbed:embed = DBR_CreateEmbed(name, description[0] ? description : "No description.", .colour = 0x5865F2);
	if (icon[0])
	{
		DBR_SetEmbedThumbnail(embed, icon);
	}
	valstr(text, members);
	DBR_AddEmbedField(embed, "Members", text, true);
	format(text, sizeof text, "<@%s>", owner);
	DBR_AddEmbedField(embed, "Owner", text, true);
	return DBR_RespondInteractionEmbed(interaction, embed);
}

forward OnMemberLoaded(DiscordGuild:server, DiscordUser:member, DiscordInteraction:interaction);
public OnMemberLoaded(DiscordGuild:server, DiscordUser:member, DiscordInteraction:interaction)
{
	if (member == DISCORD_INVALID_USER)
	{
		return DBR_RespondInteraction(interaction, "That user is not in the server.");
	}

	new name[DISCORD_USERNAME_SIZE], username[DISCORD_USERNAME_SIZE], joined[DISCORD_TIMESTAMP_SIZE], avatar[DISCORD_URL_SIZE];
	new roles, timeout, text[32], bool:isBot;
	DBR_GetGuildMemberDisplayName(server, member, name);   // nickname > global name > username
	DBR_GetUserName(member, username);
	DBR_GetGuildMemberJoinedAt(server, member, joined);
	DBR_GetGuildMemberAvatarURL(server, member, avatar);
	DBR_GetGuildMemberRoleCount(server, member, roles);
	DBR_GetGuildMemberTimeout(server, member, timeout);
	DBR_IsUserBot(member, isBot);

	new DiscordEmbed:embed = DBR_CreateEmbed(name, .colour = 0x57F287);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_AddEmbedField(embed, "Username", username, true);
	DBR_AddEmbedField(embed, "Bot", isBot ? "Yes" : "No", true);
	DBR_AddEmbedField(embed, "Joined at", joined, false);
	valstr(text, roles);
	DBR_AddEmbedField(embed, "Roles", text, true);
	format(text, sizeof text, "%d second(s)", timeout);
	DBR_AddEmbedField(embed, "Timeout left", text, true);
	return DBR_RespondInteractionEmbed(interaction, embed);
}

forward OnRoleLoaded(DiscordRole:role, DiscordInteraction:interaction);
public OnRoleLoaded(DiscordRole:role, DiscordInteraction:interaction)
{
	if (role == DISCORD_INVALID_ROLE)
	{
		return DBR_RespondInteraction(interaction, "Could not load the role.");
	}

	new name[100], colour, position, bool:hoisted, bool:mentionable, text[128];
	DBR_GetRoleName(role, name);
	DBR_GetRoleColour(role, colour);
	DBR_GetRolePosition(role, position);
	DBR_IsRoleHoist(role, hoisted);
	DBR_IsRoleMentionable(role, mentionable);

	format(text, sizeof text, "**%s**\nColour: #%06x | Position: %d | Hoisted: %s | Mentionable: %s",
		name, colour, position, hoisted ? "yes" : "no", mentionable ? "yes" : "no");
	return DBR_RespondInteraction(interaction, text);
}

forward OnChannelLoaded(DiscordChannel:channel, DiscordInteraction:interaction);
public OnChannelLoaded(DiscordChannel:channel, DiscordInteraction:interaction)
{
	if (channel == DISCORD_INVALID_CHANNEL)
	{
		return DBR_RespondInteraction(interaction, "Could not load the channel.");
	}

	new name[100], topic[256], slowmode, DiscordChannelType:type, text[400];
	DBR_GetChannelName(channel, name);
	DBR_GetChannelTopic(channel, topic);
	DBR_GetChannelSlowmode(channel, slowmode);
	DBR_GetChannelType(channel, type);

	format(text, sizeof text, "**#%s** (type %d)\nTopic: %s\nSlowmode: %d second(s)",
		name, _:type, topic[0] ? topic : "none", slowmode);
	return DBR_RespondInteraction(interaction, text);
}

forward OnUserLoaded(DiscordUser:target, DiscordInteraction:interaction);
public OnUserLoaded(DiscordUser:target, DiscordInteraction:interaction)
{
	if (target == DISCORD_INVALID_USER)
	{
		return DBR_RespondInteraction(interaction, "User not found.");
	}

	new username[DISCORD_USERNAME_SIZE], globalName[DISCORD_USERNAME_SIZE], avatar[DISCORD_URL_SIZE], banner[DISCORD_URL_SIZE];
	DBR_GetUserName(target, username);
	DBR_GetUserGlobalName(target, globalName);
	DBR_GetUserAvatarURL(target, avatar, .size = 512);

	new DiscordEmbed:embed = DBR_CreateEmbed(globalName[0] ? globalName : username, .colour = 0xEB459E);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_AddEmbedField(embed, "Username", username, true);
	if (DBR_GetUserBannerURL(target, banner))
	{
		DBR_SetEmbedImage(embed, banner);
	}
	return DBR_RespondInteractionEmbed(interaction, embed);
}

// Getters work right away in events, because the member just arrived.
public DBR_OnGuildMemberAdd(DiscordGuild:guild, DiscordUser:user)
{
	new name[DISCORD_USERNAME_SIZE], server[100], avatar[DISCORD_URL_SIZE], text[160];
	DBR_GetGuildMemberDisplayName(guild, user, name);
	DBR_GetGuildName(guild, server);
	DBR_GetUserAvatarURL(user, avatar);

	format(text, sizeof text, "Welcome to **%s**, %s!", server, name);
	new DiscordEmbed:embed = DBR_CreateEmbed("New member", text, .colour = 0x57F287);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_SendChannelEmbedMessage(DBR_FindChannelByID(WELCOME_CHANNEL), embed);
	return 1;
}
