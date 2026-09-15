# Lunax

Lunax is the Luna ecosystem manager. It installs and selects Luna compilers, toolchains, and
SDKs; maintains the environment passed to Luna; downloads artifacts through a replaceable
backend; and forwards every command it does not own to the selected Luna compiler.

This is a Luna 0.3 migration release. The command dispatcher is written in Luna. A
narrow native host library temporarily supplies argv, process, path, dynamic-library, and
libcurl operations that Luna Std cannot yet express. The compatibility layer is designed to
shrink as the standard library matures.

Lunax is dual-licensed under MIT or Apache-2.0. Published compatibility is declared in
`compatibility/luna.json`. The initial prebuilt release target is Ubuntu 24.04 x86_64 and
requires the system glibc, `libcurl.so.4`, and `tar`; other platforms remain source-build only.
After publication, `published-release.yml` downloads the public archive and checksum on a clean
Ubuntu 24.04 runner, verifies the exact asset set and GitHub-recorded digests, extracts the
package, and runs its CLI and packaged compatibility checks.

## Build

Requirements: CMake 3.20+, a C++17 compiler, libcurl development files, `tar`, and a current
Luna compiler/runtime build.

```sh
cmake -S . -B build \
  -DLUNA_COMPILER=/path/to/luna \
  -DLUNA_RUNTIME_LIB=/path/to/libruntime.a
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/luna-src/build/native/lunax`.

## Commands

```text
lunax help
lunax version
lunax env
lunax backend
lunax which
lunax download <url> <output> <sha256>
lunax install <compiler|toolchain|sdk> <version> <url> <sha256>
lunax list [compiler|toolchain|sdk]
lunax use <compiler|toolchain|sdk> <version>
lunax package fetch <package-id> <version> <url> <sha256>
lunax package list [package-id]
lunax luna <luna arguments...>
lunax <any Luna CLI command...>
```

For example, `lunax check app` and `lunax build app -O2` are forwarded verbatim. Set
`LUNAX_LUNA=/path/to/luna` to choose an unmanaged default compiler.

## Environment

Lunax configuration uses `LUNAX_HOME`, `LUNAX_LUNA`, `LUNAX_DOWNLOAD_BACKEND`, and
`LUNAX_BACKEND_PATH`. It maintains `LUNA_HOME`, `LUNA_RUNTIME_LIB`, `LUNA_TOOLCHAIN`,
`LUNA_SDK`, and `LUNA_PACKAGE_HOME` for forwarded compiler processes. `lunax env` prints
shell assignments for interactive use.

The initial installer accepts a tar-compatible archive containing one top-level directory.
Downloads and cached archives must match the caller-provided SHA-256. Extraction occurs in
a sibling staging directory and is atomically published only after layout validation; failed
staging directories are retained for inspection and never appear as installed versions.
`package fetch` places a reverse-DNS package archive in the versioned local cache; the
current Luna compiler does not yet resolve that cache as a registry. Lunax does not yet
verify signatures, solve package dependencies, repair retained staging directories, or
remove files. It is a practical integration tool with integrity and publication safety,
not yet a complete production package manager. See [architecture](docs/architecture.md) and the
[backend ABI](docs/backend-plugin.md). Distribution dependency decisions are recorded in the
[distribution policy](docs/distribution.md).
