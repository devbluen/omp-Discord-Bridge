# Discord Bridge for open.mp and SA-MP

> Versão em português: [README.pt-BR.md](README.pt-BR.md)

Connect your open.mp or SA-MP server to Discord and control everything from
Pawn: `/` commands, buttons, menus, forms, rich messages, moderation and much
more.

```pawn
public OnGameModeInit()
{
    DBR_CreateCommand("ping", "Replies with pong", .callback = "Cmd_Ping");
}

forward Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user)
{
    return DBR_RespondInteraction(interaction, "Pong!");
}
```

## Contents

- [Features](#features)
- [Installation](#installation)
  - [1. Install the plugin](#1-install-the-plugin)
  - [2. Create the bot on Discord](#2-create-the-bot-on-discord)
  - [3. Configure the token](#3-configure-the-token)
- [Concepts in 2 minutes](#concepts-in-2-minutes)
- [Quick guide](#quick-guide)
- [Callbacks](#callbacks)
- [Troubleshooting](#troubleshooting)
- [Building the plugin](#building-the-plugin)

## Features

| Feature | What you can do | Example |
| --- | --- | --- |
| Messages | Send, reply, edit, delete, pin, react, clear a channel, DM | [12](examples/en/12-messages.pwn) |
| Embeds | Title, colour, author, fields, images, footer | [02](examples/en/02-embeds.pwn) |
| Slash commands | Options, choices, subcommands, autocomplete, permissions, context menus | [03](examples/en/03-slash-commands.pwn) |
| Buttons | Every style, prefix handlers, updating the message | [04](examples/en/04-buttons.pwn) |
| Select menus | Your own options, users, roles and channels | [05](examples/en/05-select-menus.pwn) |
| Modals | Forms with text fields, menus and file uploads | [06](examples/en/06-modals.pwn) |
| Components V2 | Messages built from containers, sections, galleries and separators | [07](examples/en/07-components-v2.pwn) |
| Interactions | Private and deferred responses, follow-ups, editing, global filter | [08](examples/en/08-interaction-responses.pwn) |
| Moderation | Kick, ban, unban, timeout, nickname, roles, voice | [09](examples/en/09-moderation.pwn) |
| Lookups | Servers, members, users, roles and channels | [10](examples/en/10-lookups.pwn) |
| Bot profile | Status, activity, name, avatar, banner, description, nickname | [11](examples/en/11-bot-profile.pwn) |

All network work runs in the background: the plugin never blocks the server.

Complete examples: [in English](examples/en/README.md) · [em português](examples/pt-BR/README.md).

## Installation

### 1. Install the plugin

Download the package from the [releases page](https://github.com/devbluen/omp-Discord-Bridge/releases)
and copy the files:

| Server | Where to copy |
| --- | --- |
| open.mp | contents of `components/` into the server's `components/` folder |
| SA-MP | contents of `components/` into `plugins/`, and `plugins discord-bridge` in `server.cfg` |
| Both | contents of `include/` into your compiler's include folder |

- There is a single binary that works on both servers. SA-MP requires the
  **32-bit** build.
- On Linux, prefer the `static` package. `dynssl` needs OpenSSL installed on
  the machine. Keep any `libboost_system.so` next to the plugin.

In your script:

```pawn
#include <discord-bridge>
```

### 2. Create the bot on Discord

1. Open the [Discord Developer Portal](https://discord.com/developers/applications)
   and click **New Application**.
2. In the **Bot** tab, click **Reset Token** and copy the token. Never share
   this token.
3. Still in **Bot**, enable under **Privileged Gateway Intents**:
   - **Server Members Intent** (member list, joins and leaves);
   - **Message Content Intent** (reading message text);
   - **Presence Intent** (members' online status).
4. In **OAuth2 > URL Generator**, tick the `bot` and `applications.commands`
   scopes, choose the permissions the bot needs and open the generated link to
   invite it to your server.

### 3. Configure the token

Pick **one** of these:

**open.mp** (`config.json`):

```json
{
    "discord": {
        "bot_token": "YOUR_TOKEN",
        "channel_id": "123456789012345678"
    }
}
```

**SA-MP** (`server.cfg`):

```ini
discord_bot_token YOUR_TOKEN
discord_channel_id 123456789012345678
```

**Environment variable** (takes priority over the files):

```sh
export DISCORD_BOT_TOKEN="YOUR_TOKEN"
```

| Setting | Environment variable | Purpose |
| --- | --- | --- |
| `discord_bot_token` | `DISCORD_BOT_TOKEN` | Bot token (required) |
| `discord_bot_intents` | `DISCORD_BOT_INTENTS` | Gateway intents (default `131071`, all of them) |
| `discord_channel_id` | `DISCORD_CHANNEL_ID` | Channel returned by `DBR_FindConfiguredChannel()` |
| `discord_channel_name` | `DISCORD_CHANNEL_NAME` | Same as above, by channel name |

Start the server. Once the bot connects, `DBR_OnReady` is called and
everything is ready to use.

## Concepts in 2 minutes

### Everything starts with `DBR_`

Every plugin function and callback uses the `DBR_` prefix (**D**iscord
**BR**idge). Type `DBR_` in your editor to see the whole API.

| Starts with | Meaning | Examples |
| --- | --- | --- |
| `DBR_Get...` / `DBR_Is...` | Reads data already loaded | `DBR_GetUserName`, `DBR_IsUserBot` |
| `DBR_Set...` | Changes something on Discord | `DBR_SetGuildMemberNickname` |
| `DBR_Create...` | Creates something (embed, button, command...) | `DBR_CreateEmbed` |
| `DBR_Fetch...` | Loads up-to-date data from Discord | `DBR_FetchGuildMember` |
| `DBR_On...` | Callback called by the plugin | `DBR_OnCommand` |

Constants and tags use `DISCORD_` and `Discord`: `DISCORD_BUTTON_PRIMARY`,
`DiscordUser:`.

### Handles

Servers, channels, users, roles and messages are represented by tagged
*handles* (`DiscordGuild:`, `DiscordChannel:`...). You can get a handle from an
ID at any time, even before the bot connects:

```pawn
new DiscordChannel:channel = DBR_FindChannelByID("123456789012345678");
```

- **Getters** (`DBR_Get...`) read what is already in memory and are instant.
- **Fetch** (`DBR_Fetch...`) loads data from Discord when it is not loaded yet.

### Asynchronous callbacks

Requests to Discord take a few milliseconds. Functions ending in
`callback[], format[], ...` call your function when the answer arrives. The
**first parameter** is what was created or loaded (0 on failure); your own
values follow:

```pawn
DBR_SendChannelMessage(channel, "Hello!", "OnSent", "i", playerid);

forward OnSent(DiscordMessage:message, playerid);
public OnSent(DiscordMessage:message, playerid)
{
    if (message == DISCORD_INVALID_MESSAGE) return 1; // failed
    // ...
    return 1;
}
```

Formats: `i`/`d` integer, `f` float, `b` boolean, `s` string, `a` array
followed by its size.

### Sending consumes

Embeds, message builders and modals are **destroyed when sent**. Build, send
and forget. Components added to a row, builder or modal belong to it.

### Errors

When Discord rejects something (missing permission, wrong ID, rate limit),
`DBR_OnActionFail` tells you why:

```pawn
public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
    printf("[discord] %s failed: %s", action, message);
    return 1;
}
```

## Quick guide

### Sending messages and embeds

```pawn
DBR_SendChannelMessage(channel, "Server online!");

new DiscordEmbed:embed = DBR_CreateEmbed("Status", "Everything is running.", .colour = 0x57F287);
DBR_AddEmbedField(embed, "Players", "12/100", true);
DBR_SendChannelEmbedMessage(channel, embed);
```

To combine text, several embeds and components, use a **message builder**:

```pawn
new DiscordMessageBuilder:message = DBR_CreateMessageBuilder("Read the rules:");
DBR_AddBuilderEmbed(message, DBR_CreateEmbed("Rules", "Respect everyone."));
DBR_AddBuilderComponent(message, buttonRow);
DBR_SendMessage(channel, message);
```

### Slash commands

```pawn
new DiscordCommand:cmd = DBR_CreateCommand("dice", "Rolls a dice", .callback = "Cmd_Dice");
new DiscordCommandOption:sides = DBR_AddCommandOption(cmd, DISCORD_OPTION_INTEGER, "sides", "Number of sides");
DBR_SetOptionRange(sides, 2.0, 100.0);

forward Cmd_Dice(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Dice(DiscordInteraction:interaction, DiscordUser:user)
{
    new sides = 6;
    DBR_GetInteractionOptionInt(interaction, "sides", sides);
    // ...
}
```

- Create commands whenever you like. The plugin publishes them once the bot is
  ready and does not publish again when nothing changed.
- Commands without `.callback` arrive in `DBR_OnCommand`.
- Pass a server (`DBR_FindGuildByID("ID")`) to register the command there only:
  it shows up immediately, great for testing. Global commands may take a few
  minutes.

> Each scope (global or one server) is **replaced as a whole**. Commands created
> by other tools for the same bot and scope are removed.

### Responding to interactions

Every interaction (command, button, menu, modal) needs a response:

| Function | When to use |
| --- | --- |
| `DBR_RespondInteraction(i, "text", .ephemeral = true)` | Quick response (private with `ephemeral`) |
| `DBR_RespondInteractionEmbed` / `DBR_RespondInteractionMessage` | Response with an embed or builder |
| `DBR_DeferInteraction(i)` | Taking a while? Show "thinking..." and respond later |
| `DBR_UpdateInteractionMessage(i, builder)` | Buttons and menus: replace the message they were clicked on |
| `DBR_SendInteractionFollowup` | Extra message after the response |
| `DBR_EditInteractionResponse` / `DBR_DeleteInteractionResponse` | Edit or delete the response |
| `DBR_ShowModal(i, modal)` | Open a form |

If you do not respond, the plugin acknowledges the interaction when the
callback returns. The handle lasts 15 minutes, so you can respond after a
timer or a database query.

### Buttons and menus

```pawn
new DiscordComponent:row = DBR_CreateActionRow();
DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_SUCCESS, "Accept", "accept"));
DBR_AddComponent(row, DBR_CreateButton(DISCORD_BUTTON_LINK, "Website", "https://open.mp"));

public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
    if (!strcmp(custom_id, "accept"))
        return DBR_RespondInteraction(interaction, "Thanks!", .ephemeral = true);
    return 1;
}
```

Menus use `DBR_CreateSelectMenu` and arrive in `DBR_OnSelectMenu`. Read the
picked values with `DBR_GetInteractionValue(interaction, "", value)`.

To organise many buttons, register a prefix handler:

```pawn
DBR_RegisterHandler(DISCORD_INTERACTION_COMPONENT, "shop:", "OnShopClick", true);
```

### Modals

```pawn
new DiscordModal:modal = DBR_CreateModal("report", "Report a player");
DBR_AddModalTextInput(modal, "nick", "Player nickname");
DBR_AddModalTextInput(modal, "reason", "What happened?", DISCORD_TEXT_INPUT_PARAGRAPH);
DBR_ShowModal(interaction, modal);

public DBR_OnModalSubmit(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
    new nick[25];
    DBR_GetInteractionValue(interaction, "nick", nick);
    return DBR_RespondInteraction(interaction, "Report received!", .ephemeral = true);
}
```

### Components V2

Add blocks such as `DBR_CreateContainer`, `DBR_CreateSection`,
`DBR_CreateTextDisplay`, `DBR_CreateMediaGallery` and `DBR_CreateSeparator` to
a message builder. The message becomes Components V2 automatically: the
builder text becomes the first block and embeds are not allowed. See
[example 07](examples/en/07-components-v2.pwn).

### Moderation

```pawn
DBR_KickGuildMember(server, member, "Reason");
DBR_BanGuildMember(server, member, "Reason", .delete_message_seconds = 86400);
DBR_UnbanGuildMember(server, DBR_FindUserByID("123..."));
DBR_SetGuildMemberTimeout(server, member, 600, "Spam");   // 10 minutes
DBR_SetGuildMemberNickname(server, member, "New nickname");
DBR_AddGuildMemberRole(server, member, role);
```

The bot's role must be **above** the role of the member it acts on. Reasons
show up in the audit log.

### Lookups

```pawn
DBR_FetchGuildMember(server, member, "OnLoaded");

forward OnLoaded(DiscordGuild:server, DiscordUser:member);
public OnLoaded(DiscordGuild:server, DiscordUser:member)
{
    new name[33], joined[33];
    DBR_GetGuildMemberDisplayName(server, member, name);
    DBR_GetGuildMemberJoinedAt(server, member, joined);
    return 1;
}
```

There are also `DBR_FetchGuild`, `DBR_FetchUser`, `DBR_FetchRole`,
`DBR_FetchChannel` and `DBR_FetchMessage`.

### Bot profile

```pawn
DBR_SetBotPresenceStatus(DISCORD_BOT_PRESENCE_IDLE);
DBR_SetBotActivity("12 players online", DISCORD_ACTIVITY_WATCHING);
DBR_SetBotAvatar("bot/avatar.png");       // also looked up in scriptfiles/
DBR_SetBotUsername("My Server");
```

## Callbacks

| Callback | When it is called |
| --- | --- |
| `DBR_OnReady()` | Bot connected and servers loaded |
| `DBR_OnDisconnected()` | Connection lost (the plugin reconnects by itself) |
| `DBR_OnActionFail(action[], http_status, error_code, message[])` | Discord rejected an action |
| `DBR_OnInteraction(interaction, user, type)` | Before any interaction; return `0` to block it |
| `DBR_OnCommand(interaction, user, command[])` | Command without its own callback |
| `DBR_OnButton(interaction, user, custom_id[])` | Button click |
| `DBR_OnSelectMenu(interaction, user, custom_id[])` | Menu selection |
| `DBR_OnModalSubmit(interaction, user, custom_id[])` | Modal submission |
| `DBR_OnAutocomplete(interaction, user, command[], option[])` | Typing in an option with autocomplete |
| `DBR_OnMessageCreate/Update/Delete(message)` | Messages created, edited or deleted |
| `DBR_OnMessageReaction(message, user, emoji, type)` | Reactions |
| `DBR_OnGuildMemberAdd/Update/Remove(guild, user)` | Members joining, changing or leaving |
| `DBR_OnGuildMemberVoiceUpdate(guild, user, channel)` | Joining and leaving voice channels |
| `DBR_OnGuildCreate/Update/Delete(guild)` | Servers |
| `DBR_OnGuildRoleCreate/Update/Delete(guild, role)` | Roles |
| `DBR_OnChannelCreate/Update/Delete(channel)` | Channels |
| `DBR_OnUserUpdate(user)` | A user's profile changed |

An interaction goes through this order: `DBR_OnInteraction` → the command
callback or a handler registered with `DBR_RegisterHandler` →
`DBR_OnCommand` / `DBR_OnButton` / `DBR_OnSelectMenu` / `DBR_OnModalSubmit` /
`DBR_OnAutocomplete`.

The full list of functions, with parameters, is in the `discord-bridge.inc`
include.

## Troubleshooting

**The bot does not connect.** Check the token and whether the privileged
intents are enabled in the Developer Portal. With the default `131071` all
three must be on; if you do not want one, adjust `discord_bot_intents`.

**The command does not show up.** Global commands may take a few minutes.
Register it in your server while testing and make sure the bot was invited
with the `applications.commands` scope. Try `Ctrl+R` in Discord.

**"The application did not respond".** Discord expects a response within 3
seconds. Do not do heavy work before responding: call `DBR_DeferInteraction`
and respond later.

**Message text arrives empty.** Enable the **Message Content Intent**.

**Kick/ban does not work.** Check `DBR_OnActionFail`. Code `50013` means
missing permission or the bot's role is below the target.

**Getters return 0.** The data is not in memory yet. Wait for `DBR_OnReady` or
use the matching `DBR_Fetch...` function.

**`Could NOT find OpenSSL` when configuring.** The dependencies are not
installed where CMake can see them. On Windows, build with the vcpkg preset
(`cmake --preset windows-x86`) instead of `cmake -S . -B build`, and make sure
`VCPKG_ROOT` is set. On Linux, install `libssl-dev` and `libboost-system-dev`.

## Building the plugin

Clone the repository recursively so the open.mp and AMX SDKs come along:

```sh
git clone --recursive https://github.com/devbluen/omp-Discord-Bridge
cd omp-Discord-Bridge
```

### Windows

The dependencies (OpenSSL and Boost) are installed automatically by
[vcpkg](https://github.com/microsoft/vcpkg) from `vcpkg.json`. You only need
**Visual Studio 2022** with the *Desktop development with C++* workload,
**CMake 3.21+** and vcpkg.

1. Install vcpkg once. Keep it in a short path such as `C:\vcpkg`: deep
   folders hit the Windows 260-character path limit while dependencies build.

   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
   setx VCPKG_ROOT C:\vcpkg
   ```

   Close and reopen the terminal so `VCPKG_ROOT` is available.

2. Build. The first run downloads and compiles OpenSSL and Boost, which takes
   about 10 to 15 minutes; later builds are fast.

   ```powershell
   cmake --preset windows-x86
   cmake --build --preset windows-x86
   ctest --preset windows-x86
   ```

Outputs in `build/windows-x86/`:

- `plugins/Release/discord-bridge.dll`, `libssl-3.dll` and `libcrypto-3.dll`:
  copy **all three** to the server (`components/` on open.mp, `plugins/` on SA-MP);
- `pawno/include/discord-bridge.inc`: the include.

Windows servers are 32-bit, so the preset always builds for Win32.

### Linux

```sh
sudo apt install build-essential cmake libssl-dev libboost-system-dev
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Outputs: `build/plugins/discord-bridge.so` and `build/pawno/include/discord-bridge.inc`.
For the 32-bit build used by SA-MP, add `-DDISCORD_BRIDGE_BUILD_32BIT=ON` and
install the `:i386` versions of the libraries (see `.github/workflows/build.yml`).

Set `-DDISCORD_BRIDGE_VERSION=X.Y.Z` on either platform to choose the version.

## AI disclosure

AI tools assisted with parts of the code and documentation. Review the source
and test the plugin on your own server before using it in production.
