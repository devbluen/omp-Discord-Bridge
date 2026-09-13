/*
	Example 06 - Modals (forms)

	What you will learn:
	  - opening a modal from a command and from a button;
	  - short and long text fields, required or optional;
	  - a select menu inside the modal;
	  - reading the answers in DBR_OnModalSubmit.

	Modal rules:
	  - they can only be opened as the FIRST response to a command or button;
	  - they hold up to 5 components;
	  - DBR_ShowModal consumes the modal (no need to destroy it).
*/

#include <open.mp>
#include <discord-bridge>

#define FORM_CHANNEL  "123456789012345678"
#define STAFF_CHANNEL "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("whitelist", "Sends a whitelist application", .callback = "Cmd_Whitelist");
	return 1;
}

public DBR_OnReady()
{
	// A message with a button that opens the same form.
	new DiscordComponent:row = DBR_CreateActionRow();
	DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_PRIMARY, "Apply for whitelist", "open_whitelist"));

	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("Want to play on the server? Click the button and fill in the form.");
	DBR_AddBuilderComponent(message, row);
	DBR_SendMessage(DBR_FindChannelByID(FORM_CHANNEL), message);
	return 1;
}

OpenForm(DiscordInteraction:interaction)
{
	// The custom id "whitelist" identifies the modal when it is submitted.
	new DiscordModal:modal = DBR_CreateModal("whitelist", "Whitelist application");

	// Informational text at the top.
	DBR_AddModalComponent(modal, DBR_CreateTextDisplay("Answer carefully. The staff reviews applications within 24h."));

	// Short, required field.
	DBR_AddModalTextInput(modal, "nick", "In-game nickname", .placeholder = "e.g. Carl_Johnson", .min_length = 3, .max_length = MAX_PLAYER_NAME);

	// Long field (paragraph).
	DBR_AddModalTextInput(modal, "story", "Character story", DISCORD_TEXT_INPUT_PARAGRAPH, .min_length = 50, .max_length = 1000, .description = "Where they come from and what they want.");

	// Optional field.
	DBR_AddModalTextInput(modal, "referral", "Who invited you?", .required = false);

	// A select menu inside the modal: wrap it in a label.
	new DiscordComponent:faction = DBR_CreateSelectMenu(DISCORD_SELECT_STRING, "faction", "Pick a faction");
	DBR_AddSelectMenuOption(faction, "Police", "police");
	DBR_AddSelectMenuOption(faction, "Medics", "medics");
	DBR_AddSelectMenuOption(faction, "Civilian", "civilian");
	DBR_AddModalComponent(modal, DBR_CreateLabel("Desired faction", faction));

	return DBR_ShowModal(interaction, modal);
}

forward Cmd_Whitelist(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Whitelist(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	return OpenForm(interaction);
}

public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	if (!strcmp(custom_id, "open_whitelist"))
	{
		return OpenForm(interaction);
	}
	return 1;
}

public DBR_OnModalSubmit(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	if (strcmp(custom_id, "whitelist"))
	{
		return 1;
	}

	// Each field is read by the custom id you gave it.
	new nick[MAX_PLAYER_NAME + 1], story[1001], referral[64], faction[16];
	DBR_GetInteractionValue(interaction, "nick", nick);
	DBR_GetInteractionValue(interaction, "story", story);
	DBR_GetInteractionValue(interaction, "faction", faction);
	if (!DBR_GetInteractionValue(interaction, "referral", referral) || referral[0] == '\0')
	{
		referral = "Nobody";
	}

	new author[DISCORD_USERNAME_SIZE];
	DBR_GetUserName(user, author);

	// Send the application to the staff.
	new DiscordEmbed:embed = DBR_CreateEmbed("New whitelist application", story, .colour = 0xFEE75C);
	DBR_AddEmbedField(embed, "Nick", nick, true);
	DBR_AddEmbedField(embed, "Faction", faction, true);
	DBR_AddEmbedField(embed, "Referral", referral, true);
	DBR_AddEmbedField(embed, "Discord", author, true);
	DBR_SendChannelEmbedMessage(DBR_FindChannelByID(STAFF_CHANNEL), embed);

	// And confirm to the applicant.
	return DBR_RespondInteraction(interaction, "Application sent! Wait for the staff to answer.", .ephemeral = true);
}
