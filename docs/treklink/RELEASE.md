# TrekLink firmware releases

## Cut a release

1. Confirm the approved firmware commit contains all release changes.
2. Create and push a tag matching `v*-treklink.*`, such as `v2.7.26-treklink.1`.
3. Wait for the `TrekLink Firmware Release` workflow to build and publish the GitHub release.

The manual workflow accepts an existing tag through its `tag` input. It checks out and builds that
tag, which is useful when retrying a failed release workflow.

## Release assets

Each release contains app and factory images for `treklink-v2`, `treklink-v3-tbeam`, and
`treklink-v4-supreme`. App images use offset `0x10000`. Factory images use offset `0x0`.

The release also contains one `treklink-manifest.json`. It records the release version and each
variant's code, PlatformIO environment, chip, image file names, offsets, byte sizes, and SHA-256
digests.

## Web application access

The devices module reads `treklink-manifest.json` from this repository's GitHub releases. Devices
API design 28 lists releases for a variant. API design 29 selects an image, verifies its SHA-256
against the manifest, and serves it to the web application.
