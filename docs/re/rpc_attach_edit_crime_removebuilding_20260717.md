# RPC 43/75/112/113/116/117 compatibility pass

Date: 2026-07-17

Reference binary:

- SA-MP 0.3.7 R5 `samp.dll`
- SHA256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Ghidra export: `analysis/generated/ghidra_samp_20260609/summary.md` and `functions.tsv`; the export records all focused function boundaries below for this exact hash (`x86:LE:32:default`, Windows compiler spec).

## Static evidence

| Surface | R5 location | Observed behavior |
| --- | --- | --- |
| RPC 43 RemoveBuildingForPlayer | `samp.dll+0x1D530` | Reads model, position and radius, then enters the removal path. |
| Removal scan/store | `samp.dll+0x9D180`, `+0x9D020`, `+0x9CFF0`, `+0x9D3D0` | Scans building, dummy and object pools; matching entities get `entity+0x2F = 1` and their Z lowered by 2000. Removal data is retained for later application. |
| RPC 75 AttachObjectToPlayer | `samp.dll+0x1C6A0` | Resolves the target actor and uses GTA object-to-char attachment semantics. |
| RPC 112 PlayCrimeReport | `samp.dll+0x19050` | Reads the 30-byte suspect/vehicle/crime/position payload and calls the scanner helper. |
| Crime scanner helper | `samp.dll+0xA1790` | Resolves `ped+0x480 -> player data -> wanted+0x21C`, patches GTA `0x4E7529` for 39 bytes and calls `CAEPoliceScannerAudioEntity::AddAudioEvent` at GTA `0x4E71E0` with event `0xA4`. Crime IDs outside 2..22 do not produce a report. |
| RPC 113 SetPlayerAttachedObject | `samp.dll+0x18F00` | Validates slots 0..9 and bones 1..18, stores the 52-byte attachment record and invokes the player-ped wrapper. |
| R5 attached-object wrapper | `samp.dll+0xB0B10` | Stores ten 52-byte records and allocates a dedicated `0x119C`-byte SA-MP object wrapper per active slot. This is more than GTA opcode 069B root attachment. |
| RPC 116 EditAttachedObject | `samp.dll+0x117E0` | Reads the attachment slot and enters the original editor at `samp.dll+0x724E0`, which selects editor mode 2 and creates the GUI controls. |
| RPC 117 EditObject | `samp.dll+0x118A0` | Reads the bit-packed player-object flag/object ID and enters the original editor at `samp.dll+0x72420`, which selects editor mode 1 and creates the GUI controls. |

Evidence: `STATIC_037`. GTA scanner type/address/signature are corroborated by gta-reversed (`GTA_REVERSED_REF`). RPC layouts and response layouts are corroborated by open.mp (`OPENMP_REF`).

## Current replacement

- RPC 43 is bounds-checked, recorded in the adapter ring and applied to all three GTA pools. Matrix-less static buildings use their `CSimpleTransform`; rules are retained and rescanned every 750 ms so later streaming cannot undo the removal.
- RPC 112 is decoded exactly. The local wanted police scanner now receives GTA audio event `0xA4`; the GTA patch is applied only when the 1.0-US bytes match and is restored after the call.
- RPC 75 resolves local/remote actors and uses GTA opcode `069B`.
- RPC 113 owns ten slots per player, validates payloads, loads safe models, creates/removes GTA objects and retries while actors/models are not streamed.
- RPC 116/117 open a visible mouse/keyboard editor overlay. Move/rotate/scale (attached only), axis selection, incremental update, Save/Enter, and Cancel/Escape send the matching wire response.
- RPC 28 closes an active editor without inventing a response.

## Verification state

Project acceptance decision, 2026-07-17: these four RPC areas are **I.O.** for
the current compatibility scope.  The known differences below are retained as
non-blocking parity notes; this decision does not reclassify them as exact R5
visual or audio behavior.

| Area | Compatibility status | Known non-blocking parity notes |
| --- | --- | --- |
| RemoveBuildingForPlayer | **I.O. / accepted** | A focused R5 comparison for stream-in/reconnect edge cases would provide stronger visual evidence. |
| PlayCrimeReport | **I.O. / accepted** | Audible phrasing and the remote-suspect temporary vehicle description are not trace-matched to R5. |
| Set/RemovePlayerAttachedObject | **I.O. / accepted** | The compatibility renderer does not reproduce the full R5 bone-matrix path, non-uniform scale and both material colours exactly. |
| EditObject/EditAttachedObject | **I.O. / accepted** | The compatibility overlay is not the original 3D gizmo/camera-control and drag implementation. |
| Combined accepted scope | **100% accepted** | Wire handling and the compatibility runtime are complete for the agreed scope; exact presentation parity remains outside this acceptance gate. |

`PROBE_TRACE`: the replacement attachment run completed dialog 13501, the
fragmented 2518-character dialog 13503, dialog 13504, RPC 113, RPC 116, the
edit response and subsequent chat/sync traffic without `ID_MODIFIED_PACKET` or
disconnect.  RPC 117 remains available as a focused regression route, but is
not a blocker for this acceptance decision.

## Repro route

The bare open.mp fixture provides:

1. Join at Area 51 and inspect the removed vanilla buildings.
2. `/rpcpattach` to create the helmet attachment.
3. `/rpcpattachedit`, then exercise Move/Rotate/Scale and Save or Escape.
4. `/rpcpattachoff` to remove the slot.
5. `/rpccrime` to send documented crime ID 16; ID 1 was an invalid earlier fixture and is intentionally no longer used.

Expected logs include `remove_building ... persisted=1`, `crime_report ... played=1`, `edit_state: begin`, `rpc-user-out id=116/117`, and the server-side `OnPlayerEdit*` callback lines.
