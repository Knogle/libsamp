# libsamp / Libre-SAMP - Public Working Folder

This folder is the clean entry point for preparing the public repository. It
must not contain proprietary binaries, private local paths, or references to
non-public source trees.

## Project Goal

libsamp, written out as Libre-SAMP, develops a compatibility-focused drop-in
replacement for the SA-MP 0.3.7 `samp.dll` that works with 0.3.7-compatible
servers and open.mp.

For now, the normal SA-MP installer is still required to provide the local
client environment, assets, and companion files. This repository does not ship
proprietary client or GTA-SA files. The concrete project target is the
replaceable `samp.dll` for an existing local client installation.

The current implementation is based on reverse engineering of the original
0.3.7 DLL, golden traces, ASI probe logs, open.mp protocol semantics,
gta-reversed engine references, and project-owned runtime observations.

## Public Working Mode

- Keep changes small and reviewable.
- Keep ABI, exports, calling conventions, struct layouts, and RPC layouts
  stable.
- Mark unclear behavior as a stub with logging instead of guessing semantics.
- Use evidence tags for technical claims that are not directly obvious from the
  code.
- Do not commit proprietary assets or binaries. Project-generated assets must
  be documented in `NOTICE.md`.

## Important Files

- `TASK_TRACKER.md`: Full task tracker from the current playable milestone.
- `PUBLICATION_CHECKLIST.md`: Checklist for the first public repository state.
- `RE_EVIDENCE_GUIDE.md`: Evidence and trace rules for future work.
- `../LICENSE`: MIT license for project-owned code and generated assets.
- `../NOTICE.md`: Third-party and asset provenance.
- `../SECURITY.md`: Security policy and usage boundaries.
- `../CONTRIBUTING.md`: Contribution rules and build notes.

## Current Milestone

The client now reaches a basic playable state:

- Connect/join/spawn against an open.mp-compatible server.
- Chat send and receive.
- In-game dialogs with mouse mode.
- TAB player list as a SA-MP overlay instead of single-player stats.
- Server-created vehicles are visible and drivable.
- Spawn/teleport streaming lands at plausible positions.
- World time and the basic HUD are server-influenced.
- TextDraws are visible, including transparency improvements.
- The custom object pipeline has started, but is not complete yet.

The most important remaining topics are remote player sync, the object/material
pipeline, full RPC coverage, TextDraw parity, disconnect/resync stability, and
systematic golden-trace comparisons.
