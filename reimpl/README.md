# `reimpl/` - SA-MP DLL Clean-Room Rebuild

This folder contains Team B implementation artifacts derived from specs.

## Current Scope

- Buildable `samp.dll` skeleton (`DllMain`, no exports).
- Phased boot lifecycle dispatcher (`BOOT_PHASE_1..6`) with rollback/teardown handling.
- Original-style command-line settings parsing (`-d/-c/-h/-p/-n/-z`) in runtime init.
- LaunchMonitor-style worker thread scaffold with entry-gate wait and periodic init tick.
- Dual-stack endpoint parsing and connect core (`SPEC-NET-001` foundation).
- IPv4-era socket primitive compatibility layer (`SPEC-NET-003` foundation).
- TCP bootstrap manager lifecycle scaffold (`SPEC-NET-004` foundation).
- Default integration of vendored Knogle RakNet variant (`SPEC-NET-005` foundation).
- SA-MP client UDP datagram transform for RakNet outbound traffic, validated against original `samp.dll` capture bytes.
- Network object worker-cluster mapping guide (`SPEC-NET-006` foundation).
- Host-side parser tests for deterministic validation.

## Build (Host Tests)

```bash
cmake -S reimpl -B build-host -G Ninja -DSAMPDLL_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Disable vendored RakNet integration if needed:

```bash
cmake -S reimpl -B build-host -G Ninja \
  -DSAMPDLL_BUILD_TESTS=ON \
  -DSAMPDLL_ENABLE_RAKNET_KNOGLE=OFF
```

For Win32 ABI-shaping fallback experiments, the corresponding `OFF` build links
the static socket surface through `WSOCK32` and resolves
`getaddrinfo`/`freeaddrinfo` dynamically at runtime. This keeps the dual-stack
path available without forcing a static `WS2_32.dll` import into the socket-only
candidate.

Variant Win32 builds can also be produced through the wrapper script:

```bash
SAMPDLL_BUILD_DIR=build-win32-socket-fallback \
SAMPDLL_ENABLE_RAKNET_KNOGLE=OFF \
reimpl/scripts/build_win32.sh
```

Note: `test_ipv4_socket_compat` can end as `SKIP` (exit 0) in heavily
sandboxed environments with blocked AF_INET sockets.
Note: `test_tcp_bootstrap_manager` can also end as `SKIP` in heavily sandboxed
environments.

## Build (Windows 32-bit DLL via MinGW)

```bash
cmake -S reimpl -B build-win32 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=reimpl/cmake/toolchains/i686-w64-mingw32.cmake \
  -DSAMPDLL_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-win32 --target samp
i686-w64-mingw32-strip -s build-win32/samp.dll
```

Expected output file:

- `build-win32/samp.dll`

## Build In `devbuild` Toolbox

```bash
reimpl/scripts/build_in_devbuild_toolbox.sh
```

This runs the Win32 build in toolbox container `devbuild` and prints an ABI parity summary.

## Reverse-Engineering Toolbox

For trace capture, PE diffing, Wine probing, and binary-shaping work, use the dedicated toolbox bootstrap:

```bash
toolboxes/reverse-engineering/create.sh
```

Then run stricter PE checks against the packaged DLL:

```bash
tools/check_pe_abi_strict.sh samp.dll build-win32/samp.dll
```

For import-surface experiments on the socket-fallback Win32 artifact, use the
LIEF-based post-link shaper inside the reverse-engineering toolbox:

```bash
toolboxes/reverse-engineering/run.sh \
  bash -lc 'cd /path/to/libsamp && \
    python3 tools/shape_pe_imports.py samp.dll build-win32-socket-fallback/samp.dll build-win32-socket-fallback/samp-shaped.dll'
```

The shaper preserves the original COFF string-table tail used for long section
names (for example `"/4"` -> `.eh_frame`), so the resulting DLL remains
readable by the repository's `objdump`-based ABI checker.

Current measured result for `build-win32-socket-fallback/samp-shaped.dll`:

- imported DLL set now matches the reference set plus one extra `msvcrt.dll`,
- import symbol count improves from `87` to `247`,
- missing import symbols drop from `233` to `107`.

The optional `--extend-existing-libraries` mode is currently experimental and
can over-expand existing `GDI32`/`KERNEL32`/`WSOCK32` groups.

## Runtime Trace Diff Harness

```bash
tools/compare_runtime_traces.sh /path/to/reference.log /path/to/rebuild.log
```

Use this to compare normalized event sequences and event-count deltas between reference and rebuild runs.

## ABI Notes

- Original `samp.dll` has no export directory.
- This rebuild currently keeps that shape (no intentional exports).
- Import-table parity is not yet complete; current focus is network stack modernization and testability.
- Use `tools/check_pe_abi_strict.sh` when working on loader- and import-surface parity; the older parity script is only a coarse first pass.
