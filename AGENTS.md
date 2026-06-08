# AGENTS.md - SA-MP 0.3.7 `samp.dll` RE Short Guide

## Goal

This project develops a compatibility-focused drop-in replacement for the
SA-MP 0.3.7 `samp.dll`.

Modernization is welcome when it is useful, but compatibility with 0.3.7
servers and open.mp servers comes first.

## Working Mode

* Work conservatively and in small, reviewable changes.
* Before implementation, check existing RE documentation, traces, probe logs,
  and relevant references.
* Direct observations from the original 0.3.7 DLL override every other source.
* Mark each assumption as observed, inferred, or still to be verified.
* Do not change public APIs, exports, calling conventions, struct layouts, or
  protocol formats unless explicitly justified.
* If behavior is unclear, prefer a stub plus log plus `TODO_VERIFY` over wrong
  semantics.

## Source Priority

1. **Original SA-MP 0.3.7 `samp.dll`**

   * Exports, imports, ordinals, RVAs, calling conventions, memory layouts,
     runtime behavior.

2. **ASI probe / golden traces**

   * Memory contents, game states, hook events, network/RPC observations.

3. **Internal project documentation and current implementation**

   * `docs/re/`, `docs/traces/`, `symbols/`, `tests/`, existing headers/code.

4. **open.mp**

   * API/semantic/compatibility reference:
   * https://github.com/openmultiplayer/open.mp
   * https://open.mp/docs

5. **gta-reversed**

   * GTA-SA engine reference for classes, hooks, pools, render/ped/vehicle/camera:
   * https://github.com/gta-reversed/gta-reversed

6. **Ultimate ASI Loader**

   * Reference for ASI loading, plugin environment, and debugging:
   * https://github.com/ThirteenAG/Ultimate-ASI-Loader/tree/master

7. **SAMPFUNCS**

   * SAMP functions, modding references, and SDK guides. Use only as a
     compatibility/modding reference, not as proof of original-DLL behavior.

8. **MTA:SA**

   * Low priority, conceptual reference only:
   * https://github.com/multitheftauto/mtasa-blue

## Evidence Tags

Use these tags in comments, documentation, or commit context:

* `OBSERVED_037` - directly observed with the original 0.3.7 DLL.
* `PROBE_TRACE` - backed by ASI probe/runtime log evidence.
* `STATIC_037` - statically analyzed from the original DLL.
* `OPENMP_REF` - derived from open.mp.
* `GTA_REVERSED_REF` - derived from gta-reversed.
* `MTA_REF` - derived from MTA, low priority only.
* `INFERRED` - plausible but unproven assumption.
* `TODO_VERIFY` - must still be validated against 0.3.7.

Example:

```cpp
// OBSERVED_037 + PROBE_TRACE:
// samp.dll+0x00123456, SHA256=<hash>, run=<trace-name>
// Called after local player spawn, before first chat render.
```

## ABI And Layout Rules

* Always document RVAs instead of absolute addresses: `samp.dll+0x...`.
* Record hash/build/version for important claims.
* Guard binary-relevant structs with `sizeof` and `offsetof` `static_assert`s.
* Keep calling conventions explicit.
* Do not leak exceptions across DLL/C/WinAPI boundaries.
* Check return values from allocations, WinAPI, file I/O, and networking calls.

## ASI Probe And Traces

* Probe logs are primary evidence.
* Keep original-DLL and replacement-DLL runs comparable.
* Normalize pointers, module bases, timestamps, and random values before diffing.
* Document important scenarios as golden traces:

  * Load/Init
  * Connect/Handshake
  * Spawn
  * Chat/Dialog/TextDraw
  * Vehicle/Ped/Object State
  * Disconnect/Shutdown

## Hooking And Loader

* Keep `DllMain` minimal.
* Install hooks only when target bytes/version are validated.
* Hook documentation must include:

  * Module + RVA
  * Original bytes
  * Patch bytes
  * Patch length
  * Restore path

* Wrap `VirtualProtect`, instruction-cache flush, and restore cleanly.
* Do not blindly overwrite third-party hooks.

## Netcode And Protocol

* Validate packet/RPC IDs, bitstream layouts, string encoding, limits, and order
  against 0.3.7 traces.
* Never trust server data.
* Bounds-check every read.
* Use open.mp as the semantic reference, but 0.3.7 observations decide conflicts.

## GTA-SA Integration

* Document GTA-SA addresses with build/hash/RVA.
* Use gta-reversed as an engine reference, but do not copy assumptions blindly.
* Access game state defensively: loading screen, pause, disconnect, respawn,
  null pointers, and world changes.

## Avoid

* No cheats, anti-cheat bypasses, ban evasion, or public-server abuse.
* No proprietary binaries or assets in the repository.
* No large pseudocode blocks from proprietary disassembly.
* No mass formatting.
* No unnecessary dependencies.
* No speculative offsets presented as facts.
* Do not change compatibility behavior without a compatibility mode or rationale.

## Completion Format

After changes, report briefly:

```md
## Result
- Changed: ...
- Evidence: OBSERVED_037 / PROBE_TRACE / ...
- Tests/Checks: ...
- Compatibility risks: ...
- Open points: ...
```

If nothing was tested, say so clearly.
