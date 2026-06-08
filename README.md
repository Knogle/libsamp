<div align="center">

# RetroMP

**Experimental SA-MP 0.3.7-R5 `samp.dll` compatibility rebuild.**

Runtime traces, original DLL reverse engineering, open.mp compatibility work,
and an ASI probe for reproducible client-side instrumentation.

[![build](https://github.com/Knogle/RetroMP/actions/workflows/build.yml/badge.svg)](https://github.com/Knogle/RetroMP/actions/workflows/build.yml)
[![status](https://img.shields.io/badge/status-experimental-orange)](#current-status)
[![target](https://img.shields.io/badge/target-SA--MP%200.3.7--R5-blue)](#what-it-is)
[![platform](https://img.shields.io/badge/platform-Windows%20x86-informational)](#build-from-source)
[![evidence](https://img.shields.io/badge/evidence-RE%20%7C%20traces%20%7C%20open.mp-success)](repo/RE_EVIDENCE_GUIDE.md)
[![license](https://img.shields.io/badge/license-TBD-lightgrey)](#license)

</div>

---

## TL;DR

```sh
git clone --recurse-submodules https://github.com/Knogle/RetroMP
cd RetroMP

reimpl/scripts/build_win32.sh
tools/asi_probe/build_win32.sh
```

Build outputs:

- `build-win32/samp.dll`
- `build-asi-probe/samp_probe.asi`

The build ships the vendored [Knogle/RakNet](https://github.com/Knogle/RakNet)
submodule as the SA-MP/open.mp-compatible network transport source.

Use this only with local development servers and controlled test environments.
It is not a cheat, bypass, or public-server abuse toolkit.

## What It Is

RetroMP is a drop-in-oriented replacement for the SA-MP 0.3.7-R5 client DLL.
The goal is protocol and runtime compatibility with 0.3.7-compatible servers,
including open.mp compatibility paths, while keeping the implementation
auditable and testable.

The project is driven by observed behavior from the original DLL, ASI probe
golden traces, and public engine/protocol references. Proprietary binaries,
game assets, and local reverse-engineering workspaces are intentionally not
part of this repository.

## Current Status

This is not feature-complete. The current network-enabled development milestone
can connect, enter the gameplay state on tested local servers, handle
chat/dialog flows, spawn the local player, create vehicles, show core HUD/UI
elements, and render a growing subset of TextDraw behavior.

Known active work areas:

- Remote player sync, interpolation, nametags, and radar blips.
- Full RPC coverage with safe stubs and bounds-checked payload readers.
- SA-MP custom object loading and material handling.
- TextDraw parity for model previews, sprites, spacing, selection, and alpha.
- Vehicle sync details such as components, lock state, objective markers, and
  streaming budget.
- CI parity checks once public reference fixtures are available.

See [repo/TASK_TRACKER.md](repo/TASK_TRACKER.md) for the current task tracker.

## Highlights

- Win32 `samp.dll` rebuild with PE/export compatibility tracking.
- Vendored Knogle/RakNet transport path for SA-MP/open.mp-oriented networking.
- Runtime bridge for GTA-SA state, UI, dialogs, chat, TextDraws, vehicles, and
  basic world state.
- ASI probe included under [tools/asi_probe](tools/asi_probe) for repeatable
  instrumentation and golden trace collection.
- Evidence-tagged documentation model:
  `OBSERVED_037`, `PROBE_TRACE`, `STATIC_037`, `OPENMP_REF`,
  `GTA_REVERSED_REF`, `INFERRED`, and `TODO_VERIFY`.

## Build From Source

### Requirements

On Linux, install:

- CMake
- Ninja
- MinGW-w64 i686 GCC/G++
- Git submodule support

Fedora example:

```sh
sudo dnf install cmake ninja-build mingw32-gcc mingw32-gcc-c++
```

Debian/Ubuntu example:

```sh
sudo apt-get install cmake ninja-build gcc-mingw-w64-i686 g++-mingw-w64-i686
```

### Build The DLL

```sh
git submodule update --init --recursive
reimpl/scripts/build_win32.sh
```

This is the same public CI-style build used by GitHub Actions. It verifies the
DLL surface, runtime bridge, and vendored RakNet-backed network path without
depending on local-only reference workspaces.

### Build The ASI Probe

```sh
tools/asi_probe/build_win32.sh
```

The probe builds to `build-asi-probe/samp_probe.asi` and is loaded by a normal
ASI loader from the game root or an ASI loader search path.

## GitHub Actions

The repository contains a CI workflow at
[.github/workflows/build.yml](.github/workflows/build.yml). It builds:

- `samp.dll`
- `samp_probe.asi`
- `SHA256SUMS.txt`

The CI artifact build intentionally avoids proprietary inputs and local-only
reference paths. Runtime parity still depends on golden-trace verification.

## Documentation

- [Task tracker](repo/TASK_TRACKER.md)
- [Publication checklist](repo/PUBLICATION_CHECKLIST.md)
- [Reverse-engineering evidence guide](repo/RE_EVIDENCE_GUIDE.md)
- [TextDraw render stack notes](docs/re/textdraw_render_stack.md)
- [Custom asset pipeline notes](docs/re/samp_custom_asset_pipeline.md)
- [ASI probe README](tools/asi_probe/README.md)

## Scope And Safety

This project is for compatibility research, local testing, and preservation of
0.3.7-compatible client behavior. Do not use it to cheat, evade bans, bypass
server protections, or disrupt public servers.

Server-provided data is treated as untrusted. New RPC handlers should be
bounds-checked, fail closed, and log unknown behavior before implementing
unverified semantics.

## License

License is still to be finalized before the first public release. Third-party
license notices and submodule license review are tracked in
[repo/PUBLICATION_CHECKLIST.md](repo/PUBLICATION_CHECKLIST.md).

## Credits

- SA-MP and GTA-SA modding communities for protocol and engine knowledge.
- open.mp for public server-side compatibility references.
- gta-reversed for public GTA-SA engine research.
- Ultimate ASI Loader and related tooling for the ASI plugin ecosystem.
