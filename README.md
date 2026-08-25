# Discord Bridge for open.mp

Discord Bridge connects open.mp and SA-MP servers to Discord. It replaces
`discord-connector` and keeps its Pawn API compatible.

## Install

Download an archive from the [releases page](https://github.com/itsneufox/omp-Discord-Bridge/releases).
Copy the files into your server:

- open.mp: copy the contents of `components/` into the server's `components/` directory;
- SA-MP: copy that same `components/` content into the server's `plugins/` directory;
- copy everything in `include/` into the server's Pawn include directory.

There is only one binary. `components/discord-bridge.so` (or `.dll`) exports
both the open.mp component entry point and the classic SA-MP plugin entry
points. You still need the build that matches the server architecture; SA-MP
servers require the 32-bit build.

On Linux, choose a `static` archive for the simplest setup. A `dynssl` archive
needs compatible OpenSSL runtime libraries. Copy the complete `components/`
directory so any accompanying `libboost_system.so` file stays beside the
binary; for SA-MP, place both files in `plugins/`.

For SA-MP, replace `discord-connector` with `discord-bridge` in `server.cfg`:

```ini
plugins discord-bridge
```

Existing scripts that include `discord-connector` and use `DCC_*` natives can
keep doing so. New scripts can include `discord-bridge` and use the renamed
API.

## Configure

Create a bot in the [Discord Developer Portal](https://discord.com/developers/applications)
and invite it to your server. Give it the intents your script needs.

Set the token with an environment variable:

```sh
export DCC_BOT_TOKEN="your-bot-token"
```

The plugin also reads these server configuration keys:

- `discord_bot_token`
- `discord_bot_intents` (default: `131071`)
- `discord.channel_id` or `discord.channel_name` for `FindDiscordConfiguredChannel`

Use `DCC_BOT_INTENTS` to override the default intent mask. `DCC_CHANNEL_ID`
and `DCC_CHANNEL_NAME` override the configured target channel.

The old `discord.bot_token` and `discord.intents` keys remain supported.
Enable privileged intents in the Developer Portal when Discord requires them.

Wait for `OnDiscordReady` or `DCC_OnReady` before relying on gateway-backed
lookups. Initial guild data is available at that point, but large guild member
data may continue loading. Network work runs in the background and does not
block the server tick.

## Build from source

You need a C++17 compiler, CMake 3.19 or newer, OpenSSL, and Boost.System.
Clone the repository recursively so the open.mp and AMX SDKs are available:

```sh
git clone --recursive https://github.com/itsneufox/omp-Discord-Bridge
cd omp-Discord-Bridge
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build outputs:

- `build/plugins/discord-bridge.so` or `.dll` — the single binary for both open.mp and SA-MP;
- `build/pawno/include/` for the generated Pawn includes.

For a 32-bit Linux build, add `-DDCC_BUILD_32BIT=ON` when configuring. On
Windows with Visual Studio, select the 32-bit generator with `-A Win32`.
Set `-DDCC_VERSION=MAJOR.MINOR.PATCH` to override the default version (`1.0.0`).
