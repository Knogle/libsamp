# Contributing

Libre-SAMP is a compatibility-first reverse-engineering project. Keep changes
small, reviewable, and evidence-tagged.

## Build

Clone with submodules:

```sh
git clone --recurse-submodules https://github.com/Knogle/libsamp
cd libsamp
```

Build the DLL:

```sh
git submodule update --init --recursive
reimpl/scripts/build_win32.sh
```

Build the ASI probe:

```sh
tools/asi_probe/build_win32.sh
```

On systems without a local MinGW-w64 i686 toolchain, use the project devbuild
toolbox if available:

```sh
toolbox run -c devbuild reimpl/scripts/build_win32.sh
toolbox run -c devbuild tools/asi_probe/build_win32.sh
```

## Evidence Tags

Use the project evidence tags in code comments, docs, and commit descriptions
when behavior is not self-evident:

- `OBSERVED_037`: directly observed from the original SA-MP 0.3.7 DLL.
- `PROBE_TRACE`: supported by ASI probe or runtime logs.
- `STATIC_037`: statically recovered from the original DLL.
- `OPENMP_REF`: derived from open.mp behavior or documentation.
- `GTA_REVERSED_REF`: derived from public GTA-SA engine research.
- `MTA_REF`: derived from MTA:SA, low priority and conceptual only.
- `INFERRED`: plausible but not proven.
- `TODO_VERIFY`: must be validated before being treated as exact behavior.

## Compatibility Rules

- Do not change public ABI, exports, calling conventions, struct layouts, or
  protocol layouts without an explicit compatibility note.
- Treat all server payloads as untrusted.
- Prefer safe stubs plus logging over incorrect semantics.
- Document important RVAs as module-relative addresses, not absolute runtime
  addresses.
- Do not leak exceptions across DLL, C ABI, or WinAPI boundaries.

## Public Repository Rules

Do not commit:

- original SA-MP binaries or installer contents,
- GTA San Andreas binaries or assets,
- proprietary game/client/server assets,
- private local traces with usernames, IPs, tokens, or home paths,
- local reverse-engineering workspaces,
- large copied pseudocode blocks from proprietary disassembly.

Project-generated assets may be committed only when their provenance and
license are documented in [NOTICE.md](NOTICE.md).

## Pull Requests

A useful pull request includes:

- a narrow change summary,
- evidence tags for recovered behavior,
- build/test commands that were run,
- known compatibility risks,
- open `TODO_VERIFY` items if any behavior is still inferred.
