# Apple PR refresh, October 25

## Working state

The public PR is mastercheese-dev/CoreCE#1 (formerly innerhat-dev/OpenCE).
Its old head ff18344c is retained in backup/ios-installer-before-review-20261025.
The public branch is being rebuilt on c88fcf2d, rather than replaying implementations
already present in CoreCE under different commits. The private release branch
apple/sandbox-sync remains unchanged at 8cc4a6c7.

The remaining changes currently carried are the dynamic iOS guest arena, which
removes the Extended Virtual Addressing requirement, the player-one touch routing
correction, and a visible settings entry so a player does not have to discover
the shake gesture. The upstream names, icons and bundle defaults are preserved.
Both controller-routing probes passed. The simulator build is in progress; no
ready-for-review claim is made yet.

## Compatibility gate

CoreCE uses network 34 and corece/1/lobby topics with corece-lobby-1 signatures.
OpenCE b09a3ea7 uses network 38 and hceu/3/lobby topics with hceu-lobby-1
signatures. The prior public branch used network 29. Merely changing the browser
topics or version constant is not a valid interoperability fix. Whether the
public contribution stays against CoreCE or moves to a mainline-compatible port
is pending the user's direction. A mainline-compatible downstream build remains
part of the work; no compatible release has been produced by this refresh.

## VISR review

Reviewed https://github.com/steverice/visr at
7f08179473a5d096d718bb15881f520bf3fd6634. Its README describes native Metal,
AppKit, touch controls and Apple device support. docs/benchmarks.md explicitly
says comparisons are still in progress; no measured performance advantage is
established by that page.

Concrete useful candidates inspected:

- tools/test_shader_warming_identity.py exercises separation of map warmup
  aliases from upload identity, including unknown fingerprints, regional stock
  maps, renamed maps and path traversal. This is useful if we adopt portable
  warmup packs, but our Metal cache currently uses shader-source identity; do
  not insert VISR map aliases into that cache without a corresponding feature.
- port/ios/tests/metal_border_probe.m tests real sampler construction/cache/bind
  bodies with stand-ins only for Metal object boundaries. This is a useful
  approach for verifying sampler fidelity and failed allocations without
  claiming the probe proves GPU output. Compare with our existing Metal sampler
  tests before adding duplicate coverage.
- Automatic shader uploads are a separately enabled feature in VISR. They are
  not being enabled or copied here; local user-exported diagnostics remain the
  current behavior.

VISR is Apache-2.0 with component notices. Its contribution rules reject GPL/AGPL
imports into VISR. That policy matters for contributions back to VISR; it does
not by itself prohibit using appropriately attributed Apache code here. No VISR
implementation has been copied in this refresh.

## Completion gates still open

- Finish reusable-delta audit against the old PR and private branch, including
  private ISO retention, packaging, documentation and optional save sync.
- Address reviewer findings, distinguishing defects from feature requests and
  preserving the user's rule that touch controls cannot be hidden without an
  alternate input method.
- Decide and implement the mainline compatibility path; verify discovery and a
  real join rather than just constants or a populated listing.
- Build and test final Mac/iOS artifacts; test settings in a leased simulator.
- Rewrite PR description around final changes, attribution and current evidence;
  push the reviewed branch, inspect checks and mark ready only after validation.
