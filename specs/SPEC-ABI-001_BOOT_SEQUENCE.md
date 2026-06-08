# SPEC-ABI-001: DLL Boot Sequence (Minimum Functional Path)

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Define the minimum boot/init behavior required for an ABI-compatible, functionally loadable `samp.dll` replacement.

## Original 0.3.7-R5 Boot Path (Recovered)

The current minimum path is derived from static original-DLL analysis plus ASI
probe/golden-trace observations:

1. `DllMain`/entry wrapper receives `DLL_PROCESS_ATTACH`.
2. CRT/runtime guard and reason dispatch run through `entry0`, `fcn.100cbb0f`
   and `fcn.100c50c0`.
3. `SetUnhandledExceptionFilter(...)` is installed.
4. module-path-based asset/config resolution runs via `GetModuleFileNameA(...)`.
5. archive/font/config bootstrap objects are initialized.
6. a long-lived launch monitor owns the GTA game-state transition.
7. the monitor waits for the GTA entry state where online bootstrap is safe.
8. pre-game patches and game-state clamps are applied.
9. runtime/game hooks are installed after target bytes are validated.
10. D3D/UI objects are initialized from the render path.
11. the network/session object is created and connect-state banners begin.

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

## Structural Crosswalk (Recovered Current Path)

1. `entry0@0x100cbc90` + `0x100c50c0` form the current attach/detach dispatch core.
2. Internal startup calls reachable from `0x100c50c0` feed the launch monitor,
   game-state gate and startup UI setup.
3. Hook install + graphics/net loop ownership is recovered from original-DLL
   call-site analysis and replacement-vs-original traces.

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
- `docs/traces/`
- `analysis/generated/original_trace_20260602_2115_after_freeroam_spawn/`
