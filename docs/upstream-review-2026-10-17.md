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

### Completed follow-up review

- `d38b5fd710ce178e5e6df40fe8231bd7a5c8fbb8`: exclude and reverse. The
  diff removes original player/vehicle-count restrictions on dynamic lights and
  integrated lights, affecting flashlight availability and visibility during
  multiplayer. Restored the prior thresholds and vehicle counting, including its
  long counter for map flags. This follows the original-Xbox baseline policy;
  increased renderer capacity alone does not authorize changing these rules.
- `a93cf12071695fb2a14b65e5c228019144902f09`: retain. Completed all seven
  file diffs and checked host gating in `players_following`/`coop_scripts_host`.
  Additional network co-op players follow a scripted flag, searching nearby
  positions with line-of-sight and BSP-switch checks; fallback ignores player
  collisions only for that teleport and resets the flag immediately. Network
  clients defer movement to the host. Solo/local trigger teleports retain their
  ordinary orientation path. This repairs the existing optional network co-op
  mode, not the original two-player campaign rules. Live grav-lift/cutscene and
  crowded fallback testing remains required; compiled builds are not that test.
- `962d20d372b67e1f59cd9e1e0bd00634cece2da4`: retain Mac ANGLE fallback
  to rebased indices instead of EXT/OES base-vertex entry points. The condition
  excludes iOS and does not change native Metal. Original upstream build-number
  metadata is superseded by explicit local build numbering. Full diff inspected;
  the ANGLE menu-ring image has not been compared here.
- `7b5f8402f0bb16c369a1719bf73c6f84016304d4`: retain test-only changes:
  assert map-list refresh counts and remove unused fake bookkeeping. Complete
  diff inspected. The earlier macOS `-m32` harness limitation still applies.
- `04710ebb0e7090fcfac55b151d402659ceb6bfb5`: retain native Metal
  visibility counting and game-pixel normalization, with early depth testing for
  texture-free, non-alpha-tested, non-depth-replacing fragment shaders. Complete
  diff inspected. This repairs the query's sample-count contract rather than
  introducing a flare-size heuristic. GPU visual verification remains pending.

`67574819` has now been fully inspected but is not yet approved: it changes
sprite/glow depth testing in game renderer code beyond repairing query counts.
An Xbox reference comparison is needed to decide whether this is a faithful
correction. `70afd39e` is partially inspected only; its large render-pose/network
changes still need the untruncated remaining diff and dependency review.
`c1081648` is fully inspected but not approved pending validation of word-access
alignment/aliasing assumptions across the key's byte-array fields. The review
baseline remains incomplete and must not be advanced or published as finished.

### Network lifecycle and menu follow-up

- `85afe26a0cd58a9d0d93b6d36258a8182b36749a`: retain. All fourteen file
  diffs inspected, including sound fixtures and negative control. Host-directed
  flag/ball deletion now clears client objectives and bypasses only the normal
  protected-object deletion path. Downloading clients defer a start, checking
  the map name and games-played generation before resuming; the server holds
  requested players until their map is ready, deduplicating and bounding the
  pending list. Loop tracks stop when their source is deleted even within the
  same clock millisecond. No packet-layout change. Live CTF, interrupted download,
  and audio tests remain pending; 32-bit fixtures cannot run on this macOS host.
- `03285b910a61a2972aa83f4308cdc4dd7a5bad6f`: retain. Metal propagates
  menu-active state for Enter-as-A; postgame map lists return to the main menu
  when history is empty. The script evaluator accepts 32 choices, which fit its
  existing 32-bit mask (choice indices remain 0 through 31). Actual full diff
  and evaluator allocation/indexing inspected; live postgame navigation pending.
- `864c636158d458009496504bb8e0ea54a236bacd`: retain. Fast network setup
  and automated join resolve the selected port menu's lobby through the same
  mapping as normal navigation. Full diff inspected; no simulation/wire change.
- `fd277b0cfde833a15e231cd56c31f192ce4f1084` and
  `fcbf29e3774cc45e8c6654f3353a175eab3d967d`: retain menu occlusion fix
  and fixtures. Scenario title timers continue while menus hide their drawing;
  gameplay without menus is unchanged. Full production and fixture diffs
  inspected, including both mutation controls. Fixtures require the unavailable
  32-bit runtime; no live Custom Edition title/menu comparison performed here.

Completed the remaining `70afd39e` network-object and render-interpolation diffs.
Retain its bounded host-sample interpolation for unsteered vehicles, replacing
extrapolation and taking large jumps immediately. Remote render poses use
quaternion blending; local-player poses and local campaign are excluded from
that additional path. Simulation remains 30 Hz and packet layout is unchanged.
Client vehicle positions now lag a host sample, so live collision/boarding and
cutscene tests remain necessary. A new production-predicate regression exposed
an omitted roll-only change: identical position/forward with different up axes
is treated as unchanged. Correct this before considering the integration ready.

`9ca2ae94` and `86193542` remain partially reviewed only; their outputs exceeded
the review chunk and need file-by-file completion. Do not infer approval from
those commit names or from their presence in the inherited baseline.

The roll-only predicate defect is corrected: compare both forward and up axes. `tools/test_remote_pose.py` extracts the production predicate and reproduced the failure before the fix; it now passes under ASan/UBSan for unchanged poses, roll, yaw, translation and a non-finite up axis. This validates change detection, not full network/render interpolation.

### Protocol 29 community-map identity review completed

Retain `9ca2ae94da4bcba8c9bed04893c058ccd145f2c0`; all seventeen changed
files have now been inspected in full, including documentation and positive/
mutation fixtures. This is a protocol change: names remain human-readable while
the game record carries a nonzero version derived from the first four hash bytes.
The loader selects that version rather than whichever same-name file it finds.
Downloads keep an existing same-name map, using a hash suffix for another
version; offers must match the advertised version. Old hash-suffixed files are
renamed only when their actual hash matches, no unsuffixed file exists, and they
are neither loaded nor being shared. Version lookups are invalidated when maps
change and otherwise cached for one second; hashes are cached by path/size/time.

The protocol bump is retained together with the lookup/offer/download changes,
not as a stand-alone advertised number. Stock Xbox maps still advertise map
version zero. Full piece hashes remain part of transfer validation, but the
32-bit map version is not a collision-proof identity: do not repeat the upstream
prose's absolute claim that two different versions can never meet. Files changed
externally without changing size/time are another cache limitation. No change to
weapons or simulation tick rate is introduced by this commit. These paths compile
in current Apple builds, but the 32-bit harness and live community-map transfer
still require execution in a suitable test environment.

### Word comparisons and mixed Mac update

Retain `c1081648517596845bcd62639f46ca27cf91dc2f` under the existing
ILP32 guest build contract. The full diff and current call sites were inspected:
inputs are DWORD arrays, shader keys have word alignment and word-multiple size,
and both compared keys use the existing initialization path. The actual generated
guest Ninja flags include `-fno-strict-aliasing`, as do Linux/Android/Windows
build definitions. This closes the previously pending aliasing concern for these
builds; it does not certify arbitrary strict-aliasing consumers. No new measured
performance gain is claimed.

The complete Metal resource/program indexing portion of `86193542` is now
reviewed: retain bucket lookup with full-key equality, publishing resources in
both indexes, and unindexing retired history resources. The later `c45870c2`
menu resource invalidation remains required. The rest of the mixed commit is
still under review (branding, UI, packaging and documentation); the review
baseline is not advanced. Installer-managed builds must keep their own Halo
identity and cannot inherit OpenCE release-label assumptions.

### Mixed naming/packaging change review completed

Completed all remaining file diffs of `861935420df053f09b81e2c328838ecac7e684cf`.
Partially adopt: retain native Metal resource/program indexing, the protocol
version progression (now superseded by 29), and release-audit support for a
Metal-only dependency set. The OpenCE naming substitutions do not replace the
user-requested internal Halo identity; existing internal host/build overrides
remain authoritative. Upstream historical documentation is not evidence of a
new release or of this branch's runtime paths. Public/internal branding still
needs separation before the final branch stack is published.

The inherited DMG helper rejected Halo.app and hardcoded OpenCE in its layout.
It now reads the build's APP_NAME for bundle validation, volume name, icon
positions and layout audit, and renders a temporary copy of the background with
that name. Upstream artwork remains unchanged. The four packaging failure-boundary
tests pass with the configured Halo bundle name; no disk image was created or
published. The existing content audit also passes on installed Mac build 37.
That content audit is not a team-signing, provisioning or notarization check.

Retain `63e4366a92abae3ddd8026d6e483ffec74835b89`: the complete diff was
inspected, including fresh updater launch, disabled window restoration, accurate
fullscreen logging, and reapplying the saved fullscreen state on activation.
Checked the current setter: toggling fullscreen updates HALO_WINDOWED before
calling host_menu_window_changed, so a deliberate windowed choice is preserved.
Installer-managed builds disable the independent updater. No Mac GUI update/
activation test has been performed.

Retain the message-box bridge in `14b0f83f5736045fe6e4f8a60ecae77ecc72c803`:
the host presents up to three supplied labels with stable original button IDs,
returns the chosen ID, or -1 on presentation failure. Full diff inspected.
This does not approve the still-unreviewed mod-loading feature or any replacement
assets; it only provides the host/guest UI boundary. No mod prompt was exercised.

### Legacy storage and updater follow-up

- `c6c41adf79ae3845c70b579ed983f0bb67087e04`: all eighteen file diffs
  inspected. Partially retain migration correctness: standardized record paths,
  preference for the newer legacy source, and excluding prior migration records
  from a new copy. Its unsandboxed OpenCE default path and iOS display name are
  superseded by the Halo container/build changes. Sandboxed first launch cannot
  automatically access external legacy folders; the explicit saved-game folder
  picker and queued restore are the current migration path. Existing fixture
  validation applies to helpers, not live sandbox picker permissions.
- `07c80a5cb0cd19bcb02eced22ced28fa47f4cc45` and
  `f67a5110175318725bfcf803af1648a4edebf095`: retain final wording in the
  standalone updater UI; full one-line diffs inspected, no behavioral change.
- `147a7e4b0c956b52b91977b028f56ecbfa40b420`: retain the standalone
  updater's completion semaphore and CFRunLoop progress scheduling, avoiding a
  completion block waiting on the main queue already occupied by the installer
  method. Full diff inspected; no actual update download or GUI test executed.
- `49c94068925a8e145ac7dc57fbcbe6c372717591`: full four-file diff
  inspected. Retain only as part of the standalone updater feature: moves the
  notice from the per-player HUD to the shared overlay and uses the terminal
  font's top band. Installer-managed builds do not create the fork updater,
  so they do not populate this notice. Its precursor `997fb9d2` and updater
  implementation `0290a825` still require complete review; neither is approved
  merely by reviewing this small follow-up.
