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
