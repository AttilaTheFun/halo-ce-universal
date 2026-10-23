# Apple rebase, October 22, 2026

Working branch: `apple/sandbox-sync`. Backup: `backup/sandbox-before-rebase-20261022`
at `ae45d69f`. Replayed 41 local commits from Apple baseline `c45870c2` onto
`innerhat/main` at `41a4efee`. Main upstream was fetched at `ce95f7de`.
These histories use selective imports; this is not a wholesale main merge.

## Selected changes

| Revision | Decision and effect |
| --- | --- |
| `cdce3364` (main merge `ce95f7de`) | Adopt with attribution. Co-op trigger waits verify actual placement, preserve failed placements, and distinguish empty player lists from AI lists. Only documentation conflicted; retained both lift and door explanations. |
| `a6787dee` / main `9d7436bd` | Already inherited from Apple. Extra co-op players follow scripted lifts in the same reverse datum order as the script player list. Do not duplicate. |
| `0eca26ba` | Inherited Apple WebRTC loss handling: report up to 128 SCTP gap blocks, age gaps after one second, keep UDP path unchanged. Live browser interoperability still needs testing. |
| `3572886f` | Retain terminal logging disabled by default, matching our no-debug-text preference. |
| `1409cd3c` | Retain split-screen separators gated to the optional Hardcore panes. |
| `038252b1` (main merge `fc22e846`) | Not applicable: Apple texture_entry_result does not perform the per-texture config_boolean lookup this cache removes. |
| `a0f22cbb` (main merge `600edf7b`) | Defer: desktop GL persistent query-buffer batching is absent from this fork. Does not apply to native Metal or ANGLE GLES; importing it requires a separate renderer review. |
| `61621e65` | Exclude replacement cursor artwork and associated desktop SDL pointer flow; retain existing Apple input implementation. |
| `9662f0f6` | Do not import main's README branding rewrite. |

Restored `display.high_res_hud=false`, the previous local baseline. Existing
user configuration still takes precedence. Native Metal, ISO import, sandbox
storage, landscape-only iOS, optional GameSave, controller handling and internal
bundle configuration are preserved by the replay.

## Review limits

The Apple baseline also contains large mixed integrations (`5bd4cebe`,
`fe210408`, `4557de80`, `ae4317a5`, `93d429ab`) and documentation commits
`428467b6` and `41a4efee`. These are inherited by the requested Apple rebase;
this record does not certify every gameplay/artwork change in them against
retail Xbox. Optional competitive/Hardcore flows require further fidelity
review. Earlier review gaps remain open. Do not advance a claim that all main
commits have been independently reviewed to `ce95f7de`.

## Validation

See the final validation section below for completed builds and tests.
Full gameplay, real browser multiplayer and live iCloud syncing are not
established by compilation or the isolated harness tests.
