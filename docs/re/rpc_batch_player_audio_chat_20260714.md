# RPC batch: player pool, audio, chat and object attachment (2026-07-14)

## Original binary

- Build: SA-MP 0.3.7-R5
- File: `analysis/metadata/installer_extract/samp.dll`
- SHA256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Static-analysis inventory: `analysis/generated/ghidra_samp_20260609/functions.tsv`

All addresses below are RVAs relative to `samp.dll`.

## Evidence and implementation status

| RPC | Name | Original handler | Evidence | Replacement path |
|---:|---|---|---|---|
| 28 | CancelEdit | not mapped in the inspected registration block | `OPENMP_REF`, `TODO_VERIFY` | Empty payload is sequenced and logged; no speculative edit-state mutation is applied. |
| 33 | SetPlayerShopName | `samp.dll+0x17E50` | `STATIC_037`, `TODO_VERIFY` | Fixed 32-byte name is bounded, stored and logged. Original internal shop-object calls still need a focused runtime probe. |
| 41 | PlayAudioStream | `samp.dll+0x1D3C0` | `STATIC_037`, `OPENMP_REF`, `TODO_VERIFY` | URL, position, radius and positional flag are bounded; playback uses the optional `BASS.dll` backend. |
| 42 | StopAudioStream | `samp.dll+0x180F0` | `STATIC_037`, `OPENMP_REF` | Active compatibility BASS stream is stopped and freed. |
| 59 | ChatBubble | not mapped in the inspected registration block | `OPENMP_REF`, `TODO_VERIFY` | Per-player, expiring bubble state is queued and rendered above the player. |
| 69 | SetPlayerTeam | `samp.dll+0x19710` | `STATIC_037`, `OPENMP_REF` | Ordered player-pool event updates scoreboard and streamed remote-player team state. |
| 72 | SetPlayerColor | `samp.dll+0x19800` | `STATIC_037`, `OPENMP_REF` | Ordered player-pool event updates scoreboard/name-tag and streamed remote-player colour state. |
| 75 | AttachObjectToPlayer | `samp.dll+0x1C6A0` | `STATIC_037`, `PROBE_TRACE`, `TODO_VERIFY` | Existing bounded object-event path applies attachment through the compatibility script bridge. |
| 137 | ServerJoin | not mapped in the inspected registration block | `PROBE_TRACE`, `OPENMP_REF`, `TODO_VERIFY` | Existing ordered player-pool join path populates the scoreboard. |
| 138 | ServerQuit | not mapped in the inspected registration block | `PROBE_TRACE`, `OPENMP_REF`, `TODO_VERIFY` | Existing ordered player-pool quit path removes the scoreboard entry. |

## Static observations

- RPC 33 reads a fixed 32-byte shop name before calling two original-DLL internal helpers. Those helpers are not
  assumed to exist at the same RVA in the replacement.
- RPC 69 reads `uint16 player_id` plus `uint8 team` and distinguishes local and remote players.
- RPC 72 reads `uint16 player_id` plus `uint32 color` and distinguishes local and remote players.
- The payload layouts used for RPCs 41, 42 and 59 are corroborated by the open.mp netcode definitions, but their
  exact 0.3.7-R5 edge behaviour still requires a comparable original/replacement trace.

## Runtime verification still required

- Confirm positional audio attenuation and coordinate orientation with an original-DLL probe.
- Confirm chat-bubble vertical placement, multiline handling and colour alpha against an original trace.
- Probe the visible GTA shop behaviour caused by RPC 33 before implementing any original internal-helper analogue.
- Verify CancelEdit while editing an object and an attached object; until then the replacement deliberately avoids
  speculative UI/object mutations.
