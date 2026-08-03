# Lunax architecture

Lunax is the Luna ecosystem manager and the first application intended to exercise Luna as
an implementation language rather than only as a compiler fixture. Its durable boundary is:

```text
Luna CLI policy
  -> narrow Lunax host C ABI (temporary for Luna 0.2)
     -> process/filesystem/environment services
     -> download backend ABI v1
        -> builtin libcurl backend (default)
        -> dynamically loaded third-party backend
           -> future wget backend (not implemented before Luna 0.3)
```

The command dispatcher, argument policy, Luna CLI inheritance, and user-facing behavior live
in `src/*.luna`. `native/src/Host.cpp` is compatibility infrastructure for capabilities that
Luna 0.2 cannot yet express safely: indexing `argv`, owned path construction, process spawn,
variadic libcurl options, and dynamic-library symbol lookup. It must shrink as Std gains these
facilities; it must not become a second CLI implementation.

## Download backends

The public C-compatible contract is `include/lunax/backend_abi.h`. Every dynamic backend
exports `lunax_download_backend_v1` and returns a process-lifetime descriptor. Lunax validates
the magic, ABI version, complete structure size, reserved field, name, and callback before use.

`LUNAX_DOWNLOAD_BACKEND=curl` selects the builtin backend. Any other value is treated as a
backend name or library path. Name lookup searches `LUNAX_BACKEND_PATH`, then
`$LUNAX_HOME/backends`, then the platform loader path. The future wget implementation must be
a separate shared library implementing this ABI; Lunax core must not link to wget.

Backend v1 downloads one URL to one new file. It follows redirects, requires TLS certificate
and hostname verification, rejects overwrite, and returns a caller-owned diagnostic. A failed
download is retained for inspection because the current project policy performs no implicit
deletion. Lunax core creates the destination directory and verifies the caller-provided SHA-256
after every backend download, so integrity policy does not vary between builtin and plugin
backends.

## Managed installations

Lunax stores state below `LUNAX_HOME`:

```text
$LUNAX_HOME/
├── active/{compiler,toolchain,sdk}
├── backends/
├── compilers/<version>/
├── downloads/
├── packages/<reverse-dns-id>/<version>/
├── sdks/<version>/
└── toolchains/<version>/
```

Versions are restricted to letters, digits, `.`, `_`, and `-`; this prevents path traversal.
Install and package fetch refuse an existing target and never overwrite a download. Cached
archive names include the expected digest, and the digest is rechecked before every extraction.
The MVP expects a tar-compatible archive with one top-level directory and invokes
`tar --strip-components=1` in a unique sibling staging directory. A compiler layout must contain
`bin/luna` (`bin/luna.exe` on Windows) before the staging directory is atomically renamed to its
final version path. Extraction or validation failures are retained and reported without making
the version visible. Removal, repair, signatures, remote indexes, and dependency solving remain
future work.

## Environment and compiler selection

Compiler resolution order is `LUNAX_LUNA`, the active managed compiler, then `luna` on PATH.
Before forwarding, Lunax maintains the child environment:

- `LUNAX_HOME`
- `LUNA_HOME`
- `LUNA_RUNTIME_LIB`
- `LUNA_PACKAGE_HOME`
- `LUNA_TOOLCHAIN`
- `LUNA_SDK`
- child `PATH`, prefixed by the active compiler's `bin`

Backend selection is controlled separately by `LUNAX_DOWNLOAD_BACKEND` and
`LUNAX_BACKEND_PATH`; `LUNAX_LUNA` overrides managed compiler selection without rewriting
the active-version files.

`lunax env` prints shell assignments because a child process cannot mutate its parent shell.
Use `eval "$(lunax env)"` in POSIX shells when the current shell needs the same environment.
