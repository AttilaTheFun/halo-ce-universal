# Mainline protocol 38 migration: in progress, not distributable

The CoreCE PR remains targeted at CoreCE and ready for review. Internal
apple/sandbox-sync is now the workspace for selectively reviewing and adapting
mainline network compatibility. Its last pushed head d3031e6f carries only the
verified renderer/settings fixes. backup/sandbox-before-network38-20261025
preserves that point. Do not publish the following intermediate networking work
or claim compatibility from its version constant.

## Evidence and scope

Compared CoreCE c88fcf2d with OpenCE b09a3ea7. Differences include:
- CoreCE discovery topics/signature labels versus mainline's hceu/3/lobby and
  hceu-lobby-1.
- Game settings grow from 13,120 to 13,136 bytes; the browser also carries a
  gametype-kind byte. Equal-looking protocol numbers on divergent forks do not
  establish compatibility.
- Mainline host choices for co-op ally fire and shield effects, static scenery
  shield damage/recharge, and explicit OpenCE mode semantics.
- CoreCE's optional performance message extension and differences in reliable
  stream admission must be reviewed, not overwritten just to match the version.

The original Xbox baseline remains unchanged; server-selected optional rules
must be understood when joining a server that uses them. No blanket merge or
blind version bump is authorized by this approach.

## Selected changes

1d38b3f9 (local c1ce6c4e): adopted co-op ally-friendly-fire interpretation,
server settings, menu/config controls and regression tests. Reviewed the damage
function, settings transport bit, default, menu callbacks and positive/negative
controls. Default is true, preserving campaign damage; non-co-op and AI-owned
hits remain unchanged. Kept the current version constant during this unfinished
migration and did not restore the stale generated upstream network-format file.
The carried tests have not yet run against this adaptation.

5744d2d1: reviewed static-target classification and per-object shield recharge,
plus negative-control tests; not applied yet. Its tripmine follow-ups ef0e3be4
and 962c3eca are required review next.

Still pending: all subsequent protocol dependencies through 38, resolution of
CoreCE-specific divergences, format manifest, actual browser discovery and join,
Mac/iOS build and gameplay validation. The CoreCE PR and published Alpha builds
have not been changed to claim mainline interoperability.

## Continued migration (25 October)

5744d2d1, ef0e3be4, 962c3eca and 4ab042e1 adopted: scenery shield
recharge/damage, host explosion authority, position reconciliation for local
tripmine indices, and the stale damage fixture dependency. The adapted ally,
static-target, stale-owner and menu harness suites pass all 51 cases under
32-bit clang + qemu-i386 in a leased Linux VM (including negative controls).

cfb212fe partially adopted after reviewing mode implementations, authoritative
state validation, damage hooks, options serialization and their tests. Support
server-selected modes without replacing CoreCE's host/menu tools or adding new
default playlists. Existing variants retain mode NONE and Hardcore OFF; old
playlist options remain readable. Preserve CoreCE's flag-deletion protection,
except Infection's disposable skull. This is still an intermediate migration;
new mode tests and full build are pending, and no new build is published.

The 21 mode harness cases pass. dc7feb1c's kind-byte advertisement/parser and
browser labels are adapted; 8e8fa4d7 and 0a8584b2 fix/test battery weapons.
63fd3677 and 68d80483's authenticated reliable host shield-policy messages
are adapted, including PLAYERS' OWN. CoreCE's existing glow renderer consumes
that policy. Mainline's optional overshield rendering is absent in CoreCE;
its shader/Metal dependencies require separate review before claiming visual
parity for hosts selecting that effect. No default gameplay or local effect
was changed. The full migration remains unpublished and unvalidated.

1adbb588 full native Metal Mac build passed; installed signature and BuildInfo
verified. 40 mode/kind/battery/shield cases pass on leased Linux (91 with the
previous suites), plus the real lobby crypto/listing fixture (zero failures).
The old broad test_performance_network fixture is stale against current CoreCE
server functions (missing map-share/start-state stubs); it fails to compile.
A focused real-code test_mainline_join test passes for vanilla versus extended
host capability announcements. Preserve reliable-stream authorization; only
advertised extension hosts get private pre-join messages or the CoreCE pregame
hold. The legacy extension version cannot admit its older settings layout.

c9f3d07e adopted: signed live listings older than an hour are ignored, retained
ones retain the ten-minute window (including its independent restamping tests).
Discovery now uses mainline's signed namespace, with version 38 set only after
the generated wire manifest exactly equals OpenCE b09a3ea7's manifest. This is
not proof of a successful join. Final Mac/iOS build, fresh discovery and actual
join remain gates before publishing. The optional overshield visual parity gap
noted above remains. No updated build has been published.

## Live discovery verification

At 2f4d9a34, captured 357 real MQTT listing packets during a 22-second public
broker subscription/query. The current production crypto/listing decoder,
compiled for the game's 32-bit ABI in the leased Linux VM, accepted 38 signed
current-protocol games advertising 80 players. A negative-control build changing
only the signing namespace back to corece-lobby-1 accepted zero from the exact
same capture. No invite, player name, or captured packet is committed. Two other
public brokers timed out; HiveMQ answered. This verifies listing decoding and
availability, not tunnel setup, game loading or a multiplayer match.

The complete protocol-38 native Metal Mac build at 2f4d9a34 passed and was
installed in /Applications/Halo.app as instructed; strict nested signature and
BuildInfo verified. This is an ad-hoc local validation artifact with placeholder
version 0.4.0 (1), NOT a release or published update. Logs:
/tmp/halo-mainline38-mac-final.log. iOS simulator/device build is next; release
numbering/signing/publication and actual join remain outstanding.

## Completed native match and iOS builds

The native Metal Mac app joined an ongoing public retail Chill Out match on an
oss-leased Mac. Gameplay rendered, the local player entered the active simulation,
and live positions from the other players arrived. The test sent no chat or
automatic firing. The app was stopped and the lease released after verification.
This closes the native tunnel/settings/live-state gate; it does not establish
full custom-mode visual parity or an iOS on-device match.

iOS simulator and device builds both passed at 1904518d. The release retains
local saves by default and includes no game image, maps or opening movie.
