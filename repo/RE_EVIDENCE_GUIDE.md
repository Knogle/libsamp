# RE Evidence Guide

These rules keep new implementation work traceable and suitable for a public
repository.

## Evidence Tags

- `OBSERVED_037`: Directly observed with the original SA-MP 0.3.7 DLL.
- `PROBE_TRACE`: Backed by the ASI probe, a golden trace, or a runtime log.
- `STATIC_037`: Statically analyzed from the original 0.3.7 DLL.
- `OPENMP_REF`: Derived from the open.mp protocol, documentation, or server
  behavior.
- `GTA_REVERSED_REF`: Derived from gta-reversed for GTA-SA engine integration.
- `SAMPFUNCS_REF`: Derived from SAMPFUNCS/modding references for API or hook
  behavior.
- `INFERRED`: Plausible, but not proven yet.
- `TODO_VERIFY`: Must be validated against the original DLL or a golden trace.

## Which Tag To Use

- ABI, struct layouts, RVAs, calling conventions:
  `STATIC_037` or `OBSERVED_037`.
- Runtime order, game state, hooks:
  `PROBE_TRACE` plus optional `OBSERVED_037`.
- RPC semantics and server responses:
  `OPENMP_REF` plus the local trace ID.
- GTA pools, CFont, CCamera, CPed, CVehicle:
  `GTA_REVERSED_REF`, applied defensively.
- Heuristics and fallbacks:
  `INFERRED` and `TODO_VERIFY`.

## Trace Rules

- Keep original-DLL runs and replacement-DLL runs comparable.
- Normalize timestamps, pointers, module bases, and random values before diffing.
- Store key scenarios separately:
  - Load/Init
  - Connect/Handshake
  - Spawn/Respawn
  - Chat/Dialog/TextDraw
  - Vehicle State
  - Remote Player State
  - Object/Material State
  - Disconnect/Shutdown
- Only commit normalized excerpts into documentation.

## Comment Example

```c
// OBSERVED_037 + PROBE_TRACE:
// Original 0.3.7-R5 applies the local spawn camera reset after server spawn
// acceptance and before the first steady player sync. Keep this order because
// earlier RefreshStreamingAt calls can leave the client near stale world cells.
```

## Do Not Commit

- Proprietary DLLs, EXEs, TXDs, IMG files, or extracted assets.
- Raw dumps with private prefix paths.
- Long proprietary disassembly or pseudocode blocks.
- Speculative offsets without `TODO_VERIFY`.
