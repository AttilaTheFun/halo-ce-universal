# CoreCE: Halo: Combat Evolved for Linux, Windows, and macOS

<p align="center">
  <img src="port/assets/icon/corece-icon.png" alt="CoreCE" width="384">
</p>

<p align="center">
  <a href="https://discord.gg/4q7tpz7Gs"><img src="https://img.shields.io/badge/Discord-join-5865F2?style=for-the-badge&logo=discord&logoColor=white" alt="Discord"></a>
</p>

**CoreCE** (Open Community Edition) is an open source port of the Halo: Combat Evolved decompilation, from Xbox development build 2342 and [bnunu/halo-1](https://github.com/bnunu/halo-1). It adds 128-player multiplayer, online campaign co-op, mod packs, field-of-view settings, a server browser, peer-to-peer hosting, automatic map downloads, and voice chat, and it can play Custom Edition maps.

Windows, Linux, and Apple Silicon each run the game's own code, with a new layer in place of the Xbox hardware. Windows and Linux draw with OpenGL. The Mac build draws with Metal. Sound, input, and gamepads go through SDL3. With Smooth Motion on, the game draws a frame for every refresh of the display, while the simulation stays at 30 ticks a second, as on the Xbox.

<p align="center">
  <img src="docs/images/main-menu.png" alt="Original Halo main menu running on Apple Silicon" width="512">
</p>

## Your own copy

CoreCE does not include the game data. Use your own copy of the game.

1. Start the game (`CoreCE.app` on Mac, `halo.exe` on Windows, `halo` on Linux).
2. At the first start, choose your copy in the file picker. On Linux and Windows, a copy next to the executable is used without the file picker. Where no file picker can be shown (on Linux, without the desktop portal or `zenity`), the game says to put your copy next to the executable, and stops.
3. On Mac, an extracted folder offers **Copy and Manage**, **Use This Folder**, or **Cancel**. Managed copies and imports go under `~/Library/Application Support/CoreCE/Game Data/`. Saves, profiles, and `config.toml` stay in that Application Support directory. Prior files are copied from `~/Library/Application Support/Halo OG/`, or from `Halo CE Universal` when that is the only older folder. The old folder is left in place.
4. The game sets up `maps` from your copy, then starts.

`maps` holds the Xbox maps only. If it holds Halo PC's maps instead, the game says so and stops. Custom Edition maps go in `custom_maps` beside `maps`.

On Linux and Windows, the game puts `maps` next to the executable. On Mac, see [Mac data import](docs/macos-menu-and-releases.md#independently-supplied-data).

## Download

Mac | Linux | Windows

https://github.com/mastercheese-dev/CoreCE/releases

The Mac package is Apple Silicon and macOS 26 or later. Open the DMG and drag **CoreCE.app** into Applications. No Homebrew or developer tools are needed to run it. The app is ad hoc signed and is not notarized, so macOS may ask you to open it from Privacy & Security the first time. This Mac release uses network protocol **32**, so a classic game can include another protocol 32 player. Hardcore needs this build. The Mac app looks for a newer release from this fork and asks before installing it. Linux and Windows update from the [CoreCE project releases](https://github.com/mastercheese-dev/CoreCE/releases). A debug build stops at the first failed check and writes it to the log.

Arch Linux users can install the [AUR package](https://aur.archlinux.org/packages/CoreCE-bin). The [Linux README](port/linux/README.md) covers controls, settings, and multiplayer. Most of that is the same on every platform. On Mac, settings live in `~/Library/Application Support/CoreCE/config.toml`.

## Multiplayer

- **Up to 128 players** on up to 128 machines, and up to 4 players on one device in split screen.
- **Windows, Linux, and Mac play together.** On a local network they can play LAN.
- **Peer-to-peer internet hosting.** Join through the server browser or an invite link. A host lists a game as public, or keeps it private for players with the invite. With Discord, the host can send the invite through Discord.
- **Online co-op campaign,** with friendly fire, player collisions, and more enemies as the player count grows.
- **Voice chat:** push to talk (V) or an open microphone, changed in the audio settings.
- **Map sharing:** a player missing the game's Custom Edition map downloads it from the other players.
- **Moderation:** the host can kick and ban from the scoreboard, or with the `kick` and `ban` console commands. Players can vote to kick.
- **Scrollable scoreboard:** the mouse wheel or Page Up and Page Down moves through a large lobby.
- **Netcode:** each machine moves its own player at once, and the host decides the game. See [port/linux/NETCODE.md](port/linux/NETCODE.md).

## Custom Edition maps

Put Halo Custom Edition maps in `custom_maps`, not in `maps`. That folder also needs `bitmaps.map`, `sounds.map`, and `loc.map` from your installed copy of Halo Custom Edition. On Mac, put `custom_maps` beside the Xbox `maps` folder the game is using, including a managed copy under Application Support.

The Gearbox maps that shipped with Halo PC (Ice Fields, Death Island, Danger Canyon, Infinity, Timberland, and Gephyrophobia) are separate. Copy them from a Halo Custom Edition install's `maps` folder into `custom_maps`. They then appear under Custom Singleplayer and Custom Multiplayer. A custom singleplayer map plays as a campaign level, alone or as online co-op. Maps that require OpenSauce will not load.

The server browser and the LAN browser can download a missing custom map from the other players, including `bitmaps.map`, `sounds.map`, and `loc.map`. This works on a LAN, and on the internet when the host has map sharing on (the default). See [docs/custom_edition_caches.md](docs/custom_edition_caches.md).

## Mods

Mods are folders of Halo PC tag files, as the Halo Editing Kit and Invader write them, in `mods/` next to `maps`. Each has a `mod.toml`. Every mod in the folder starts on. The main menu's Mods turns them off and on and sets the load order, with the top mod winning. The game refuses mods that say they cannot be played together. Mods are drawn on your machine only. See [docs/mods.md](docs/mods.md).

## Community maps and competitive options

Mac Settings can download 40 approved community maps in the background after you opt in. The collection is about 863 MiB, built for Xbox v5 NTSC data. Verified downloads stay usable offline. Windows and Linux currently install matching maps by hand. See [community-map setup](docs/playtesting.md#community-maps).

**PB Options** offers a match timer, spawn markers, timer announcements, and optional silent movement and weapon-ready sounds. Those modifications default off. The host chooses them. Use **Stock** for original-rule play. Timer announcements need a separate audio pack, which these builds do not include. See [PB Options](docs/performance-options.md).

## Graphics

The game keeps the Xbox look unless a setting says otherwise.

**Display**

| Option | What it does | Default |
| --- | --- | --- |
| DISPLAY MODE, RESOLUTION, WINDOW SIZE | Fullscreen, borderless, or windowed. F11 switches mode. | Borderless, at the display's resolution |
| RESOLUTION SCALING | Draw at the window's resolution, or at the Xbox's 640x480 and scale it up | Native |
| V-SYNC, FRAME RATE LIMIT | Wait for the display, or run free up to a frame cap | V-sync on |
| SMOOTH MOTION | A frame for every refresh, blended between the game's 30 ticks a second | Off |
| INSTANT AIM | In first person, the view turns the frame the mouse moves | Off |
| FOV, VIEWMODEL FOV | Field of view of the view and of the weapon and hands, from 80 to 150 degrees | Stock |

**Image quality**

| Option | What it does | Default |
| --- | --- | --- |
| ANTI-ALIASING | FXAA, SMAA, MSAA, or SSAA | Off, as on the Xbox |
| SHADOW RESOLUTION | Object shadows at 128 (the Xbox's), 256, 512, or 1024 pixels | 128 |
| PER-PIXEL LIGHTING | Lights models for each pixel | Off |
| MODEL DETAIL | Stock uses the Xbox's 480 lines. Native uses this display. Highest keeps the most detail. | Stock |
| HIGH-RES HUD, BUTTON STYLE, HIGH-RES TEXT | Redrawn HUD art, button art, and text that stay sharp when scaled | On for the high-res HUD and text |
| CAMPAIGN HUD | On a custom map, draw the campaign HUD. The reticle stays the map's. | Off |
| MATCH TIMER | A clock at the bottom right in multiplayer | Off |

**Extras**

| Option | What it does | Default |
| --- | --- | --- |
| LEGS | See your own legs when you look down in first person | Off |
| ARM SHIELDS | The energy shield's flare on your first-person arms | Off |
| SHIELD GLOW, GLOW INTENSITY | Energy shields light what is around them as they flare | Off |
| SHIELD COLOR | Your own shield in a multiplayer armor color, as your machine draws it | Default |

## How to build

You do not need the Xbox SDK. The port supplies the declarations the game uses. See [port/include/xdk](port/include/xdk/README.md).

1. Install Python and [ninja](https://ninja-build.org/).
2. Install the tools for your platform. See that platform's README.
3. From the repository root, run `python configure.py`.
4. Build a target:

| Target | Result |
| --- | --- |
| `ninja linux` | `build/linux/halo` |
| `ninja windows` (on Windows, or on Linux after `configure.py --windows-cross`) | `build/windows/halo.exe` and `SDL3.dll` |
| Mac | [Apple setup](docs/apple-build.md). The Mac build uses LLVM 22 and `tools/macos_build.py`. |

`python configure.py --release` builds a release, which does not stop on assertions. `--portable` makes a Linux or Windows build that starts on other x86-64 machines. Without it, those builds use the instructions of the machine that compiled them.

The experimental [iPhone port](port/ios/README.md) is a developer build. This testing release has no iPhone package.

## Credits

The decompilation is of Xbox build 2342. CoreCE starts from [bnunu/halo-1](https://github.com/bnunu/halo-1), which is a fork of [punpckhdq/halo](https://github.com/punpckhdq/halo). High-res redraw artists are in [AUTHORS.md](AUTHORS.md). This tree reviews upstream changes one by one.

## Licence

This fork is released under [CC0 1.0](LICENSE.md). Third-party code, fonts, and art keep the licences next to them.

## Disclaimer

CoreCE (Open Community Edition) is an unofficial fan project. It is not endorsed by or affiliated with Microsoft, Xbox Game Studios, Halo Studios, Activision, or Bungie. Halo and Halo: Combat Evolved are trademarks of Microsoft Corporation. Halo is copyright Microsoft Corporation.

No game data is included. You must provide your own copy of the game.
