# Changelog

## 0.1.0 - Development

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
