# Internal Apple distribution

Branch stack: `innerhat-dev:main` → `AttilaTheFun:apple/ios-installer` →
`AttilaTheFun:apple/ios-internal`.

Only `apple/ios-installer` is proposed upstream in PR #1. Keep functional fixes
there first, then rebase this branch onto it. Do not open a PR from this branch.

This branch owns the Halo display name, original Xbox title artwork and
`com.LoganShire.OpenCE` distribution identifier on iOS and macOS. Keep signing
credentials, team identifiers and provisioning profiles outside the repository.
Installer builds come from this branch; increment the build number for each
publication. Never bundle retail game images or install on physical devices as
part of automated testing.

Before building, extract the original title icon from your own disc image:

```sh
python3 tools/internal_disc_icon.py /path/to/Halo.iso
```

The generated catalogue stays in ignored `build/internal-artwork`; no extracted
retail artwork is committed. Both Apple builds use this icon, and the iOS importer
uses it as launch artwork. The IPA includes a standard PNG copy for Installer.

## Mac Installer distribution

Build with `tools/macos_build.py --host-only --release --install
--sign-identity "Developer ID Application: YOUR IDENTITY"` (all on one line).
The internal release can omit Sparkle feed configuration because Installer
manages distribution. A partially configured feed is still rejected.

Archive `build/macos/Halo.app` with `ditto -c -k --keepParent`, submit the ZIP
to Apple using your local notarytool profile, then staple the app and re-create
the ZIP. Verify the app with `codesign --verify --deep --strict` and
`spctl --assess --type execute` before publishing:

```sh
installer-server publish build/releases/Halo-macOS.zip --notes-file /path/to/notes.md
```

Use a new `APP_BUILD` number for each publication. The server extracts the
original title icon from AppIcon.icns and lists this as Halo, with bundle ID
`com.LoganShire.OpenCE`, separately from the iOS build. No disc image ships.
