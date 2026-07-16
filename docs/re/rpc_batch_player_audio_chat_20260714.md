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
- `STATIC_037`: the original audio initializer at `samp.dll+0x66480` calls
  `BASS_Init(-1, 44100, 0, NULL, NULL)`, enables network playlist processing with config option `21 = 1`, and sets
  config option `11 = 10000` for the network timeout. It also sets the persistent option-16 network agent to
  `SA-MP/0.3`. The inspected original and replacement prefixes both use BASS 2.4.10 with SHA256
  `a3b00967d5c4ef1a2b4980183934d46ef36cee4b3dc1b2a6da1f820d63448390`.
- `STATIC_037`: the stream worker at `samp.dll+0x66700` calls
  `BASS_StreamCreateURL(url, 0, 0x00940000, NULL, NULL)` on a background thread. It does not add a sample-3D flag.
  For positional RPCs it instead polls every 20 ms and applies distance/radius attenuation through BASS config
  option `5` (global stream volume). The replacement now mirrors the config, creation flags, and attenuation math;
  moving the blocking URL open to a worker remains an explicit parity item.
- `PROBE_TRACE`: replacement runs decoded the UFW RPC 41 payload successfully as a 33-byte
  `https://somafm.com/dubstep130.pls` URL with `use_pos=1`, but the old backend failed at
  `BASS_StreamCreateURL`. That path omitted playlist processing and used different creation flags. The replacement
  now records `BASS_ErrorGetCode` on init/open/play/config failures so the next run can distinguish URL, codec,
  TLS, device, and parameter failures.
- `INFERRED`: missing playlist processing is the strongest explanation for the observed `.pls` failure, but the
  exact cause is not promoted to `OBSERVED_037` until the corrected replacement run succeeds or returns a concrete
  BASS error code.

## Runtime verification still required

- Confirm positional audio attenuation and coordinate orientation with an original-DLL probe.
- Confirm successful `.pls` playback after the original BASS config/flag correction, then move the blocking
  `BASS_StreamCreateURL` call off the game/session thread to match `samp.dll+0x66700`.
- Confirm chat-bubble vertical placement, multiline handling and colour alpha against an original trace.
- Probe the visible GTA shop behaviour caused by RPC 33 before implementing any original internal-helper analogue.
- Verify CancelEdit while editing an object and an attached object; until then the replacement deliberately avoids
  speculative UI/object mutations.
