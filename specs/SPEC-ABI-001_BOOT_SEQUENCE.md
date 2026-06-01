# SPEC-ABI-001: DLL Boot Sequence (Minimum Functional Path)

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Define the minimum boot/init behavior required for an ABI-compatible, functionally loadable `samp.dll` replacement.

## Old 0.2x Reference Boot Path (Confirmed)

The legacy source has a complete and explicit boot chain:

1. `DllMain` on `DLL_PROCESS_ATTACH`:
2. `InitSettings()`
3. `SetUnhandledExceptionFilter(...)`
4. archive path build via `GetModuleFileNameA`, load `samp.saa`
5. file system hook install
6. `_beginthread(LaunchMonitor, ...)`
7. `LaunchMonitor`:
8. `new CGame()`
9. `CGame::InitGame()`
10. poll until `ADDR_ENTRY == 7`
11. `CGame::StartGame()`
12. `CGame::InitGame`:
13. `ApplyPreGamePatches()`
14. `CGame::StartGame`:
15. `ApplyInGamePatches()`
16. `GameInstallHooks()`
17. `InitScripting()`
18. graphics hook calls `TheGraphicsLoop()`
19. `TheGraphicsLoop()` calls `DoInitStuff()`
20. `DoInitStuff()` first initializes D3D/UI objects, then instantiates `CNetGame(...)`.

## Current 0.3.7-R5 Binary Boot Path (Mapped)

- Entry RVA/VA: `0x000cbc90` / `0x100cbc90`
- Observed chain:
1. entry wrapper sets runtime guard (`fcn.100ce430`)
2. reason-based dispatch (`PROCESS_ATTACH`, `THREAD_ATTACH`, `THREAD_DETACH`, `PROCESS_DETACH`)
3. secondary runtime/bootstrap dispatch (`fcn.100cbb0f`)
4. main dll dispatch thunk (`fcn.100c5270 -> jmp 0x100c50c0`)
5. `0x100c50c0` contains attach/detach core path:
6. `cmp reason, 1` attach branch
7. `SetUnhandledExceptionFilter(...)`
8. `GetModuleFileNameA(...)`
9. multiple internal startup calls/objects
10. detach-side cleanup call path for `reason == 0`
11. returns through runtime teardown (`fcn.100ce46b`)

## Structural Crosswalk (Legacy -> Current)

1. Legacy `DllMain` (`saco/main.cpp`) maps to current `entry0@0x100cbc90` + `0x100c50c0` dispatch core.
2. Legacy `LaunchMonitor`/`CGame::InitGame`/`CGame::StartGame` maps to current internal startup calls reachable from `0x100c50c0` attach branch.
3. Legacy hook install + graphics/net loop maps to current callbacks and loop function pointers set in attach path.

## Required Reimplementation Behavior

1. Deterministic `fdwReason` dispatch.
2. Process attach phase:
3. runtime/bootstrap guard setup
4. unhandled exception filter setup
5. module-path-based runtime asset/config resolution
6. core subsystem init entry (hook scheduler + network bootstrap stubs)
7. Process detach phase:
8. reverse-order cleanup of workers/resources
9. stable success/failure return semantics matching Windows DLL contract
10. thread attach/detach handlers must exist and be stable (no-op allowed).

## Non-Goals (Current Phase)

1. Full parity for all render/audio/gameplay internals.
2. One-to-one replication of all legacy hook targets.

## Evidence

- `analysis/notes/entry_100cbc90.txt`
- `analysis/notes/fcn_100cbb0f_secondary_dispatch.txt`
- `analysis/notes/fcn_100c5270_entry_dispatch.txt`
- `analysis/notes/fcn_100c50c0_dispatch_core.txt`
- `/tmp/obj_100c50c0.txt`
- Legacy reference checkout:
- `/tmp/samp-0.2x-ref/saco/main.cpp`
- `/tmp/samp-0.2x-ref/saco/game/game.cpp`
- `/tmp/samp-0.2x-ref/saco/game/hooks.cpp`
- `/tmp/samp-0.2x-ref/saco/game/patches.cpp`
- Confirmed working local client reference:
- `/home/chairman/Projects/sa-mp.dll-rebuild/samp/client/main.cpp`
- `/home/chairman/Projects/sa-mp.dll-rebuild/samp/client/game/game.cpp`
- `/home/chairman/Projects/sa-mp.dll-rebuild/samp/client/game/hooks.cpp`
