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
