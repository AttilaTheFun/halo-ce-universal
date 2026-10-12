# Internal Apple distribution

Branch stack: `innerhat-dev:main` → `AttilaTheFun:apple/ios-installer` →
`AttilaTheFun:apple/ios-internal`.

Only `apple/ios-installer` is proposed upstream in PR #1. Keep functional fixes
there first, then rebase this branch onto it. Do not open a PR from this branch.

This branch owns the Halo display name, personal ring artwork and
`com.LoganShire.OpenCE` distribution identifier on iOS and macOS. Keep signing
credentials, team identifiers and provisioning profiles outside the repository.
Installer builds come from this branch; increment the build number for each
publication. Never bundle retail game images or install on physical devices as
part of automated testing.
