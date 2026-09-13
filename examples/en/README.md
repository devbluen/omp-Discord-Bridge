# Discord Bridge examples

> Versão em português: [../pt-BR](../pt-BR/README.md)

Each file is a small, standalone gamemode about a single topic. If you are just
starting, read them in order: each example only uses what the previous ones
already explained.

| File | Topic |
| --- | --- |
| [01-getting-started.pwn](01-getting-started.pwn) | Connecting, sending messages, callbacks, reading chat (`!ping`) and handling errors |
| [02-embeds.pwn](02-embeds.pwn) | Full embeds, several embeds in one message and a self-updating panel |
| [03-slash-commands.pwn](03-slash-commands.pwn) | `/` commands, options, choices, permissions, subcommands, autocomplete and context menus |
| [04-buttons.pwn](04-buttons.pwn) | Button styles, clicks, prefix handlers and updating the button's message |
| [05-select-menus.pwn](05-select-menus.pwn) | Option, user, role and channel menus |
| [06-modals.pwn](06-modals.pwn) | Forms with text fields and a menu, opened from a command or a button |
| [07-components-v2.pwn](07-components-v2.pwn) | Messages built from blocks: containers, sections, galleries and separators |
| [08-interaction-responses.pwn](08-interaction-responses.pwn) | Every way to respond: private, deferred, follow-up, edit and delete |
| [09-moderation.pwn](09-moderation.pwn) | Kick, ban, unban, timeout, nickname, roles and voice |
| [10-lookups.pwn](10-lookups.pwn) | Loading servers, members, users, roles and channels |
| [11-bot-profile.pwn](11-bot-profile.pwn) | Bot status, activities, name, avatar, banner and description |
| [12-messages.pwn](12-messages.pwn) | Replying, reacting, editing, pinning, clearing a channel and sending DMs |

## How to run an example

1. Configure the bot token as described in the [main README](../../README.md#3-configure-the-token).
2. Replace the IDs at the top of the file (`#define ..._CHANNEL`) with IDs from your server.
3. Compile the file as a gamemode and start the server.

Run one example at a time. `DBR_On...` callbacks are delivered to the first
script that has them, so two examples loaded together would compete for the
same events.

> Global commands may take a few minutes to show up in Discord. To test
> quickly, pass your server: `DBR_CreateCommand(..., DBR_FindGuildByID("ID"))`.
