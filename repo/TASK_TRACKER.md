# Task Tracker

Status date: 2026-06-08

This tracker describes the project state from the current playable milestone.
Priorities:

- `P0`: Blocks core playability or public repository quality.
- `P1`: Important for compatibility and visible parity.
- `P2`: Stabilization, tests, and quality.
- `P3`: Convenience, documentation, and cleanup.

States:

- `[todo]`: Not started.
- `[doing]`: In progress.
- `[blocked]`: Blocked by missing trace/source evidence.
- `[verify]`: Implemented, needs golden-trace or gameplay verification.
- `[done]`: Finished and tested.

## 1. Public Repo Hygiene

- [done] `P0` Align project name and goal around `libsamp` / `Libre-SAMP` as a
  `samp.dll` drop-in replacement.
- [done] `P0` Remove old crosswalk/planning files that are not public-ready.
- [done] `P0` Remove local reference source names from public naming.
- [done] `P0` Reframe documentation around original-DLL reverse engineering,
  golden traces, and public references.
- [done] `P0` Check the index: no proprietary binaries, no local working assets,
  no private prefix copies.
- [done] `P0` Finalize license files and third-party notices.
- [done] `P1` Write README for the first public state.
- [done] `P1` GitHub Actions builds public-ready Win32 artifacts for `samp.dll`
  and the ASI probe with the vendored Knogle/RakNet transport path.
- [todo] `P1` Document the build/runbook for the devbuild toolbox and prefix
  deployment.
- [verify] `P1` Verify network-enabled CI builds against golden-trace scenarios
  once public-ready fixtures exist.
- [todo] `P2` Build or document a trace sanitizer for logs.

## 2. ABI / DLL Surface

- [verify] `P0` Check exports, ordinals, and PE profile against original-DLL
  analysis. Evidence: `STATIC_037`.
- [verify] `P0` Keep DllMain minimal and do not leak exceptions across DLL
  boundaries.
- [todo] `P0` Extend the ABI tracker with current runtime hooks, env flags, and
  build artifacts.
- [todo] `P1` Add `static_assert`s for binary-relevant structs:
  RPC payloads, TextDraw transmit structs, player/vehicle sync structs.
- [todo] `P1` Maintain hook documentation for each patch: module/RVA, original
  bytes, patch bytes, patch length, restore path.

## 3. Boot / Lifecycle

- [done] `P0` Implement pre-connect panorama, text output, and 10s delay.
  Evidence: `PROBE_TRACE`, `OBSERVED_037`.
- [done] `P0` ESC/Pause during pre-connect pauses progression.
- [verify] `P0` Extend the device-selection resolution list, including
  2560x1440.
- [verify] `P1` Integrate loading screen through the `LOADSCS.txd` path.
- [todo] `P1` Unify shutdown/disconnect paths: `/q`, `/quit`,
  server disconnect, game close.
- [todo] `P1` Reset reconnect state cleanly: RPC tables, local player,
  vehicles, textdraws, labels, gangzones, objects, timers.

## 4. Netcode / RPC Coverage

- [done] `P0` RakNet base connect, join, chat, and dialog responses work.
  Evidence: `OPENMP_REF`, `PROBE_TRACE`.
- [verify] `P0` RPC decoder inventory is broad enough to capture unknown RPCs
  as logged stubs.
- [todo] `P0` Register all server-triggered RPCs locally as safe bounds-checked
  stubs.
- [todo] `P0` Unknown-RPC policy: one chat notice, detailed local log, no crash.
- [todo] `P1` Bitstream fixtures for critical RPCs:
  `SetSpawnInfo`, `RequestSpawn`, `Spawn`, `SetPlayerPos`,
  `PutPlayerInVehicle`, `CreateVehicle`, `ShowTextDraw`, `ShowDialog`,
  `CreateObject`, `SetObjectMaterial`, `SetPlayerTime`.
- [todo] `P1` Check client-to-server sync: on-foot, in-vehicle, passenger, aim,
  damage, death, request class, request spawn.
- [todo] `P1` Investigate disconnect/timeout: keepalive, ack cadence,
  player-sync cadence, vehicle-sync cadence.
- [todo] `P2` Fuzz/bounds tests for RPC readers without networking.

## 5. Spawn / Player State

- [done] `P0` Spawn at the correct position and teleport with
  `RefreshStreamingAt`/`RestartIfWastedAt`.
- [done] `P0` Camera and mouse control work after spawn.
- [verify] `P0` Death/respawn no longer leaves a detached state.
- [todo] `P0` Make remote-player sync visible: create, skin/model, position,
  movement, vehicle enter/exit.
- [todo] `P0` Add player blips for remote players on radar and map.
- [todo] `P1` Sync player score/ping/name/color/team for TAB list and blips.
- [todo] `P1` Fully verify `SetPlayerArmour`, `SetPlayerArmedWeapon`,
  `ResetPlayerWeapons`, `ResetPlayerMoney`.
- [todo] `P1` Map `ApplyAnimation` correctly onto local and remote peds.
- [todo] `P2` Check ped audio, CJ voices, fight style, special actions, and
  armed-weapon edge cases.

## 6. Vehicles

- [done] `P0` Server vehicles are created, loaded, visible, and drivable.
- [done] `P0` Vehicle streaming is stabilized, with no direct crashes under many
  vehicles.
- [verify] `P0` Vehicle blips: empty vehicles should be visible, but color and
  alpha still need to be correct.
- [todo] `P0` Sync remote players in vehicles.
- [todo] `P1` Apply vehicle health, color, siren, paintjob, components, and
  numberplate.
- [todo] `P1` Map lock state, doors, engine, lights, and objective marker
  correctly.
- [todo] `P1` Remove vehicle respawn/despawn cleanly from GTA pools.
- [todo] `P2` Document streaming budget and LOD strategy.

## 7. Objects / Custom Assets

- [doing] `P0` Create normal GTA-SA objects through the GTA pipeline.
- [doing] `P0` Log unsupported custom models instead of crashing.
- [todo] `P0` Finalize the SA-MP custom object asset pipeline:
  IMG/TXD/DFF lookup, model ID mapping, load/unload, material binding.
- [todo] `P0` Defensively implement `CreateObject`, `DestroyObject`,
  `MoveObject`, `StopObject`, `AttachObjectToPlayer`,
  `AttachObjectToVehicle`.
- [todo] `P1` Implement `SetObjectMaterial` and `SetObjectMaterialText` with
  alpha, font, alignment, and texture-dictionary support.
- [todo] `P1` Check object streaming radius and prioritization against golden
  traces.
- [todo] `P2` Add crash fixtures for unsupported model IDs and missing assets.

## 8. TextDraw Renderer

- [verify] `P0` Basic rendering with text, colors, shadow, outline, and
  transparency is visible.
- [doing] `P0` Render empty box/sprite TextDraws correctly.
- [todo] `P0` Clickable TextDraws: hover, cursor, SelectTextDraw color, click RPC
  to server.
- [todo] `P1` GTA CFont parity: spacing, proportional fonts, underscores, color
  tokens, line height, left/center/right alignment.
- [todo] `P1` Sprite/model-preview TextDraws: texture name lookup, model
  loading, camera rotation, zoom.
- [todo] `P1` Finalize resolution-scaled coordinates:
  640x448/480 logical space, widescreen, 2560x1440.
- [todo] `P2` Golden-trace screenshot comparisons for TextDraw scenes.

## 9. Chat / Input

- [done] `P0` Chat receive and send works.
- [verify] `P0` `/q` and `/quit` are wired.
- [todo] `P1` Finalize chat colors and embedded color tokens.
- [todo] `P1` Chat input box: 80% transparent black box from `>` to the dynamic
  input end, keeping the 1px vertical correction.
- [todo] `P1` Fix command/text spacing: spaces after commands and between
  TextDraw words must not be lost.
- [todo] `P1` Chat history, cursor, backspace, paste, Enter/Escape, max length,
  non-ASCII handling.
- [todo] `P2` Optional debug-only chat dump for server-side echo tests.

## 10. Dialogs / Menus / Mouse Mode

- [done] `P0` In-game dialogs replace Win32 dialogs in normal mode.
- [done] `P0` Win32 dialogs are reserved for explicit debug mode.
- [verify] `P0` Mouse mode for dialogs and TextDraw selection.
- [todo] `P1` Complete dialog styles:
  message box, input, password, list, tablist, headers.
- [todo] `P1` Dialog layout: scrollbar, selection, OK/Cancel, text wrapping,
  color tags.
- [todo] `P2` Dialog input validation and SendDialogResponse fixtures.

## 11. HUD / Frontend / Pause

- [done] `P0` TAB shows the SA-MP player list instead of single-player stats.
- [done] `P0` ESC/Pause stays open and is not immediately closed.
- [verify] `P1` Scoreboard shows local player plus remote players with
  score/ping.
- [todo] `P1` Scoreboard sort/order, colors, ping updates, max rows, scrollbar.
- [todo] `P1` HUD visibility RPCs: radar, clock, money, health/armour, wanted,
  zone names.
- [todo] `P2` Further align F8 screenshots: filenames, chat notice, error cases.

## 12. World / Visual State

- [done] `P0` Motion blur/bloom while driving fast is disabled.
- [done] `P0` World time is applied from the server.
- [verify] `P0` Weather/time is not rolled back by the local single-player
  timer.
- [todo] `P1` Stabilize world streaming during fast driving and teleports:
  no missing blocks, no HUD dropouts.
- [todo] `P1` Implement `RemoveBuildingForPlayer` and test against server-side
  map scenes.
- [verify] `P1` GangZones: RPC 108/120/121/85 pool, 1024-slot recovery
  snapshot, shared 500 ms flashing, radar/pause-map call hooks, lifecycle
  reset, and four-zone bare fixture. Evidence: `STATIC_037`, `OPENMP_REF`;
  replacement visual run remains `TODO_VERIFY`.
- [todo] `P2` Camera RPCs: interpolate, attach, spectate, reset.

## 13. Audio

- [verify] `P1` `PlayerPlaySound`/`PlaySound` implemented and checked against a
  run.
- [todo] `P1` `PlayAudioStreamForPlayer` and `StopAudioStreamForPlayer`.
- [todo] `P2` Audio edge cases: stop on disconnect, stop on spawn reset,
  duplicate stream handling, volume/range.

## 14. 3D Text / Labels / Pickups / Checkpoints

- [todo] `P1` `Create3DTextLabel`, attach to player/vehicle, update, delete.
- [todo] `P1` Pickups: create/destroy, model loading, pickup notification.
- [todo] `P1` Checkpoints and race checkpoints: show/hide, size, target marker.
- [todo] `P2` Label distance culling and LOS/behind-camera behavior.

## 15. Remote Players

- [doing] `P0` Remote player create/show path.
- [todo] `P0` Remote on-foot sync: position, rotation, velocity, animation,
  weapon, keys.
- [todo] `P0` Remote in-vehicle sync: driver/passenger, seat, vehicle transform,
  health.
- [todo] `P1` Remote aim/damage/death events.
- [todo] `P1` Remote name tags and health bars.
- [todo] `P1` Remote player colors in chat, TAB, blips, and nametags.
- [todo] `P2` Interpolation/lag smoothing instead of hard teleports.

## 16. Server Testbed

- [todo] `P0` Version bare open.mp testserver scenarios:
  spawn, teleport, vehicles, TextDraw, dialogs, remote player, object stress.
- [todo] `P1` Scripted scenario runner: client starts, server sends defined RPC
  sequences, logs are normalized.
- [todo] `P1` Two-client scenario for remote-player sync.
- [todo] `P2` Long-run soak test: 30 min drive, teleports, death, reconnect.

## 17. Debugging / Tooling

- [done] `P0` ASI probe and runtime logs are usable for golden traces.
- [todo] `P1` Extend the probe with remote-player/vehicle/object pool snapshots.
- [todo] `P1` Stabilize packet/RPC trace diff tooling.
- [todo] `P1` Document short commands for log clear/build/deploy/run.
- [todo] `P2` Generate symbol/RVA maps automatically from build and analysis.

## 18. Security And Compatibility Boundaries

- [todo] `P0` Do not document or implement cheat, anti-cheat bypass, or public
  server abuse features.
- [todo] `P0` Always bounds-check server data.
- [todo] `P1` Never crash on unknown or oversized payloads.
- [todo] `P1` Keep feature flags for risky hooks.
- [todo] `P2` Compatibility notes for known unsupported server features.

## Next Sensible Work Order

1. `P0` Make remote-player sync visible, including blips.
2. `P0` Make unknown-RPC stubs complete and crash-free.
3. `P0` Stabilize the custom object pipeline, first GTA objects, then SA-MP
   custom assets.
4. `P1` TextDraw click/selection and model preview.
5. `P1` Scoreboard with local player, remote players, ping/score.
6. `P1` Reproduce long-run disconnect/resync systematically.

## Current Open Risks

- `INFERRED`: Some runtime hooks are reconstructed from plausible original-DLL
  traces, but are not fully validated across multiple scenarios yet.
- `TODO_VERIFY`: Custom object materials and model previews may still have
  wrong asset or alpha semantics.
- `TODO_VERIFY`: Remote player sync is not proven fully playable yet.
- `TODO_VERIFY`: High vehicle/object density may still trigger streaming or
  pool pressure.
- `TODO_VERIFY`: TextDraw coordinates and CFont spacing are visibly improved,
  but not 1:1 yet.
