/*
	Example 09 - Member moderation

	A /mod command with subcommands:
	  /mod kick     member [reason]
	  /mod ban      member [reason] [delete_days]
	  /mod unban    id [reason]
	  /mod timeout  member minutes [reason]
	  /mod nickname member [nickname]         (empty removes the nickname)
	  /mod role     member role add
	  /mod voice    member                    (disconnects from voice)

	Important points:
	  - the bot needs the permissions in Discord (kick, ban, moderate members,
	    manage nicknames/roles) and its role must be ABOVE the role of the
	    member it acts on;
	  - reasons show up in the server audit log;
	  - when Discord rejects an action, DBR_OnActionFail tells you why.
*/

#include <open.mp>
#include <discord-bridge>

#define LOG_CHANNEL "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:mod = DBR_CreateCommand("mod", "Moderation tools");
	// Only members who can moderate see the command.
	DBR_SetCommandPermissions(mod, DISCORD_COMMAND_MODERATORS);

	new DiscordCommandOption:sub;

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "kick", "Kicks a member");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Who gets kicked", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "reason", "Reason", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "ban", "Bans a member");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Who gets banned", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "reason", "Reason", false, sub);
	new DiscordCommandOption:days = DBR_AddCommandOption(mod, DISCORD_OPTION_INTEGER, "delete_days", "Delete messages from the last N days", false, sub);
	DBR_SetOptionRange(days, 0.0, 7.0);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "unban", "Removes a ban");
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "id", "ID of the banned user", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "reason", "Reason", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "timeout", "Times out a member");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Who gets timed out", true, sub);
	new DiscordCommandOption:minutes = DBR_AddCommandOption(mod, DISCORD_OPTION_INTEGER, "minutes", "Duration (0 removes the timeout)", true, sub);
	DBR_SetOptionRange(minutes, 0.0, 40320.0);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "reason", "Reason", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "nickname", "Changes a member nickname");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Member", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "nickname", "New nickname (empty removes it)", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "role", "Adds or removes a role");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Member", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_ROLE, "role", "Role", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_BOOLEAN, "add", "True adds, false removes", true, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "voice", "Disconnects a member from voice");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "member", "Member", true, sub);
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	if (strcmp(command, "mod"))
	{
		return 1;
	}

	new subcommand[DISCORD_COMMAND_NAME_SIZE], reason[128];
	new DiscordGuild:server, DiscordUser:member;
	DBR_GetInteractionSubcommand(interaction, subcommand);
	DBR_GetInteractionGuild(interaction, server);
	DBR_GetInteractionOptionUser(interaction, "member", member);
	if (!DBR_GetInteractionOptionString(interaction, "reason", reason))
	{
		reason = "No reason given";
	}

	if (!strcmp(subcommand, "kick"))
	{
		// Extra check on top of the command permission.
		if (!DBR_HasInteractionPermission(interaction, DISCORD_PERMISSION_KICK_MEMBERS))
		{
			return DBR_RespondInteraction(interaction, "You cannot kick members.", .ephemeral = true);
		}
		DBR_KickGuildMember(server, member, reason);
		LogAction(user, "kicked", member, reason);
		return DBR_RespondInteraction(interaction, "Member kicked.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "ban"))
	{
		if (!DBR_HasInteractionPermission(interaction, DISCORD_PERMISSION_BAN_MEMBERS))
		{
			return DBR_RespondInteraction(interaction, "You cannot ban members.", .ephemeral = true);
		}
		new days;
		DBR_GetInteractionOptionInt(interaction, "delete_days", days);
		DBR_BanGuildMember(server, member, reason, days * 86400);
		LogAction(user, "banned", member, reason);
		return DBR_RespondInteraction(interaction, "Member banned.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "unban"))
	{
		// A banned user is no longer in the server: use the ID directly.
		new id[DISCORD_ID_SIZE];
		DBR_GetInteractionOptionString(interaction, "id", id);
		new DiscordUser:banned = DBR_FindUserByID(id);
		if (banned == DISCORD_INVALID_USER)
		{
			return DBR_RespondInteraction(interaction, "Invalid ID.", .ephemeral = true);
		}
		DBR_UnbanGuildMember(server, banned, reason);
		return DBR_RespondInteraction(interaction, "Ban removed.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "timeout"))
	{
		new minutes;
		DBR_GetInteractionOptionInt(interaction, "minutes", minutes);
		DBR_SetGuildMemberTimeout(server, member, minutes * 60, reason);
		LogAction(user, minutes ? "timed out" : "removed the timeout of", member, reason);
		return DBR_RespondInteraction(interaction, minutes ? "Member timed out." : "Timeout removed.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "nickname"))
	{
		new nickname[DISCORD_NICKNAME_SIZE];
		DBR_GetInteractionOptionString(interaction, "nickname", nickname);
		DBR_SetGuildMemberNickname(server, member, nickname, "Changed by command");
		return DBR_RespondInteraction(interaction, "Nickname updated.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "role"))
	{
		new DiscordRole:role, bool:add;
		DBR_GetInteractionOptionRole(interaction, "role", role);
		DBR_GetInteractionOptionBool(interaction, "add", add);
		if (add)
		{
			DBR_AddGuildMemberRole(server, member, role, "Role given by command");
		}
		else
		{
			DBR_RemoveGuildMemberRole(server, member, role, "Role removed by command");
		}
		return DBR_RespondInteraction(interaction, "Roles updated.", .ephemeral = true);
	}
	if (!strcmp(subcommand, "voice"))
	{
		DBR_DisconnectGuildMemberVoice(server, member, "Disconnected by command");
		return DBR_RespondInteraction(interaction, "Member disconnected from voice.", .ephemeral = true);
	}
	return 1;
}

LogAction(DiscordUser:moderator, const action[], DiscordUser:target, const reason[])
{
	new moderatorName[DISCORD_USERNAME_SIZE], targetName[DISCORD_USERNAME_SIZE], text[256];
	DBR_GetUserName(moderator, moderatorName);
	DBR_GetUserName(target, targetName);
	format(text, sizeof text, ":shield: **%s** %s **%s**. Reason: %s", moderatorName, action, targetName, reason);
	DBR_SendChannelMessage(DBR_FindChannelByID(LOG_CHANNEL), text);
}

public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	// Code 50013 = the bot lacks permission (or its role is below the target).
	if (error_code == 50013)
	{
		printf("[discord] %s: the bot is not allowed to do that.", action);
		return 1;
	}
	printf("[discord] %s failed (HTTP %d, code %d): %s", action, http_status, error_code, message);
	return 1;
}
