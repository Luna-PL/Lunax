# Roadmap

## 0.1: practical Luna 0.2 integration

- Luna-owned CLI policy and transparent Luna command forwarding;
- compiler/toolchain/SDK version directories and active selection;
- remote package archive cache;
- libcurl downloads and backend ABI v1;
- explicit native compatibility boundary.

## Luna 0.3 migration

- replace argv/path/process helpers with Luna Std;
- replace borrowed `cstr` command data with owned Luna String/Vec;
- expose structured `Result` errors instead of integer host statuses;
- connect the package cache to compiler dependency resolution and lock generation;
- add checksum/signature verification and transactional install state;
- remove the compatibility host surface operation by operation.

## After 0.3

- implement and distribute wget as a separate dynamic backend plugin;
- remote indexes, mirrors, authenticated registries, dependency solving, and offline mode;
- repair, rollback, garbage collection, and explicit removal commands;
- self-update only after signed metadata and rollback are available.
