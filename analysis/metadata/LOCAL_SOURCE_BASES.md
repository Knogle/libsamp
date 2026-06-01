# Local SA-MP Source Bases Available In `~/Projects`

Date: 2026-03-09

Purpose: record the local source trees that materially strengthen reverse analysis and direct reconstruction of the target `samp.dll`.

## Summary

Beyond the source trees already vendored inside this repository, this machine also has another broad SA-MP client/server code base:

1. `/home/chairman/Projects/sa-mp.dll-rebuild/samp`
2. `/home/chairman/Projects/sa-mp-legacy-rebuild/samp`

These are not the target binary, but they are high-value reconstruction bases because they preserve large parts of the client/runtime architecture, hook flow, RakNet usage, UI flow, and game bootstrap.

## Evidence

### Workspace Tree

Path:

- `/home/chairman/Projects/sa-mp.dll-rebuild/samp`

Signals:

1. `README.md` states: source code of SA-MP Client 0.2.5 worked by zyronix.
2. The same README states the network was updated to work with SA-MP 0.3.7.
3. `client/net/netgame.cpp` still declares `NETGAME_VERSION 7776 // 0.2.5`.
4. `server/main.h` declares `SAMP_VERSION "0.3.7"`.

Interpretation:

This tree is a hybrid structural base: old client architecture with later network adaptations. It is useful for crosswalking subsystem boundaries and for identifying likely surviving call chains in the modern binary.

Observed local fact:

1. `samp/client/main.cpp` is byte-identical to `/home/chairman/Projects/sa-mp-legacy-rebuild/samp/client/main.cpp`.
2. `samp/client/net/netgame.cpp` is byte-identical to `/home/chairman/Projects/sa-mp-legacy-rebuild/samp/client/net/netgame.cpp`.
3. `samp/client/buildinfo.h` is byte-identical to `/home/chairman/Projects/sa-mp-legacy-rebuild/samp/client/buildinfo.h`.

Meaning:

For the currently inspected client core, the external tree is not a divergent branch; it is effectively a duplicate source anchor for the same old client baseline.

### External Legacy Tree

Path:

- `/home/chairman/Projects/sa-mp-legacy-rebuild/samp`

Signals:

1. `client/buildinfo.h` declares `SAMP_VERSION "0.2.5-zyronix"`.
2. `client/main.cpp` contains the classical `DllMain` -> `InitSettings` -> `_beginthread(LaunchMonitor, ...)` startup pattern.
3. `client/net/netgame.cpp` preserves the old `CNetGame` construction, pool allocation, RPC registration, connect timestamping, and state-machine structure.
4. The tree includes `saco.rc`, archive helpers, D3D hooks, GUI code, RakNet, and the old `samp_a.map`.

Interpretation:

This tree is the cleaner 0.2.5-era structural baseline and should be treated as a first-class reference base for:

1. bootstrap and launch-monitor behavior,
2. D3D/UI/render hook chains,
3. archive/resource expectations,
4. `CNetGame` lifecycle and reconnect flow,
5. RakNet wrapper and helper layering.

## Practical Use In This Project

Current mode is compatibility-first, so these trees may be used directly to accelerate reconstruction:

1. derive subsystem ownership,
2. derive likely call-order hypotheses,
3. locate old names for anonymous modern-binary clusters,
4. reconstruct old code paths directly when they plausibly survive in the target lineage,
5. generate specs and packaging requirements from those recovered paths.

Still, binary validation remains mandatory:

1. the target remains the current reference `samp.dll`,
2. any hypothesis from these trees must be checked against current-binary evidence,
3. a source match is useful evidence, not final proof of ABI parity.

## Immediate RE Targets Strengthened By These Trees

1. `DllMain` / launch monitor / startup gating
2. `CNetGame` wait-connect-connect-connected transitions
3. D3D device hook and GUI/render loops
4. archive and file hook chains
5. resource and `.rc` expectations
