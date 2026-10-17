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
