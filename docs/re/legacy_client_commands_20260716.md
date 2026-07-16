# SA-MP 0.3.7 client command inventory

Date: 2026-07-16

## Evidence

- Binary: original SA-MP 0.3.7-R5 `samp.dll`
- SHA-256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Registration function: `samp.dll+0x69110` (`STATIC_037`)
- RCON handler: `samp.dll+0x69030`; packet `0xC9`, `DWORD` length, command bytes,
  `HIGH_PRIORITY`/`RELIABLE`/channel 0 (`STATIC_037`)
- Save handlers: `samp.dll+0x68A00` (`save`) and `samp.dll+0x68B80` (`rs`)
- Supporting string XREFs:
  `analysis/generated/ghidra_samp_20260609/string_xrefs.tsv`

The table is an inventory of commands registered by the original client. It
does not imply that every command should be exposed unchanged in the replacement;
several are internal diagnostics or depend on legacy wrapper state that has not
yet been reconstructed.

## Commands registered unconditionally

| Command | Observed purpose | Replacement status |
| --- | --- | --- |
| `quit`, `q` | Disconnect and exit | Implemented as `/quit` and `/q` |
| `save` | Append Pawn `AddPlayerClass`/`AddStaticVehicle` line to `savedpositions.txt` | Implemented as `/save` and `/savepos` |
| `rs` | Append compact raw coordinates to `rawpositions.txt`/`rawvehicles.txt` | Implemented |
| `rcon` | Send an RCON command | Implemented as the original packet `0xC9` plus `DWORD` length and command bytes; payloads are redacted from replacement traces/history |
| `mem` | Print current memory value | Not implemented |
| `fpslimit` | Set frame limiter, original accepted 20-90 | Implemented with an EndScene pacing fallback (`INFERRED`) |
| `pagesize` | Set chat lines, original accepted 10-20 | Implemented; the overlay retains 20 lines and displays the selected tail |
| `fontsize` | Set chat font adjustment, original accepted -3 through 5 | Implemented for the replacement chat fonts |
| `nametagstatus` | Toggle name-tag status display | Partially implemented; gates the replacement health/armour status bars (`INFERRED`, `TODO_VERIFY`) |
| `timestamp` | Toggle chat timestamps | Implemented |
| `headmove` | Toggle remote head movement | Command/state implemented; remote head animation itself remains `TODO_VERIFY` |
| `hudscalefix` | Toggle HUD scale fix | Not implemented |
| `testdw` | Death-window diagnostic | Not implemented |
| `interior` | Print current interior | Implemented through GTA opcode `0x077E`, matching the original handler |
| `togobjlight` | Toggle object lighting | Not implemented |
| `cmpstat` | Internal compression/statistics diagnostic | Not implemented |
| `dl` | Toggle vehicle debug labels | Implemented |
| `ctd` | Internal client diagnostic | Not implemented |
| `audiomsg` | Toggle audio-stream messages | Implemented; successful streams use the original `Audio stream: %s` message |
| `logurls` | Toggle URL logging/messages | Implemented for explicit audio URL tracing |

## Commands registered only in the original debug mode

The registration block at `samp.dll+0x69246` is conditional on the original
debug-mode global.

| Command | Observed purpose |
| --- | --- |
| `vsel` | Vehicle selection diagnostic |
| `v`, `vehicle` | Spawn/select a vehicle by model/id |
| `player_skin` | Change local player skin |
| `set_weather` | Set local weather |
| `set_time` | Set local time |

These commands should remain development-only if reintroduced. They directly
mutate local GTA state and are not required for normal 0.3.7 server compatibility.

## Replacement-only diagnostic

`/debug 0-3` controls safe, read-only diagnostic summaries in the replacement
chat overlay:

- `0`: disabled;
- `1`: connection/session/spawn and local position;
- `2`: additionally pool and sync counters;
- `3`: additionally health, armour, interior, weapon, vehicle and last packet.

The output is throttled according to level and is also retained in the normal
`samp_runtime.log` path through the chat trace.

## Implemented settings and mapping commands

The handlers and value ranges were recovered from the R5 command block and
their direct callees:

| Command | Original handler | Reproduced behavior |
| --- | --- | --- |
| `pagesize` | `samp.dll+0x685F0` | Decimal `10` through `20`; invalid input prints `pagesize [10-20] (lines)` |
| `fontsize` | `samp.dll+0x68670` | Adjustment `-3` through `5`; invalid input prints `Valid fontsize: -3 to 5` |
| `timestamp` | `samp.dll+0x68730` | Toggles timestamps captured when each chat line is added |
| `audiomsg` | `samp.dll+0x68790` | Toggles the stream announcement; R5 `samp.dll+0x66A3C` uses `Audio stream: %s` |
| `logurls` | `samp.dll+0x68800` | Toggles explicit URL entries in `samp_runtime.log` |
| `fpslimit` | `samp.dll+0x688D0` | Decimal `20` through `90`; replacement pacing is local to its EndScene hook |
| `headmove` | `samp.dll+0x68960` | Toggle and original confirmation text; remote head animation is not reconstructed yet |
| `rs` | `samp.dll+0x68B80` | Appends the original compact formats shown below |
| `interior` | `samp.dll+0x68FD0` | Executes GTA opcode `0x077E` and prints `Current Interior: %u` |
| `nametagstatus` | `samp.dll+0x68720`, callee `+0x8E90` | Preserves the name and gates the replacement status bars; original AFK/status-icon parity remains open |

`/rs` uses the original module-relative output names and formats:

```text
rawpositions.txt: x,y,z,angle ; comment
rawvehicles.txt:  model,x,y,z,angle,color1,color2 ; comment
```

The original toggle keys (`audiomsgoff`, `logurls`, `disableheadmove`, and
`nonametagstatus`) are statically confirmed. The replacement currently keeps
these values for the life of the DLL rather than writing the original client
configuration; persistence is `TODO_VERIFY`.

## Legacy `save` behavior reproduced

`STATIC_037` at `samp.dll+0x68A00` shows append mode and these exact layouts:

```text
AddPlayerClass(skin,x,y,z,angle,0,0,0,0,0,0); // comment
AddStaticVehicle(model,x,y,z,angle,color1,color2); // comment
```

The original output file is module-relative `savedpositions.txt`. The
replacement accepts both `/save comment` for legacy compatibility and the more
explicit `/savepos comment` alias requested for the rebuilt client. Newlines are
removed from comments before writing.
