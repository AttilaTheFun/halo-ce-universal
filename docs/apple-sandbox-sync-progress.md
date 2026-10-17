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
