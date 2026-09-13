/*
	Example 02 - Embeds

	What you will learn:
	  - building an embed with title, description, colour, author, fields,
	    images and footer;
	  - sending an embed together with text;
	  - sending several embeds in one message (message builder);
	  - keeping a "status panel" up to date by editing the same message.

	Important rule: sending an embed CONSUMES the handle. After
	DBR_SendChannelEmbedMessage, DBR_AddBuilderEmbed or DBR_EditMessage you do
	not need to (and must not) call DBR_DeleteEmbed on it.
*/

#include <open.mp>
#include <discord-bridge>

#define STATUS_CHANNEL "123456789012345678"

new DiscordChannel:gChannel = DISCORD_INVALID_CHANNEL;
new DiscordMessage:gStatusPanel = DISCORD_INVALID_MESSAGE;

// Functions returning a tag must be declared before they are used.
forward DiscordEmbed:CreateStatusEmbed();

main()
{
}

public OnGameModeInit()
{
	gChannel = DBR_FindChannelByID(STATUS_CHANNEL);
	return 1;
}

public DBR_OnReady()
{
	SendFullEmbed();
	SendSeveralEmbeds();

	// Create the panel and keep the message to edit it later.
	DBR_SendChannelEmbedMessage(gChannel, CreateStatusEmbed(), "", "OnPanelCreated");
	return 1;
}

SendFullEmbed()
{
	// Every parameter of DBR_CreateEmbed is optional.
	new DiscordEmbed:embed = DBR_CreateEmbed("Welcome to My RP Server");

	DBR_SetEmbedDescription(embed, "A roleplay server with economy, jobs and much more.");
	DBR_SetEmbedColour(embed, 0x5865F2);                       // side bar colour (RRGGBB)
	DBR_SetEmbedURL(embed, "https://open.mp");                  // turns the title into a link
	DBR_SetEmbedAuthor(embed, "My Server Staff", "https://open.mp", "https://assets.open.mp/assets/images/assets/logo-light-trans.png");

	// Fields: name, value and whether they sit side by side (inline).
	DBR_AddEmbedField(embed, "IP", "`myserver.com:7777`", true);
	DBR_AddEmbedField(embed, "Version", "open.mp", true);
	DBR_AddEmbedField(embed, "Rules", "Read the #rules channel before playing.", false);

	DBR_SetEmbedThumbnail(embed, "https://assets.open.mp/assets/images/assets/logo-light-trans.png");
	DBR_SetEmbedImage(embed, "https://assets.open.mp/assets/images/assets/logo-light-trans.png");
	DBR_SetEmbedFooter(embed, "Sent automatically by the server");
	DBR_SetEmbedTimestamp(embed, "2026-01-01T12:00:00Z");       // ISO 8601 date (UTC)

	// The text is optional and shows above the embed.
	DBR_SendChannelEmbedMessage(gChannel, embed, "Here is the server information:");
}

SendSeveralEmbeds()
{
	// For more than one embed in the same message, use a message builder.
	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("Events this week:");
	DBR_AddBuilderEmbed(message, DBR_CreateEmbed("Race", "Saturday at 8 PM", .colour = 0xFEE75C));
	DBR_AddBuilderEmbed(message, DBR_CreateEmbed("Shooting contest", "Sunday at 6 PM", .colour = 0xED4245));
	DBR_SendMessage(gChannel, message);
}

DiscordEmbed:CreateStatusEmbed()
{
	new online, text[32];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i))
		{
			online++;
		}
	}
	format(text, sizeof text, "%d/%d", online, MAX_PLAYERS);

	new DiscordEmbed:embed = DBR_CreateEmbed("Server status", .colour = 0x57F287);
	DBR_AddEmbedField(embed, "Players online", text, true);
	DBR_AddEmbedField(embed, "State", "Online", true);
	DBR_SetEmbedFooter(embed, "Updates every minute");
	return embed;
}

forward OnPanelCreated(DiscordMessage:message);
public OnPanelCreated(DiscordMessage:message)
{
	if (message == DISCORD_INVALID_MESSAGE)
	{
		return 1;
	}
	// The handle keeps working to edit, delete or pin the message.
	gStatusPanel = message;
	SetTimer("UpdatePanel", 60000, true);
	return 1;
}

forward UpdatePanel();
public UpdatePanel()
{
	if (gStatusPanel != DISCORD_INVALID_MESSAGE)
	{
		// Edit the same message: empty text and a new embed.
		DBR_EditMessage(gStatusPanel, "", CreateStatusEmbed());
	}
	return 1;
}
