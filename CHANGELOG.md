# Changelog

## 0.1.1 - Development

- Added post-publication consumer verification for the exact GitHub asset set, checksums,
  extracted CLI, backend, and packaged compatibility declaration.
- Added a native Ubuntu Debian package gate with automatically discovered shared-library
  dependencies and an explicit `tar` dependency.
- Kept the portable archive dependency strategy explicit instead of bundling an unaudited
  libcurl runtime closure.

## 0.1.0 - 2026-08-04

- Added the Luna-written CLI dispatcher and transparent forwarding to the selected Luna
  compiler.
- Added compiler, toolchain, SDK, and remote package-cache installation/list/selection
  commands.
- Added the default libcurl downloader with TLS verification, redirect handling, timeout,
  and no-overwrite behavior.
- Frozen download backend ABI v1 and dynamic backend loading while intentionally deferring
  the wget backend until after Luna 0.3.
- Added managed Luna environment output and child-process propagation.
- Required caller-provided SHA-256 verification for downloads, installs, cached
  archives, and package fetches independently of the selected backend.
- Added sibling staging and atomic publication, including compiler-layout
  validation and full install/list/use/forward/package integration coverage.
- Added Linux CI against an explicitly selected Luna compiler revision.
- Declared the Luna v0.2.1 compatibility baseline and backend ABI in a packaged manifest.
- Added MIT OR Apache-2.0 licensing and an Ubuntu 24.04 x86_64 prerelease boundary.
