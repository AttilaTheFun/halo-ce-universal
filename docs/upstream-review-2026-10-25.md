# Apple refresh, October 25, 2026

Rebased apple/sandbox-sync from innerhat/main 41a4efee onto c88fcf2d.
The upstream has moved to mastercheese-dev/CoreCE. Before-rebase backup:
backup/sandbox-before-rebase-20261025 (3dc0fbaa). Main OpenCE fetched at b09a3ea7.
No wholesale OpenCE merge. Public apple/ios-installer PR remains separate.

## Decisions

- CoreCE rebase retains native Metal, the cloak/child-function lifetime fix
  (5e8aa0cd), opening-screen update-cursor fix (361604aa), initial map-item
  placement (53e2c659), CE meter positioning and host hit validation. Local
  private storage, ISO import, checkpoint reconciliation, controller routing,
  versioning and branding patches replayed with resolved rename conflicts.
- Exclude bundled opening_frames.bin from both Apple packages. Runtime fallback
  remains available when user data supplies it. Keep the existing Halo icon,
  com.LoganShire.Halo packaging, and Foundation-selected private storage.
- Retain high_res_hud=false and restore the six changed Slayer-family bonus/
  penalty defaults to their previous values. Optional match settings remain
  host controlled. No claim of a complete retail-fidelity audit of CoreCE's
  mixed gameplay/menu changes is made.
- Adopt d17ecdd6: co-op BSP correction tests the player's center rather than
  feet a rounding error below the floor; no wire change.
- Adopt 667c2f29, 07c8430b, 32604c8e: bounded pending inventory list handles
  unit/weapon stream arrivals after inventory datagrams, expiry and game reset.
  Includes upstream regression and negative-control tests; no wire change.
- a689a8a3 framebuffer-unbind fix is already present in CoreCE; do not duplicate.
- 933cad12 line-of-sight validation and 384d9ba5 CE HUD fixes have counterpart
  adaptations in CoreCE (34eb9cf6, 3b184f81); do not replace those wholesale.
- 5744d2d1 scenery-shield behavior changes the wire version; defer it and its
  dependent tripmine fixes ef0e3be4/962c3eca and fixture update 4ab042e1 to a
  deliberate mainline protocol migration.
- Defer 1d38b3f9, cfb212fe, 63fd3677, dc7feb1c, 68d80483: host/game-mode,
  shield and browser changes belong to mainline's later protocol transitions.
  8e8fa4d7 and 0a8584b2 fix/test Gun Game, absent from this CoreCE baseline.
  782f4ce1 concerns mainline's PC vehicle-set menu, not this fork's menu layout.
- Defer Android-specific 34c17f11, 3975480a, 99c20307 pending a separate GLES
  renderer port/review; this Apple candidate defaults to native Metal.

## Compatibility boundary

This candidate uses CoreCE network 34. Mainline OpenCE uses 38. It cannot join
current version-38 servers; changing just the advertised number is unsafe.
This is a principal reason to consolidate on an Apple port following mainline.
Large inherited CoreCE changes and deferred mainline groups above have not all
been independently verified against retail gameplay. This is a tested Alpha
candidate, not a declaration of full parity or complete upstream review.

## Consolidation research

- Project Reach / HaloPad (chrissotraidis/projectreach) main 96a92cbe now pins
  OpenCE 273/network 38. Its STATUS.md records clean iOS and Mac builds and
  bounded two-device gameplay/map-transfer testing. It uses a small adaptation
  over pinned upstream source, reducing long-term merge work. Its latest public
  release v0.3.11 still uses network 32; inspect current main, not that release,
  for version 38. It does not establish native-Metal performance parity with
  this build, and sustained controller/thermal testing remains open.
- VISR (steverice/visr) is actively developing iOS, tvOS, visionOS and Mac,
  including native Metal and an AppKit Mac host; protocol constant is 38.
  CI documentation distinguishes compiled probes/builds from actual gameplay;
  visionOS builds remain advisory. Its Apache contribution policy explicitly
  excludes GPL/AGPL code, so our inherited patches cannot simply be transplanted.
- NicholasDominici/halo-ce-ios last pushed October 9 (ios-port), while
  zimm3rmann/halo-ce-ios-macos last pushed September 27. bmanturner's Apple/tvOS
  fork last pushed October 22 but has an older mainline merge.

Recommended next step: evaluate Project Reach current-main packages with the
same ISO, controller and saved checkpoint before replacing this app. Compare
VISR if native Metal and Vision Pro are essential. Keep only independently
reviewed missing fixes for upstream contributions, not another permanent engine
fork. No maintainer outreach, PR update or repository migration performed here.
