# Distribution policy

## Decision for Lunax 0.1.x

The Ubuntu 24.04 build links to the distribution `libcurl.so.4`. Lunax uses only the HTTP and
HTTPS easy API, while the distribution library can pull in a substantially larger optional
protocol and authentication dependency graph. Copying that graph into the archive would expand
the security and third-party-license surface without improving the supported platform boundary.

Lunax therefore uses two explicit package classes:

- the MVP tar archive remains an Ubuntu 24.04 x86_64 package with system dependencies;
- the next Ubuntu release also produces a native Debian package whose shared-library dependencies
  are calculated by `dpkg-shlibdeps` and which explicitly depends on `tar`.

CI builds the Debian package, inspects its control metadata, requires the Ubuntu 24.04
`libcurl4t64` and `tar` dependencies, extracts it without privileged installation, and runs the
packaged binary. Releases attest every package and checksum through the repository release
workflow. Lunax 0.2 additionally publishes checksummed and attested `LUNA-SOURCE-COMMIT`
evidence for its exact compiler candidate, and clean consumer verification requires those
GitHub/Sigstore attestations. The
`.deb` is not added retroactively to the immutable v0.1.0 release.

## Deferred portable archive

A cross-distribution archive may later build a minimal libcurl with only HTTP/HTTPS and the
required TLS and certificate features. It must pin source revisions, disable unused protocols,
record the complete dependency graph, include every required third-party notice, and pass tests
in a clean environment without system libcurl. Until all of those gates exist, Lunax does not
claim that its archive is self-contained.
