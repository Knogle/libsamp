# ABI Tracker

## Target

- Input reference: `path/to/original/samp.dll`
- Goal: clean-room replacement with compatible load/runtime behavior.

## Current Status

1. Build system for Win32 DLL is in place.
2. DLL name/shape target is `samp.dll` with no intentional exports.
3. Dual-stack network foundation implemented (`SPEC-NET-001`, `SPEC-NET-002`).
4. Host-side parser tests are passing.
5. `devbuild` toolbox build is working (`build-win32/samp.dll` generated).
6. Candidate currently matches image base/subsystem/no-exports and differs in sections/import set.
7. Entry boot-path analysis documented (`SPEC-ABI-001`).
8. Original-DLL boot/runtime analysis documented (`SPEC-ABI-001`, `analysis/metadata/CURRENT_DLL_DETAILED_MAP.md`).
9. Phased boot runtime scaffold implemented (`BOOT_PHASE_1..6` attach/detach dispatch).
10. Boot sequence cross-checked against original 0.3.7 runtime traces and the current replacement path.
11. Runtime now includes settings parser + LaunchMonitor-thread + entry-gate wait + init-tick flow.
12. IPv4 primitive net layer implemented (`SPEC-NET-003`) with host tests (UDP bind/send/recv + TCP listen/connect smoke).
13. TCP bootstrap manager lifecycle scaffold implemented (`SPEC-NET-004`) with start/stop/connect/idempotency tests.
14. Knogle RakNet variant integrated as vendored optional backend with bridge smoke test (`SPEC-NET-005`).
15. TCP bootstrap manager now provisions optional RakNet client lifecycle via adapter (`create/connect/disconnect/pump` APIs).
16. IPv4 compatibility module now includes mapped local-address enumeration helper (`gethostname/gethostbyname/inet_ntoa`) aligned to `fcn.10053b70`.
17. Network object lifecycle/connect-worker cluster formalized as `SPEC-NET-006`.
18. Scope decision: `d3d9`/`d3dx9_25` and `BASS.dll` are treated as external runtime dependencies for now; primary compatibility target is `samp.dll` lifecycle + networking/state behavior.
19. Candidate PE now matches reference `FileAlignment` (`0x1000`) and carries a `.rsrc` section again.
20. Win32 socket-fallback compatibility experiment with `SAMPDLL_ENABLE_RAKNET_KNOGLE=OFF` removes `libwinpthread-1.dll`, proving that dependency is packaging-optional rather than fundamental.
21. The same experiment shows `.eh_frame` and `.tls` persist even without the C++ RakNet path, so those deltas come from the MinGW/CRT path and need a separate packaging strategy.
22. The socket-fallback Win32 build now links its static socket surface through `WSOCK32.DLL`; `getaddrinfo`/`freeaddrinfo` are resolved dynamically from `ws2_32.dll` at runtime so the fallback candidate no longer needs a static `WS2_32.dll` import.
23. A LIEF-based post-link import shaper now exists (`tools/shape_pe_imports.py`) and can append missing reference DLL families to the socket-fallback artifact while preserving the existing runtime-facing import groups.
24. The shaper now also preserves/regrafts the COFF string-table tail needed for the long `"/4"` section-name indirection, so the shaped output remains readable by `objdump`.
25. Current strict-check result for `build-win32-socket-fallback/samp-shaped.dll`: DLL-family mismatch is reduced to one extra `msvcrt.dll`, import-symbol count rises from `87` to `247`, and missing import symbols drop to `107`.
26. The experimental `--extend-existing-libraries` mode is currently not a win: it over-expands existing import groups and regresses the strict import-symbol totals.

## Pending ABI Work

1. Build and inspect candidate PE headers against baseline.
2. Reduce section delta (`.eh_frame`, `.bss`, `.idata`, `.tls`) and add resource section parity.
3. Recreate required imported DLL surface while keeping dual-stack internals.
4. Extend LaunchMonitor scaffold with concrete GTA state probes and hook-install call chain parity.
5. Gradually map and reimplement subsystem entry flows (network, audio, UI, rendering hooks).
6. Add behavior-comparison harness against original binary.
7. Extend TCP bootstrap manager from scaffold to full worker/thread + container parity.
8. Move bootstrap manager transport path from socket helper-only mode to RakNet-backed runtime mode.
9. Wire runtime boot/connect state machine to manager RakNet path and map packet/event transitions.
10. Implement `SPEC-NET-006` worker lifecycle internals (connect/listen queue handoff + deterministic stop ordering).
11. Keep external renderer/audio parity non-blocking while `samp.dll` ABI/runtime parity is still incomplete.
12. Eliminate remaining MinGW/CRT shape deltas (`.eh_frame`, `.tls`, `msvcrt.dll`) independently from the RakNet choice.
13. Recreate reference import surface (`USER32`, `COMCTL32`, `ADVAPI32`, `WSOCK32`, `WINMM`, `PSAPI`, `SHELL32`, `d3dx9_25`, `BASS`) via compatibility shims or direct subsystem reintegration.

## Validation Commands

```bash
reimpl/scripts/build_win32.sh
tools/check_pe_abi_parity.sh samp.dll build-win32/samp.dll
tools/check_pe_abi_strict.sh samp.dll build-win32/samp.dll
reimpl/scripts/build_in_devbuild_toolbox.sh
toolboxes/reverse-engineering/create.sh
```
