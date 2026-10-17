# Apple sandbox rebase review — in progress

Target Apple baseline: `c45870c2e14425b662d4f5d23789a92851f84403`.
Main-upstream target: `7524ba61` (fetched revision). This ledger is partial;
it does not advance the last fully reviewed revision. Remaining commits must
be reviewed before publishing the rebased contribution.

| Commit | Actual change and decision | Evidence and limits |
| --- | --- | --- |
| `c45870c2e14425b662d4f5d23789a92851f84403` | Retain: native Metal menu-art resources are unindexed, freed and removed from the resource list when their texture registration is replaced or forgotten. Prevents address reuse from displaying a prior menu image. OpenGL path remains unchanged. | Full diff inspected. Mac build 32 compiled this code; actual map-transition rendering remains unverified. |
| `0997b42ba6eb8222e65b7e9fe962a96967f1f57a` | Defer/exclude from this baseline: suppresses first-person flares by radius band and scales growing first-person flares by 0.7. The radius test is a heuristic across tag definitions, not an identified plasma-pistol-only property. Reverse this commit while retaining prior renderer fixes. | Full diff inspected. No Xbox visual comparison establishes the heuristics as faithful corrections. Reversal requires new guest builds. |
| `d9669f6eef2362624cef4c04a59088634d0d5c61` | Retain already-inherited fix: assembly conversion preserves literal string directives, handles escapes in quoted symbol names, and retains symbol renaming. Prevents byte tables and underscore-leading strings from being corrupted. Includes regression tests and CI invocation. | Full diff inspected. Seven converter tests passed, including an actual arm64_32 compile/conversion/ELF assembly byte-table round trip. Upstream voice stress-test claims were not rerun locally. |
| `9c70f5f5e3c0eafcee0086e12ac37711161f8039` | Retain already-inherited guard: Opus packets must describe the protocol's 20 ms frame before decoding. Rejects malformed/unsupported frame duration without changing valid packets. | Full diff inspected; guard present in source. No live microphone or multiplayer test performed. |

| Commit | Actual change and decision | Evidence and limits |
| --- | --- | --- |
| `e5e97c052626235ee6f8736f66fd12e0b52dc0b2` / Apple `659bc5ec` | Retain together with the following correction: unsteered client vehicles follow positional differences between host samples, including recorded lifepods without pilots; teleports over the step threshold do not produce extrapolation. Host simulation and wire layout are unchanged. | Full diff inspected. Campaign co-op lifepod/Pelican behavior needs a live host/client test; not claimed to fix the earlier single-player cutscene report. |
| `7cf24ae589b308bb7be66e097681eeaed66f7905` / Apple `794f22fb` | Retain: host at-rest state zeros velocity, and extrapolation stops after the last observed sample interval. Prevents stale packets allowing unoccupied vehicles to drift indefinitely. Initializes tracking on a new game. | Full diff inspected; dependent on preceding path tracking. Timing/fairness and live network behavior unverified. |
| `4301ca88d6b5fa6cd4e98520672790bbbce425e7` | Retain already-inherited fix: postgame map-selector wrapper adds main_menu_if_no_history in both generated XML and generator patch, so Back has a main-menu destination. No wire/protocol change. | Full diff inspected. Runtime postgame navigation still requires testing. |
| `521c44f862f474da599564dc8c985251c731691f` / Apple `04aa59cf` | Retain: only at non-stock field of view, screen-fog redraw uses depth equality and retains depth, keeping fog on the drawn canopy. Stock-FOV path unchanged. | Full diff inspected. Native Metal and ANGLE wide-FOV canopy visuals remain unverified. |
| `d41109385a00ff0b373dba429fcfabc48c9d092b` | Retain: an extended p2p-select wait resets STUN observations, reconnects brokers with reset retry delay, and ignores the stale ready-socket lists. Relevant to phone backgrounding and Mac sleep. | Full diff inspected. No wire change; live resume/rejoin test remains necessary. |
| `92b8ac527bc08fb9426cd91863a5198f9e6ac878` | Retain: GLES base-vertex draws require a loaded core pointer or advertised and loaded EXT/OES entry point. Optional pointers do not fail initialization; WebGL does not query them. | Full diff inspected. iOS build 33 compiled this path. Actual GLES/ANGLE draw comparison remains pending. |

Map-sharing hardening `77e27537e05198cff78612719604898af0f3ad83` is
**partially inspected only**, not approved as fully reviewed: inspected geometry
vertex limits, tag-name overlap handling, required globals, and bounded debug
formatting; remaining map-share/decoder/test diffs still need inspection.
Attempted the map-share restart, trust, and server-teardown harness suites.
All 64 cases failed at compilation because the default Apple clang `-m32`
target has no runnable 32-bit macOS runtime on this host. No test bodies ran;
this is not a passing validation or evidence of 64 game logic failures. The
harness README explicitly requires a Linux 32-bit C runtime. Do not remove the
ABI requirement just to make these tests pass on the host.

### Map-sharing hardening review completed

Completed the remaining diffs of `77e27537e05198cff78612719604898af0f3ad83`,
including the decoder and all added/modified regression fixtures. Retain the
already-inherited change. Remote piece bitmaps track their allocated piece count,
are forgotten after a download ends, and are checked/reallocated before access.
Failed finalization removes only newly moved files and refreshes map discovery;
previously existing files are preserved. Repeated map requests are rate-limited
with the latest request queued, and switching requested names cannot reset the
current game's no-progress timeout. These are transfer validation changes, not
weapon/simulation changes or a new wire layout. Review the five-second retry
interval in live join/reconnect testing.

The same commit caps geometry counts consistently at 64000, rejects missing
required globals, prevents tag-name overlap with mutable data, bounds debug
formatting, and masks the Vorbis floor index consistently with draw_line. Native
cache-parser tests for 64000 accepted/64001 rejected passed (2 tests), with
undefined-behavior trapping. This module explicitly supports the 64-bit host;
this does not replace the still-unrun 32-bit map-sharing harness. Its positive
and mutation-control test diffs were inspected but their runtime assertions have
not run on this Mac. No downloaded community map or live peer was used.

### Audio resampler series

Full diffs reviewed, including generated XML, config/generator edits and mixer
regressions. Retain the final series as an opt-in audio-processing setting;
sinc remains the configured/default fallback and reverb selection is independent.

| Apple commit | Main-upstream equivalent | Decision and effect |
| --- | --- | --- |
| `497b7fcc43851454a1bc1f9c17b4ae3f4b805566` | `4afb5966` | Superseded by the next commit; do not retain its intermediate coupling of reverb-off to linear resampling (or its incorrect history indexing). |
| `65a4c37a1aaa935db7b6b74c82f92e0e790d5f06` | `39fcfdaf` | Retain final independent audio.resampler setting: linear optional, sinc default; correct per-channel history indexing. |
| `75f2df1a576011deb67c0205e377bb0b379e941c` | `0f3ceb9e` | Retain comments clarifying spectral aliasing; no runtime change. |
| `e1587dab683534ca5467c4ac0fa57613b8676739` | `5a1b8a40` | Retain final Sound Processing submenu, generator/manifest/XML, independent reverb/resampler settings, simplified setting fallback and contiguous mirrored-history reads. Mixer fixtures cover interpolation against computed samples and a nearest-neighbor mutation control. |

Audio menu XML parses successfully. No live audio/reference comparison and no
32-bit mixer harness execution on this Mac; the known host ABI limitation applies.
The optional mode does not establish equivalence to original Xbox audio.

### Rendering capacity and avoided clock reads

| Commit | Decision and actual effect | Validation limits |
| --- | --- | --- |
| `60004d153ce1af912bb73a5f5f280d50c93dfc9d` | Retain: short-circuit clock reads for inactive keys and unheard/muted voice indicators; collision timings read clocks only when timing is enabled and a start timestamp exists. Does not change tick frequency or held-button calculation. | Full diff reviewed; no measured performance gain claimed. |
| `9aae4b18f1e04cbee502957b759ede0ac0522dd1` | Retain: dynamic-triangle capacity increases 49152→98304, retaining allocation refusal at the limit. Prevents omitted rendering under high geometry loads rather than changing game rules. | Full diff reviewed; peak-memory and heavy-scene verification pending. |
| `1e2a951569df5cfc5751713d0ad4f5da94157fc8` | Retain: debug model vertex scratch buffer becomes static to avoid 128 KB stack allocation/probing per rendered model. | Full diff reviewed; assumes existing serial rasterizer use, no new parallel rendering introduced. |
| `fd8157c074eb22a4e2168a152e2c7807daff2214` | Retain: transparent group capacity increases 384→2048 to avoid dropped render groups. No change to object simulation or enabled gameplay variants. | Full diff reviewed; actual crowded-scene rendering not tested. |

The final resampler's contiguous two-sample access was also checked against the
actual mirrored history allocation/write path (two times RESAMPLER_HISTORY per
channel), confirming its ring-wrap assumption in source.

### Additional platform review

| Commit | Decision and effect | Evidence |
| --- | --- | --- |
| `4aecd050531abbc2995199cfa4e9de2bc8613bcf` | Retain: static tag accessor uses __inline__ to avoid the Windows shim's weak-symbol interpretation of __inline. | Complete diff inspected; Apple compilation includes header, Windows build not run. |
| `9ade750f7d22114bfef3351a7aab7f9b6589bfaa` | Retain: test stand-in clients add a distinct 32-hex-digit hardware ID to the join request. | Complete diff inspected; test tooling only, no live bot session run. |
| `c90a3bab81e3d604202e83cdde447f96e2f015f7` | Retain: classify a nav point with NONE screen type after spawn, or skip it without a player unit, before accessing arrow sequences. | Complete diff inspected; spawn timing rendering unverified. |
| `fc4cc8cda5115345679a0247d8764733da14e4a6` | Retain pending comparative testing: default iOS interpolation off, explicit saved player choice still wins. No simulation-rate change. | Complete one-line diff inspected; performance comparison remains pending. |
| `7599543bda2a3a9a59bb7dbb1ac494a1687dc03b` | Retain update initialization ordering for standalone builds: ask for game data before starting updater UI. Installer-managed builds already disable that updater. | Complete diff inspected; no update-service request or GUI test run. |

`6462dffdc751f104fac7e9e13cedb1c3b4e2c437` runtime-directory documentation
is partially reviewed and needs adaptation to the new sandbox. Its profile/map
selection file descriptions were checked against actual engine read/write sites,
revealing required sync metadata outside the snapshot's current two directories.

### Configuration repair

Retain `0ed8fe3a2bb7456b3be7bb9638151391de4bf319`: generate each TOML
section once and join repeated sections from affected older files before parsing.
The repaired text is written only after successful parsing. Full implementation
and fixture diff reviewed. The existing test initially failed compilation because
the Apple fork's newer port_config.c also includes input_bindings.def. Added that
real include to the fixture's copied inputs. The production-parser regression now
passes, covering fresh defaults, saved setting updates, and preservation of both
sections' values during repair. This does not certify every possible TOML syntax.

Also inspected complete diffs of `d38b5fd710ce178e5e6df40fe8231bd7a5c8fbb8`
(always retain multiplayer flashlights/dynamic lighting beyond original Xbox
capacity thresholds) and `ebc5b8019937b258fb9bf3ab8398309ad862c6e5` (ignore
scripted BSP switches in active multiplayer). Decisions remain pending: the former
changes original lighting behavior under load; the latter can change custom-map
script semantics even though it avoids disruptive reloads. Neither is yet marked
accepted by this ledger. Co-op teleport `a93cf120` was only partially inspected.
