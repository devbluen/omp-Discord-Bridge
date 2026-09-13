/*
	Example 07 - Components V2

	Components V2 is Discord's newer message format: instead of text + embeds,
	the whole message is built from blocks.

	Available blocks:
	  - Text Display  : text with markdown (headings, bold, lists...);
	  - Separator     : line/space between blocks;
	  - Container     : box with a coloured bar that groups other blocks;
	  - Section       : 1 to 3 texts with an "accessory" next to them (image or button);
	  - Thumbnail     : small image, used as a section accessory;
	  - Media Gallery : gallery with 1 to 10 images;
	  - Action Row    : row of buttons or a menu, as in regular messages.

	Just add one of these blocks to the message builder: the plugin marks the
	message as Components V2 by itself. This format has no embeds, and the
	builder text becomes the first Text Display.
*/

#include <open.mp>
#include <discord-bridge>

#define PANEL_CHANNEL "123456789012345678"
#define IMAGE "https://assets.open.mp/assets/images/assets/logo-light-trans.png"

main()
{
}

public DBR_OnReady()
{
	new DiscordChannel:channel = DBR_FindChannelByID(PANEL_CHANNEL);
	SendPanel(channel);
	SendSimpleNotice(channel);
	return 1;
}

SendPanel(DiscordChannel:channel)
{
	// Box with a blue side bar.
	new DiscordComponent:box = DBR_CreateContainer(0x5865F2);

	DBR_AddComponent(box, DBR_CreateTextDisplay("# My RP Server\nWelcome! Everything you need is here."));
	DBR_AddComponent(box, DBR_CreateSeparator());

	// Section with an image next to it.
	new DiscordComponent:about = DBR_CreateSection();
	DBR_AddComponent(about, DBR_CreateTextDisplay("## About"));
	DBR_AddComponent(about, DBR_CreateTextDisplay("Realistic economy, jobs and weekly events."));
	DBR_SetSectionAccessory(about, DBR_CreateThumbnail(IMAGE, "Server logo"));
	DBR_AddComponent(box, about);

	// Section with a button next to it.
	new DiscordComponent:rules = DBR_CreateSection();
	DBR_AddComponent(rules, DBR_CreateTextDisplay("**Rules**\nRead them before joining the game."));
	DBR_SetSectionAccessory(rules, DBR_CreateButton(DISCORD_BUTTON_SECONDARY, "Read rules", "panel:rules"));
	DBR_AddComponent(box, rules);

	DBR_AddComponent(box, DBR_CreateSeparator(true, DISCORD_SPACING_LARGE));

	// Image gallery.
	new DiscordComponent:gallery = DBR_CreateMediaGallery();
	DBR_AddMediaGalleryItem(gallery, IMAGE, "City");
	DBR_AddMediaGalleryItem(gallery, IMAGE, "Beach");
	DBR_AddComponent(box, gallery);

	// Row of buttons inside the box.
	new DiscordComponent:buttons = DBR_CreateActionRow();
	DBR_AddComponent(buttons, DBR_CreateButton(DISCORD_BUTTON_SUCCESS, "I want to play", "panel:play"));
	DBR_AddComponent(buttons, DBR_CreateButton(DISCORD_BUTTON_LINK, "Website", "https://open.mp"));
	DBR_AddComponent(box, buttons);

	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder();
	DBR_AddBuilderComponent(message, box);
	DBR_SendMessage(channel, message);
}

SendSimpleNotice(DiscordChannel:channel)
{
	// Components V2 without a box: the builder text becomes the first block.
	new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("### Maintenance today at 10 PM");
	DBR_AddBuilderComponent(message, DBR_CreateSeparator());
	DBR_AddBuilderComponent(message, DBR_CreateTextDisplay("The server will be offline for about 30 minutes."));
	DBR_SendMessage(channel, message);
}

public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	if (!strcmp(custom_id, "panel:rules"))
	{
		return DBR_RespondInteraction(interaction, "1. Respect everyone.\n2. No cheating.\n3. Have fun!", .ephemeral = true);
	}
	if (!strcmp(custom_id, "panel:play"))
	{
		return DBR_RespondInteraction(interaction, "Connect to `myserver.com:7777`.", .ephemeral = true);
	}
	return 1;
}
