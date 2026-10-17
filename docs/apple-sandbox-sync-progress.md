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
