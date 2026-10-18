# Build on Apple Silicon

## Sandbox/iCloud branch status

The current `apple/sandbox-sync` development branch builds **Halo.app**, with
`com.LoganShire.Halo` supplied explicitly for internal builds. It uses native
Metal on Mac and supports native Metal on iPhone/iPad. The older workflow below
is inherited upstream documentation and has not yet been reconciled with this
branch; its OpenCE names, ANGLE-only description and unsandboxed save paths do
not describe these internal builds.

Build the current local products without account provisioning:

```sh
export HALO_MACOS_LLVM_BIN="$(brew --prefix llvm@22)/bin"
python3 tools/macos_build.py --renderer metal --no-data-path \
  --bundle-id com.LoganShire.Halo --installer-managed \
  --build-number NEXT_BUILD --jobs 3 --install
python3 tools/ios_build.py --platform both --bundle-id com.LoganShire.Halo \
  --build-number NEXT_BUILD --jobs 3
```

Replace `NEXT_BUILD` with a new integer. Run the builds sequentially: both
configure the root Ninja file. Mac packaging enables App Sandbox by default;
these commands produce an ad-hoc Mac app and unsigned iOS device app, not
Installer-ready releases. The simulator product is signed for local testing.

First launch asks for an original Xbox ISO/XISO using the system picker. The
importer copies and hashes the image, verifies the retained copy and extracts
validated maps before publishing the import. It does not establish ownership
or certify an image against a retail hash database. Neither image nor maps are
bundled in the app.

On iOS/iPadOS, new imports are `Documents/game-<UUID>/disc.iso` and saves are
`Documents/save/` in the app container. On sandboxed macOS, Foundation resolves
Application Support inside the app container; the app uses its `Halo/`
subdirectory, with retained images under `Game Data/<UUID>/disc.iso`.
iOS app deletion removes its container; simply deleting a macOS app can leave
its container behind. Offloading an iOS app also preserves its data.

Save synchronization code uses CloudKit's private database, with ISO-hash-based
compatibility and explicit conflict choices. It is **not enabled or verified
against a live account in the current builds**. A shared Apple developer team,
CloudKit container and matching provisioned entitlements are required on every
platform. No Enterprise/In-House signing identity has been found on this Mac.
Do not publish these local products as cloud-enabled releases.

The save snapshot includes persistent profiles/checkpoints and campaign/profile
selection, excluding the image, extracted maps and transient caches. Importing
old saves uses an explicit folder picker from the iCloud Saves panel and applies
the queued snapshot on next launch, preserving a backup. Cross-device campaign
continuation still needs live acceptance testing; backend tests are insufficient.

See [current verification record](apple-sandbox-sync-progress.md) and
[upstream review](upstream-review-2026-10-17.md) for remaining release gates.

Start here for both macOS and iPhone. All commands run on an Apple Silicon Mac
from the repository root unless stated otherwise. The Mac game runs natively;
the iPhone build also uses native ARM code and ANGLE's Metal renderer.

To run CoreCE on Mac without compiling, download the
[macOS DMG](../port/macos/README.md#download). Published testing prereleases have
direct asset links; unpublished CI artifacts require GitHub sign-in and expire.
The DMG bundles the runtime
dependencies; only your own original Xbox game data is needed separately.

## 1. Clone and install public dependencies

```sh
git clone https://github.com/pfista/halo-og.git
cd halo-og
```

Install Xcode Command Line Tools for a Mac-only build, or full Xcode 26 or later
for iPhone. Open Xcode once to accept its license and install the iOS platform
support. For simulator testing, also install an iOS 26+ simulator runtime in
Xcode's Settings → Components. Physical-device builds do not need a simulator.

Using [Homebrew](https://brew.sh/):

```sh
brew install python cmake llvm@22 lld@22 sdl3 ninja
export HALO_MACOS_LLVM_BIN="$(brew --prefix llvm@22)/bin"
export PATH="$(brew --prefix lld@22)/bin:$PATH"

# For iPhone builds; adjust if Xcode is installed elsewhere:
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer

python3 tools/macos_setup.py
```

The compiler adapter requires **LLVM 22** (tested with 22.1.8), not Apple clang
or a newer LLVM major. [Homebrew's LLVM 22 formula](https://formulae.brew.sh/formula/llvm@22)
provides that version; LLD is a separate formula. Keep the exports in the shell
used for later builds. Do not put LLVM's entire `bin` directory ahead of the
system compiler: the host builds with Apple's clang, while
`HALO_MACOS_LLVM_BIN` selects the game compiler. Python 3.10+ and CMake 3.28+
are required.

Already have LLVM 22 at `/opt/homebrew/opt/llvm`? Use that keg for
`HALO_MACOS_LLVM_BIN` instead. Setup accepts an ELF-capable `ld.lld` on `PATH`;
if unavailable, installing `uv` enables its local Zig/LLD fallback. Ninja can
come from Homebrew or that fallback environment.

Setup downloads only public dependencies. ANGLE and Khronos headers have pinned
URLs and SHA-256 checksums in [dependencies.json](../port/macos/dependencies.json).
The build also fetches SDL3 3.4.16 and musl 1.2.5. No Android NDK is required.

## 2. Supply game data to play

Mac and iPhone native builds use the declarations in
[port/include/xdk](../port/include/xdk/README.md). They do not require the
private Xbox SDK, `xbox/include`, or `cachebeta.exe`. The Mac app can be compiled
and packaged without game data using `python3 tools/macos_build.py --no-data-path`;
first launch then asks for the user's disc image or extracted maps folder.

To play, supply **original Xbox Halo: Combat Evolved maps** extracted from your
retail disc image. These are not included or downloaded by the build scripts.
Supported Xbox version-5 headers are PAL `01.01.14.2342` and USA
`01.10.12.2276`. Use one complete set; do not mix regions or builds. Halo PC,
Custom Edition, Anniversary and MCC maps are not substitutes.

The checkout should look like this:

```text
halo-og/
  assets/
    maps/
      ui.map
      a10.map
      a30.map
      ...                  # all remaining campaign and multiplayer maps
```

To extract an existing XISO, one option is the open-source
[XboxDev/extract-xiso utility](https://github.com/XboxDev/extract-xiso#building).
Build it under the ignored build directory, then copy only the maps into place:

```sh
git clone https://github.com/XboxDev/extract-xiso.git build/tools/extract-xiso
cmake -S build/tools/extract-xiso -B build/tools/extract-xiso/build
cmake --build build/tools/extract-xiso/build
build/tools/extract-xiso/build/extract-xiso \
  -x -d build/game-extracted "/path/to/your/Halo.xiso.iso"
mkdir -p assets
cp -R build/game-extracted/maps assets/
```

Use the actual paths and filename case from your extraction. If you already
have an extracted `maps/` folder, copy it directly; extraction is unnecessary.
Keep the disc image outside the repository. Supply the full set to play the full
campaign. The Mac data importer validates supported map headers when selecting
a disc image or game folder. The older SDK-oriented `macos_preflight.py` check
is not a prerequisite for these native builds.

## 3. Build and play

- [macOS: compile, launch, controls and performance](../port/macos/README.md#build).
  No Apple development account or special entitlement is needed.
- [iPhone: compile, sign and install](../port/ios/README.md#build).
  Use your own Apple development team, a unique bundle identifier and a
  phone in Developer Mode. Build 17 removes the Extended Virtual Addressing
  requirement; Personal Team installation remains unverified.
- [iOS simulator](../port/ios/README.md#simulator): useful for menu and touch
  checks without device signing; it does not establish device performance.

The tested devices are an M5 Mac on macOS 26.5.1 and an iPhone 17 Pro Max on
iOS 26.6.2. The current bundled SDL3 requires macOS 26.0, which packaging
enforces in the app's minimum version; the iPhone build targets iOS 26+.
Other OS/device combinations have not been exhaustively tested.

## Development workflow

[Apple regression checks](apple-regression-checks.md) covers source-only CI,
native menu responsiveness and local gameplay/renderer validation.

Use `main` in [pfista/halo-og](https://github.com/pfista/halo-og)
as the working branch. `origin` points to that fork; `upstream` points to
[cybersecurity/halo-ce-universal](https://github.com/cybersecurity/halo-ce-universal).
For a fresh clone, add the upstream remote:

```sh
git remote add upstream https://github.com/cybersecurity/halo-ce-universal.git
git fetch upstream
```

This is an independent fork with an [original-Xbox fidelity policy](xbox-fidelity.md).
Keep the last integrated baseline recorded while staying current with useful core
improvements through selective review. During upstream-update work, inspect every
new commit's actual diff and behavioral effects. Prioritize verified matching-
decompilation progress, performance, stability and platform fixes. Consider netcode
and potential matchmaking improvements individually, with timing and fairness
validation. Preserve original Xbox NTSC gameplay and presentation; high-refresh
rendering can coexist with the original 30 Hz simulation.

Do not merge upstream `main` wholesale. Select or cherry-pick accepted changes and
split mixed commits to exclude unrelated modifications. Renaming/rebranding,
replacement fonts/artwork/HUD, overhead player labels and changes to gameplay rules
are excluded by default unless the user requests them separately. Record each
commit's hash, category, decision, reason, validation and integration status in the
update's review ledger, including rejected, deferred and already-equivalent
changes. Record how far upstream was reviewed separately from how far it was
integrated. See the fidelity policy for the detailed review criteria.

When a contribution is ready, prepare a focused branch against
`upstream/main` and open a pull request into that repository. CI still checks
fork pushes; automatic releases and Discord build notifications run only in
the upstream repository. Our fork also has a separate, explicitly dispatched
[testing prerelease workflow](macos-menu-and-releases.md#manual-testing-prerelease)
that collects successful Mac, Windows, Linux and Android builds from the same
latest-main revision. Its protocol metadata comes from that revision; current
source uses protocol 11. Fork CI disables the existing upstream self-updater.

## Source and build artifacts

`assets/`, `xbox/`, `original/` and `build/` are ignored, along with disc images,
map files, SDK archives, Apple signing material and generated Xcode projects.
Generated game binaries stay under `build/` and must not be committed. The
[macOS DMG workflow](https://github.com/pfista/halo-og/actions/workflows/macos-dmg.yml)
builds a separate downloadable artifact from the committed source and public
dependencies, with no game data or private SDK inputs. It audits the bundle
before packaging and retains the DMG artifact for 14 days. This CI download is
separate from the Developer ID and Sparkle release process. Manual testing
publication can promote those audited CI outputs to a GitHub prerelease with
tagged direct download links, without personal signing credentials or game data.
The Mac download is `Halo-OG-macos-arm64.dmg` containing `CoreCE.app`.
The bundle identifier remains stable. The app copies prior data from
`~/Library/Application Support/OpenCE/` into the canonical
`~/Library/Application Support/CoreCE/` directory. If that folder is absent,
it copies `Halo OG`, or `Halo CE Universal` when that is the only older folder.
Original files and any files already in the new directory are preserved.

Device signing settings belong in your local Xcode account and ignored build
directory. Pass your team ID and bundle identifier on the command line; do not
hardcode your team, certificate name, device IDs or provisioning profile into
source files. The checked-in entitlement file is empty; no special memory
capability, identity or secret belongs there.

iPhone apps never bundle maps or disc images. Users import their own data after
installation. Keep signing credentials and profiles local; publish only the
unsigned public artifacts after checking their contents.

### iOS guest address bounds

Build 17 uses `vm_map` to reserve a 4-GiB-aligned, 2304-MiB virtual region in
the normal process address space. The compiler and import stubs recover its
base from the guest stack's upper 32 bits; host services use the selected base.
The executable image remains separately signed and its indirect-call delta
lives in the arena. The previous 448-GiB fixed address and Extended Virtual
Addressing requirement are removed. See [design, attribution and validation](apple-ios-dynamic-arena.md).

### iPhone/iPad graphics and diagnostics

Shake the device to open Graphics & Diagnostics, including with a hardware
controller. Metal (ANGLE) remains the default; native Apple OpenGL ES is an
alternative. Driver and render-resolution changes take effect on relaunch.
Choose 480p, 720p (default), 900p, 1080p or native, with a 30/60/120 FPS render
limit (default 60). Smooth motion restores the older Apple build's rendering
interpolation, without changing the original 30-Hz game simulation.

MetalFX spatial upscaling and the FPS overlay can be toggled live; upscaling
is available only with the active Metal renderer. Ordinary game-console chatter
is hidden; logs are retained. Share Play-session Report opens the system share
sheet (AirDrop or Save to Files) with bounded log excerpts, settings, the last
ten minutes of half-second timing summaries and shader diagnostics. It does
not include maps, the ISO or saves. First-use GPU-stall capture is opt-in because
its GPU synchronization changes performance. Normal timing capture avoids that
synchronization.


### GameSave opt-in

New iOS cloud builds use GameSave (iOS/iPadOS 26+), and configured Mac builds use
its macOS 26+ counterpart so both can share the same iCloud Documents container.
The existing `--cloud-container` and `--entitlements` build options now require
`CloudDocuments` and matching iCloud/ubiquity container identifiers. The selected
profile must authorize them; merely adding values to an entitlements file does
not grant access. App Sandbox remains required for the Mac cloud build.

Settings → GameSave / iCloud Saves → Enable GameSave Sync is a separate,
per-device opt-in, off by default. No GameSave directory or iCloud account lookup
is started while disabled. Older systems and unprovisioned/re-signed copies keep
local saves. GameSave files contain validated checkpoint snapshots, never retail
game data. Live save files remain private to the app and restores require the
guest to be stopped. An upload handed to GameSave is marked queued until a later
synchronization confirms it. Conflict resolution backs up all known checkpoint
versions first. Other cloud backend implementations are retained for existing
build compatibility, not used concurrently with GameSave.

Tests: `python3 tools/test_apple_game_save.py`,
`python3 tools/test_apple_cloud_runtime.py`, and
`python3 -m unittest tools.test_apple_cloud_config` use synthetic accounts,
profiles and directory states. Passing them is not a live iCloud test.
