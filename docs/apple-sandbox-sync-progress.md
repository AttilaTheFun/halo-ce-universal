# Apple sandbox and shared-save migration

Work in progress, 2026-10-17. No new distribution has been published.

## Required outcome

- Native Halo.app, iOS and iPadOS; personal distribution identifier
  `com.LoganShire.Halo`, with the public contribution configurable for other teams.
- Sandbox-compliant ISO picker and validated private copy in the app container;
  no retail data in the signed application. Preserve existing saves during migration.
- Resume campaign checkpoints between Mac, iPad and iPhone using Apple cloud
  storage, including account-unavailable, offline and concurrent-save handling.
- Verify signing capabilities, then publish all platform builds through Installer.
  Do not assume the available account is an Enterprise Program account.
- Refresh both upstream levels and keep a single public contribution PR, with
  personal branding/signing on the internal branch. Do not create worktrees.

## State and recovery

Original branches preserved at `backup/ios-installer-before-cloud-20261017`
(ff18344c) and `backup/ios-internal-before-cloud-20261017` (a48ace12).
Temporary local branch `apple/sandbox-sync` builds on innerhat c45870c2.
This is not an extra PR; it stages the replacement contribution until verified.
A literal replay of all 36 original commits conflicts because innerhat's
5dab1b3e has incorporated much of the iOS host. That attempt was aborted.
Reapply only remaining differences and verify feature coverage before replacing
apple/ios-installer and replaying internal changes.

Dynamic iOS arena patch a6f0b6f2 has been reapplied with conflicts resolved,
retaining upstream's 0.3.0 version and adding the no-EVA metadata. Python build
scripts compile, but native compilation and runtime testing are still pending.

Fetched upstreams:
- innerhat/main c45870c2: 65 commits beyond previous 036b9fe6; includes native Mac
  Metal, its own iOS host, runtime/menu fixes and upstream network changes.
- OpenCommunityEdition/OpenCE main 7524ba61: 37 commits beyond last reviewed
  8102e711. Per-commit diff review and integration ledger remain required.

## Remaining engineering and verification

1. Audit old final contribution against current innerhat. Preserve memory fixes,
   landscape, import, controls, menu/browser, diagnostics and license coverage;
   retain upstream improvements rather than reverting files wholesale.
2. Review both upstream diffs and record decisions according to fidelity policy.
3. Implement shared container paths and sandbox entitlements. The installed Mac
   currently uses Application Support/Halo OG, not a sandbox container. Mac app
   deletion alone must not be advertised as guaranteed container cleanup.
4. Identify actual committed checkpoints/profile files and a coherent save
   boundary. Never cloud-sync ISO/maps, transient map cache or diagnostics.
5. Implement cloud snapshots, atomic local restore, conflict handling, identity
   changes, offline retry and visible sync state before entering campaign.
6. Provision a shared cloud container with the actual available account; prove
   Mac/iPad/iPhone save interoperability, not merely successful API compilation.
7. Sign/audit sandboxed artifacts, increment versions, publish through Installer.
   Physical-device launch/testing needs Logan's specific permission first.

The recovered old saves are in ~/Library/Application Support/Halo OG. The
network-test snapshot is separate under Recovered Saves. Original ISO remains
in ~/Developer/game_images. No generated artifacts or credentials belong here.

## Private Mac import progress

Mac disc import now makes a SHA-256/readback-verified `disc.iso` copy beside its
extracted maps, records integrity metadata, and stores the private image path in
preferences. Failed import removes only the unpublished generation. Security
scope stays open through the source copy. The private generation is excluded
from backup; it must never enter the cloud save payload.

Default Mac storage now comes from Foundation's user Application Support URL,
with a Halo subdirectory, rather than a HOME-based path. This follows the
container when sandbox entitlements are applied; it does not by itself enable
sandboxing. Legacy in-scope OpenCE/Halo OG/Halo CE Universal data is preserved by
the existing migration helper. Migration from outside the sandbox and across
bundle identifiers still requires an explicit authorized picker flow.

Validation: native Foundation preferences/import tests pass, including retained
ISO byte equality and integrity metadata, source-image removal, invalid-image
rollback, existing settings, and legacy migration tests. No GUI/device launch.

Signing evidence from Visor: no Enterprise/In-House credentials on Mini. iOS
team LOGAN SHIRE, LLC and Developer ID team Aphelion, LLC differ. No Halo App ID
or iCloud container is configured. Asked Logan which team to use and whether to
configure its App ID/container/profiles; no account changes before his answer.

## Save snapshot foundation

`port/apple/save_snapshot.m` now serializes persistent `u/UDATA` and `z/saved`
files with per-file hashes, a schema version, snapshot identity and explicit
compatibility ID. Map caches, ISO, logs and the working `z/savegame.bin` are
excluded. Source inspection shows the persistent campaign checkpoint is written
with header/checksum last and then closed in game_state_write_to_persistent_storage.
The coordinator must take snapshots only at a quiescent save boundary, not by
polling an actively written directory. That integration remains outstanding.

Decoding validates paths, hashes, format, compatibility, case-equivalent names,
file/directory collisions and bounds before staging into a new private directory.
Staging never overwrites live saves. Journaled adoption/recovery, CloudKit
transport, offline/conflict handling and UI are still required. These APIs are
not yet wired into the app; there is no claim of functioning cloud sync.

`python3 tools/test_apple_save_snapshot.py` compiles production code with ASan
and UBSan and exercises round-trip restore, ignored cache/ISO files, wrong
compatibility, corrupt content, malformed entries, traversal, collisions and
symlink rejection. Passed locally. This does not prove cross-device runtime
checkpoint compatibility, which requires actual Mac/iOS/iPadOS verification.

Cache Cleanup status: no Bazel output bases or linked worktrees for this repo.
Both former Apple worktree directories remain removed.

## Transactional save restore

`save_restore.m` stages a validated snapshot, records which persistent save
folders existed, then moves original directories into the private transaction
before replacing them. Files and directory changes are synced before commit.
Startup recovery rolls back an uncommitted transaction or finishes committed
cleanup. A restore error retains the journal/backups for retry; a game must not
start if recovery fails. The game must be stopped for restoration.

Both Mac and iOS startup paths now call the recovery bridge before guest launch;
build inputs include the shared modules. Actual cloud-triggered restoration is
not wired yet. Full app builds and sandboxed runtime verification remain pending.

Validation: ASan/UBSan tests terminate a child process at each of six transaction
checkpoints, restart recovery twice, and check that both profile and checkpoint
belong to the same old/new generation while cache bytes remain untouched. All
passed. These are process-death tests, not power-loss hardware tests. Production
builds do not include the fault-injection environment variable.

## Sandbox packaging in progress

The Mac packager now defaults to sandbox entitlements and Halo.app, accepts
`--bundle-id` and an optional embedded provisioning profile, and supports
`--installer-managed` without a Sparkle feed. Installer-managed builds disable
the fork updater. Optional bundled command-line content tools inherit the app
sandbox. ISO selection remains a system picker, and sandbox first launch requires
a private image import instead of retaining an external maps-folder path.

A clean native Metal build exposed an upstream compiler-wrapper path mismatch:
the plugin was generated in build/macos-metal but loaded from build/macos. The
wrapper now selects the same renderer-specific directory as the build graph.
Menu/installer unit tests pass (14). A clean native app build is in progress;
its signature, sandbox runtime and device/cloud behavior are not yet verified.
Signing-account setup remains pending Logan's team choice.

Clean native Metal build 30 completed. The bundle is Halo.app with identifier
com.LoganShire.Halo, HaloSandboxed=true and HaloInstallerManaged=true. Its actual
code signature contains app-sandbox, user-selected file access, network client/
server, USB/Bluetooth and the existing compiled-guest executable-memory exception.
Strict nested signature verification passes; bundle inspection finds no ISO,
XISO, map files or GameDataPath.txt. This is ad-hoc development signing, not
team signing or notarization. Per the local install workflow it was installed
at /Applications/Halo.app, retaining the previous copy in the installer backup;
the installed signature and BuildInfo were checked. It was NOT launched.
Runtime sandbox/import/controller/gameplay acceptance is still pending, as are
cloud functionality and Installer publication. Do not treat build 30 as the
completed cross-device release.

## Cloud transport and checkpoint capture

`cloud_save.m` implements bounded CKAsset download/validation and upload using
CKRecordSaveIfServerRecordUnchanged. It accepts the caller's last observed head;
conflicts are returned without an overwrite/retry. Temporary upload assets stay
alive until completion and are removed afterwards. Tests inject a database and
make NO Apple account requests. They cover missing/offline responses, file
lifetime, successful validation, conflict without retry, metadata mismatch and
compatibility rejection. ASan/UBSan passed. This transport still needs the
account-bound coordinator and production container setup before app use.
Apple policy reference:
https://developer.apple.com/documentation/cloudkit/ckmodifyrecordsoperation/recordsavepolicy/ifserverrecordunchanged

The Apple guest's successful persistent checkpoint write now invokes a host
callback after closing its save handle. Failed open/payload/header writes never
capture a snapshot. The host prepares a durable local candidate at
`Cloud Saves/local-checkpoint.plist`; it is NOT a cloud acknowledgement. The
compatibility key includes a versioned native save format and the private ISO's
SHA-256. Legacy imports without an integrity manifest do not enqueue snapshots.
This callback and configuration are wired into both Apple build/launch paths.
Other platform/Xbox builds retain their existing behavior behind HALO_MACOS.

Tests: `tools/test_apple_checkpoint_boundary.py` runs the production persistent
write function with injected file failures; `tools/test_apple_save_snapshot.py`
now also checks real host capture and rejection of missing game identity. Both
passed. Full Mac rebuild 31 is running, unified exec session 55378; log is
/tmp/halo-checkpoint-build.log. Revalidate that handle before waiting/restarting.
No build 31 installation/publication yet. Physical devices untouched.

## Account-aware reconciliation

`HaloCloudSync` now persists an account/compatibility-bound common snapshot,
compares local candidates with the remote head, uploads only when the remote
has not diverged, and presents conflict states when both sides changed. Remote
restoration waits for guestStopped; conflicting local data is retained under
Cloud Saves/Conflicts before adopting remote data. A newly captured checkpoint
during upload remains pending instead of being acknowledged accidentally.
Account changes require explicit consent; invalidation discards late callbacks.
Invalid metadata/oversized candidates fail closed. Authentication and UI wiring
are still outstanding; the coordinator is not yet running in the app.

Tests run the coordinator against the production transport with an injected
server: matching base, local upload, cloud arrival during play, stopped-guest
restore, competing local/remote edits, rejected conditional write, preserved
conflicting snapshot, different-account refusal and late-callback invalidation.
ASan/UBSan passed. There have been no real CloudKit requests or developer-account
changes, and no claim of real multi-device sync.

Build 31 finished successfully (session 55378 is terminal). Installed it via the
local workflow with strict signature and BuildInfo verification. Source recorded
62aaeb96. No launch, device interaction or Installer publication. The build
contains checkpoint capture, not the later coordinator. Team signing and actual
sandbox/runtime/cloud acceptance remain pending.

## Runtime/cloud UI integration

`cloud_runtime.m` now connects the coordinator to Apple startup, CKContainer
account checks and the private database. It captures existing offline progress
before a first cloud comparison, bounds startup network waiting, retries while
running, and invalidates old callbacks/actions when the account changes. Cloud
restores are followed by transaction recovery before guest entry. A failed local
capture does not masquerade as an empty device eligible for cloud overwrite.
Mac menu and iOS shake diagnostics expose iCloud Saves status/conflict controls.
These flows require real account and UI testing; syntax checks passed against
both macOS and iPhoneOS SDKs, not a claim of rendered UI acceptance.

Cloud networking is explicitly opt-in via HaloCloudContainer. Build tools accept
--cloud-container and --entitlements; Mac also requires an embedded profile.
The requested container must appear in the supplied CloudKit entitlements, and
sandboxed Mac builds retain App Sandbox. Positive/negative configuration checks
passed. No account configuration was changed or external cloud request made.

Mac host build 32 compiled and passed strict signature checks, then installed
locally per workflow. Its cloud container is absent, so it reports unavailable
rather than attempting unauthorized cloud access. It was not launched/published.

The first iOS build found the new checkpoint hook missing from the separate iOS
import list; fixed. Session 83382 failed and is terminal. Replacement combined
simulator/device build is running as unified exec session 35836; log:
/tmp/halo-ios-sandbox-build.log. The simulator guest linked and Xcode host
configuration began. Poll that handle before retrying; do not duplicate builds.
The running invocation predates the new --build-number flag and uses its default
12; repackage with a current build number before any publication.

## iOS contribution comparison, 17 October

Inspected the actual old-public-versus-innerhat diffs for iOS host controls,
import/startup, CMake packaging, plist/entitlements, build/compiler/resigning
scripts, and shared graphics/debug defaults. Restored the omitted stb_vorbis
license to packaged resources and renamed remaining iOS user-facing OpenCE
strings to Halo. The missing-map message now directs users to the ISO importer.
Controls otherwise differ only in accessibility-label punctuation; keep upstream.
Keep upstream's explicit SDK selection in the compiler-plugin build and its
ANGLE token fallback. Dynamic-arena entitlement/resigning changes were already
reapplied earlier in this branch. Keep upstream's interpolation default for now;
the smooth-motion option remains available and performance comparison is pending.
This is a scoped contribution audit, not completion of the upstream commit ledger.

Build session 35836 remains live; the device guest compiled and host CMake
configuration is progressing. Source changes during this build mean final host
artifacts must be rebuilt from the committed revision before distribution.
No simulator was running when Visor reported its shutdown-all incident.
The native macOS VM recipe is now available locally and was read; it requires
one-time user Setup Assistant/permissions before automated UI testing. No VM
has been downloaded, created or booted.

## Simulator first-launch smoke check

Installed the completed simulator product from session 35836 onto the existing
OpenCE migration test simulator (only this simulator was booted). Launch of
com.LoganShire.Halo succeeded and reached the first-run ISO importer, with the
white-on-blue Choose Halo ISO button and the landscape interface fully visible.
The screenshot is at build/apple-tests/sandbox-first-launch.png (ignored).
The compiled simulator artifact predates f08941b4, so its screen still says
OpenCE; a final host rebuild is required. iPad plist lists landscape-left/right
only. This proves startup/import-screen presentation, not file-picker import,
gameplay, physical-device behavior or cloud operation. Terminated the app and
shut down only the owned simulator afterward. Device build session 35836 is
still configuring its host dependencies; do not duplicate it.

## Symmetric conflict preservation

Keep This Device now persists the observed cloud snapshot under Cloud Saves/
Conflicts before submitting a conditional overwrite. Previously only Use iCloud
preserved its losing side. Backup failure stops the upload; the existing server
change-tag condition still rejects another device's intervening save. The UI now
explains that both choices keep backups. Production transport/coordinator tests
passed with ASan/UBSan, including a rejected-server update retaining the cloud
backup and an unwritable backup destination preventing any upload. This is
injected-database evidence, not a real iCloud account test.

Combined simulator/device build 35836 is now terminal with exit 0 and both
Xcode builds succeeded. The physical-device artifact is unsigned; no device
installation or publication occurred. Started incremental host rebuild for both
platforms from 5ada1f66 with build number 33 (session 37275, log
/tmp/halo-ios-host33-build.log) to include all changes made during the long build.
Poll this handle before another build.

## Build 33 and upstream review

Host rebuild 37275 completed with exit 0 for simulator and device. Both generated
plists identify Halo / com.LoganShire.Halo / build 33, explicitly do not require
Extended Virtual Addressing, and have no cloud container configured. Both include
stb_vorbis license text; file-extension audit found no ISO/XISO/map files. Device
artifact remains unsigned. This audit is not signing or runtime acceptance.

Started docs/upstream-review-2026-10-17.md with four actual-diff reviews and
seven passing assembly-converter regressions. Reversed the Apple fork's new
radius-based weapon-flare suppression/brightness heuristic pending Xbox visual
evidence, following the inherited fidelity policy. Build 33 predates this reversal;
new guest builds are required before release. The complete upstream ledger and
public/internal branch update remain outstanding.

Upstream review continued with six complete networking/rendering/menu diffs
recorded in the 17 October ledger. The map-sharing hardening review is partial.
Attempted 64 network harness cases; all stopped at missing 32-bit host-runtime
link support, before execution. No harness is running. These tests still require
a suitable supported execution environment; no VM was created for this attempt.
