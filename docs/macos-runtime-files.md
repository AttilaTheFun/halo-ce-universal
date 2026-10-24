# Files a Mac game uses

A running Mac app uses three places. The app bundle is what the disk image installs. Settings and Xbox save data live in Application Support. The game data folder is the one you chose, and it holds the maps plus the logs the game writes beside them.

Xbox paths are turned into those folders. `d:\` is the game data folder. Any other drive letter `X:\` is the folder `X` inside Application Support. The hard-drive saves are `z:\`, and the memory-unit saves are `u:\`. A Custom Edition install named by `paths.custom_edition` in `config.toml` is `h:\`. That path is absent unless you set it.

`HALO_SAVE_ROOT` replaces Application Support for that launch, and the first-launch copy described below does not run. `HALO_DATA_ROOT` replaces the chosen game data folder for that launch. Starting the host with a guest image path, and without `HALO_SAVE_ROOT`, uses `build/macos/saves/` instead of Application Support. The installed app does not do that.

## The installed app

The disk image installs `/Applications/CoreCE.app`. The game does not write inside this bundle during play. An update replaces the whole bundle.

```text
CoreCE.app/Contents/Info.plist
CoreCE.app/Contents/MacOS/halo
CoreCE.app/Contents/_CodeSignature/CodeResources
CoreCE.app/Contents/Frameworks/libSDL3.0.dylib
CoreCE.app/Contents/Frameworks/Sparkle.framework/
CoreCE.app/Contents/Resources/AppIcon.icns
CoreCE.app/Contents/Resources/BuildInfo.txt
CoreCE.app/Contents/Resources/Helmet.pdf
CoreCE.app/Contents/Resources/brokers.txt
CoreCE.app/Contents/Resources/halo_guest.elf
CoreCE.app/Contents/Resources/map-downloads.json
CoreCE.app/Contents/Resources/Licenses/ANGLE.txt
CoreCE.app/Contents/Resources/Licenses/KCP.txt
CoreCE.app/Contents/Resources/Licenses/Project.txt
CoreCE.app/Contents/Resources/Licenses/SDL3.txt
CoreCE.app/Contents/Resources/Licenses/Sparkle.txt
CoreCE.app/Contents/Resources/Licenses/mbedtls.txt
CoreCE.app/Contents/Resources/Licenses/miniupnpc.txt
CoreCE.app/Contents/Resources/Licenses/musl.txt
CoreCE.app/Contents/Resources/Licenses/tomlc17.txt
```

`Info.plist` names the app CoreCE. The bundle identifier is `local.halo.ce-universal`. The short version is the release version. `CFBundleVersion` is the bundle build. `CoreCEMacUpdateBuild` is the Mac release number the updater compares. Get Info shows the short version and the bundle build, and does not show the Mac release number.

`MacOS/halo` is the host. `halo_guest.elf` is the game image it loads. `BuildInfo.txt` records the version, source revision, guest hash, and Mac release number. `brokers.txt` is the list of internet-play brokers, copied into Application Support on every launch. `map-downloads.json` is the community-map catalog configuration. `Helmet.pdf` is the menu icon artwork. `Licenses/` holds the license texts bundled with the app.

`libSDL3.0.dylib` is the window and input library. `Sparkle.framework` is the bundled updater framework. Inside it, `Versions/B` holds `Updater.app`, `XPCServices/Downloader.xpc`, `XPCServices/Installer.xpc`, headers, modules, a code signature, and `Resources` with one `.lproj` folder per language. That is 85 files, all Sparkle's. The game does not add or change them. This fork's Mac updates download a release from the fork and swap the app. They do not write a Sparkle appcast into the bundle.

While an update is installing, these exist only for that swap and are removed when it finishes:

| Path | Role |
| --- | --- |
| `$TMPDIR/<id>.dmg` | The downloaded disk image |
| `$TMPDIR/<id>/CoreCE.app` | The app copied out of that image |
| `$TMPDIR/<id>.sh` | The installer script, deleted after it runs |
| `/Applications/.CoreCE-update/` | Holding folder next to the installed app |
| `/Applications/CoreCE.app.previous` | The previous app, restored if the new one cannot take its place |

## Application Support

```text
~/Library/Application Support/CoreCE/
```

The folder is created on first launch. If `~/Library/Application Support/OpenCE/` exists, that tree is copied here. Otherwise `Halo OG` is copied, or `Halo CE Universal` when that is the only older folder. The original folder is left in place. Files already in CoreCE are kept. `HALO_SAVE_ROOT` skips this copy.

`macos-settings.json` is copied with a special case: `data_path` and `iso_path` are rewritten only when they point inside the older folder. `legacy-migration.json` and any `legacy-migration-failed-*` file in the older folder are not copied.

| Path | Written by | What it is |
| --- | --- | --- |
| `macos-settings.json` | The Mac host | The chosen game data folder (`data_path`), an optional disc image (`iso_path`), whether the window opens windowed (`windowed`), and whether community downloads are on (`community_downloads`) |
| `config.toml` | The game, on first launch and when a setting is saved | Settings, key bindings, and comments. Quit before editing it. A later launch keeps comments and adds missing settings |
| `halo.log` | The Mac host, replaced each launch | The host log for that launch: build, data folder, save folder, display, and engine messages |
| `brokers.txt` | The Mac host, replaced each launch | A copy of the bundle's `brokers.txt`. Internet play reads this file beside `config.toml` |
| `legacy-migration.json` | The first launch that copies an older folder | A record that the copy finished, including the paths. A later launch does not copy again |
| `legacy-migration-failed-<id>.json` | A copy that failed | Why the copy stopped. The game quits instead of starting on empty settings. The next launch tries again |
| `.migration-<id>/` | During the copy only | Staging for that copy, removed when the copy finishes |
| `update.log` | Older Mac builds | The previous updater's last script output. Current builds do not write it. Update history is `update.txt` in the game data folder |
| `Cache/MetalWarmup-v1.bin` | The Metal renderer, when the game exits | Pipelines learned while playing, read on the next launch so they can be compiled sooner. A bad or stale file is ignored |
| `Community Maps/maps/` | Launch, when the game data is an original Xbox map set the downloader accepts | Downloaded community maps, one `<id>.map` each. The folder is created even when community downloads are off |
| `Community Maps/Downloads/` | The same launch | Partial files named `<sha256>-<uuid>.partial`, removed when the download finishes or fails |
| `Community Maps/catalog.json` | A successful catalog fetch, which runs only while community downloads are on | The last catalog the app stored, with the catalog URL it came from |
| `Community Maps/local-maps.json` | A community package import | Receipts for maps imported from a package |
| `Game Data/<id>/` | Choosing a disc image or a maps folder that the app copies in | A new folder per import. `import.json` describes it. A disc import writes `maps/` (through a temporary `maps.partial` that is renamed when the copy finishes) and does not copy movies. A folder import copies `.map` files from `maps` and, when present, `maps_de`, `maps_fr`, `maps_es`, and `maps_it`. The folder in `macos-settings.json` may instead be one you already had, outside Application Support |
| Files inside `Game Data/<id>/` | The game, when that import is the folder it is using | The same files the game writes into any game data folder, including `debug.txt` and `gamestate.txt` |

### Save volume (`z`)

`z/` is `z:\`. The game creates this layout on first use. `z/maps/` is the exception: it is not created here, and shows up when an older support folder that already had it is copied.

```text
z/saved/player_profiles/default_profile/00.sav
z/saved/player_profiles/default_profile/01.sav
z/saved/playlists/default_playlist/<nn>/blam.lst
z/saved/recordings/last_recording/
z/saved/hdmu.map
z/lastprof.txt
z/lastmpvr.txt
z/lastmpmp.txt
z/last_solo.txt
z/last_language.dat
z/savegame.bin
z/cache000.map
z/maps/
```

`00.sav` and `01.sav` are the two built-in controller profiles (standard and inverted). A profile or game variant you create is a save container under `u/`, described below. `lastprof.txt` remembers the last profile directory.

`default_playlist/<nn>/blam.lst` is one built-in multiplayer game variant. The first run creates `00` through `25`. `lastmpvr.txt` remembers the last variant directory. `lastmpmp.txt` remembers the last multiplayer map.

`last_recording/` is created when saves are set up. The game does not write a film file into it. `last_solo.txt` is the last campaign map name. `last_language.dat` stores the language last used to choose map files. When that language changes, the cached map files below are deleted and this file is rewritten.

`savegame.bin` here is the game-state file the engine opens on the hard-drive volume. A profile's own checkpoint is the `savegame.bin` inside that profile's folder under `u/`.

`hdmu.map` is the index of the saves under `u/`. The engine uses six cached map files, `cache000.map` through `cache005.map`. On startup it also deletes `cache007.map` through `cache019.map` if they are present. `z/maps/` can be present because an older support folder was copied. This build does not put the retail maps there. They stay in the game data folder.

### Memory unit (`u`)

`u/` is `u:\`. Each profile or saved game variant is a directory:

```text
u/UDATA/<12 hex digits>/SaveMeta.xbx
u/UDATA/<12 hex digits>/blam.sav
u/UDATA/<12 hex digits>/savegame.bin
u/UDATA/<12 hex digits>/blam.lst
```

The 12 hex digits are a hash of the save's display name. `SaveMeta.xbx` stores that name. A player profile is `blam.sav` plus `savegame.bin`, the checkpoint kept with that profile. A game variant you save is `blam.lst` and has no `savegame.bin`. More containers appear as you create profiles and variants.

## The game data folder

This is the folder in `macos-settings.json` `data_path`, or the folder you pick on first launch. It is `d:\`. You supply the disc files. The game adds the files in the second table.

Supplied by the disc or by an extract of it:

```text
maps/ui.map
maps/a10.map maps/a30.map maps/a50.map
maps/b30.map maps/b40.map
maps/c10.map maps/c20.map maps/c40.map
maps/d20.map maps/d40.map
maps/beavercreek.map maps/bloodgulch.map maps/boardingaction.map
maps/carousel.map maps/chillout.map maps/damnation.map
maps/hangemhigh.map maps/longest.map maps/prisoner.map
maps/putput.map maps/ratrace.map maps/sidewinder.map maps/wizard.map
bink/intro.bik bink/credits.bik
bink/attract1.bik bink/attract2.bik bink/attract3.bik
xdemos/XDemos.xbe
xdemos/xdemos.xdi
xdemos/media/ambient.wma xdemos/media/launch.wav xdemos/media/menu.wav
xdemos/media/fontb.xpr xdemos/media/fontn.xpr xdemos/media/xdemos.skn
xdemos/media/Dn.png xdemos/media/Up.png xdemos/media/screen.png
xdemos/media/screenT.png xdemos/media/wait.png
xdemos/media/nocontrollermessage.png
xdemos/videos/H2_E3.bik xdemos/videos/H2_E3.png
xdemos/videos/H2_Making.bik xdemos/videos/H2_Making.png
xdemos/videos/H2_X02.bik xdemos/videos/H2_X02.png
default.xbe
```

`maps/` is required for English. The names above are the original Xbox maps. `ui.map` is the menu. The `a` through `d` maps are the campaign. The rest are multiplayer. German, French, Spanish, and Italian look in `maps_de/`, `maps_fr/`, `maps_es/`, and `maps_it/` when that folder exists, and otherwise use `maps/`. `bink/` holds the intro, credits, and attract movies. `xdemos/` is the disc's extra media. `default.xbe` and `xdemos/XDemos.xbe` are Xbox executables and are not run.

Custom Edition maps and their resource maps live in `custom_maps/`, which you create by adding maps or by downloading one:

```text
custom_maps/<name>.map
custom_maps/bitmaps.map
custom_maps/sounds.map
custom_maps/loc.map
```

A map keeps its own name. A second version of that name is `<name>_<hash>.map`, and the menus show it with `(2)`, `(3)`, and so on. An optional picture `<name>.bmp` and description `<name>.txt` can sit beside it. Without the picture, the menus use the unknown-level picture. A download in progress is `<name>.map.part`, or `<name>_<hash>.map.part` when that is the file being written. Resource maps use the same part suffix. A download that cannot be played is deleted, including its part file.

Written by the game into this folder:

| File | When |
| --- | --- |
| `debug.txt` | Appended through the session. Each line starts with `mm.dd.yy HH:MM:SS` and two spaces. This is the game log |
| `update.txt` | Appended when a Mac build is recorded or an update is installed. The same date format as `debug.txt`. A success line is `mac N replaced by mac M.` A failed swap is `mac N stayed in place. mac M did not install.` The first launch of a build can also append `mac N installed.` |
| `init.txt` | Read at startup when you put console commands here. The game does not need the file. A slow-frame dump can also write `<map>_init.txt` |
| `core/<map>_slow_<month>_<day>_<year>_<hour>_<minute>_<second>.bin` | Written with that slow-frame dump. `core/` is created for it. `<map>_init.txt` then records the core name |
| `gamestate.txt` | Replaced at startup with the game's memory allocation record, then appended as allocations are logged |
| `stabbed.txt` | Replaced when the sound cache or texture cache runs out of room |
| `bans.txt` | Appended when the host bans a machine. Each line is tab-separated: time, address, hardware id, Discord name and id, player names, and reason |
| `cheaters.txt` | Appended when the host records a machine it dropped for cheating, in the same line shape. That drop is also written to `bans.txt` |

`debug.screenshot_directory` in `config.toml`, when set, is a separate folder. The Metal renderer writes `frame<number>.bmp` there when `debug.screenshot_every` is greater than zero. The screenshot key does not write a file on this Mac build.

Finder may add `.DS_Store` in any of these folders. The game does not read it.
