# Current `samp.dll` Detailed Function Map (2026-03-05)

Purpose: consolidate current reverse-analysis evidence for `samp.dll` (SHA-256 `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`) and map it against the legacy 0.2x source tree.

This document is Team-A output and may be consumed by Team B only as behavioral/spec guidance.

## 1. Binary Inventory Snapshot

- PE profile: `PE32 DLL`, image base `0x10000000`, entry `0x100cbc90`, no exports.
- Function inventory (r2 `aaa;aflj`): `3935` discovered function entries.
- Import surface (count by DLL):
  - `KERNEL32.dll`: 125
  - `USER32.dll`: 88
  - `d3dx9_25.dll`: 46
  - `WSOCK32.dll`: 25
  - `BASS.dll`: 17
  - `GDI32.dll`: 10
  - `ADVAPI32.dll`: 4
  - `WINMM.dll`: 2
  - `SHELL32.dll`: 1
  - `PSAPI.DLL`: 1
  - `COMCTL32.dll`: 1

Primary artifacts:

- `analysis/generated/r2_aflj.json`
- `analysis/generated/top_functions.tsv`
- `analysis/generated/function_range_hist.tsv`
- `analysis/generated/import_dll_counts_rz.tsv`

## 2. Entry/Boot Dispatch Map

Already high-confidence mapped:

1. `entry0 @ 0x100cbc90`:
   - CRT wrapper, reason dispatch (`PROCESS_ATTACH/DETACH`, `THREAD_ATTACH/DETACH`), calls runtime gate and secondary dispatch.
2. `fcn.100cbb0f`:
   - secondary dispatch/bootstrap path; captures OS/version and command line, initializes startup branches, teardown branch on detach.
3. `fcn.100c5270 -> fcn.100c50c0`:
   - main dispatch core for attach/detach flow.

Evidence:

- `analysis/notes/entry_100cbc90.txt`
- `analysis/notes/fcn_100cbb0f_secondary_dispatch.txt`
- `analysis/notes/fcn_100c5270_entry_dispatch.txt`
- `analysis/notes/fcn_100c50c0_dispatch_core.txt`

Legacy analogs:

- `samp/client/main.cpp` (`DllMain`, launch/bootstrap orchestration)
- `samp/client/game/game.cpp` (`InitGame`, `StartGame`, flow gating)
- `samp/client/game/hooks.cpp` (hook installation chain)

## 3. Network Subsystem Map (Extended)

### 3.1 Winsock primitive helpers

| Current function | Behavior evidence | Legacy analog |
|---|---|---|
| `fcn.10053820` | guarded `WSAStartup(2.2)` | `raknet/TCPInterface.cpp`, `raknet/SocketLayer.cpp` init pattern |
| `fcn.10053850` | guarded `WSACleanup()` | same shutdown pattern |
| `fcn.100539c0` | `gethostbyname` -> first addr -> `inet_ntoa` | `SocketLayer::DomainNameToIP` |
| `fcn.10053870` | IPv4 `sockaddr_in` + `connect` helper | `SocketLayer::Connect` style helper |
| `fcn.100538b0` | `socket` + `setsockopt` + `ioctlsocket(FIONBIO)` + `bind` | `SocketLayer::CreateBoundSocket` |

### 3.2 Additional mapped network cluster

| Current function | Observed behavior | Likely legacy family |
|---|---|---|
| `fcn.10053a00` | `recvfrom`, `ntohs`, packet handoff via internal parser | `SocketLayer::RecvFrom` / packet ingest |
| `fcn.10053ab0` | `htons`, repeated `sendto`, failure via `WSAGetLastError` | `SocketLayer::SendTo` |
| `fcn.10053b40` | wrapper: `inet_addr` then forwards to send helper | send path adapter |
| `fcn.10053b70` | `gethostname` + `gethostbyname` + `inet_ntoa`, enumerates addresses | local-address resolution helper |
| `fcn.10053bf0` | `getsockname` + `ntohs` | `SocketLayer::GetLocalPort` |
| `fcn.10055ff0` | resolve host, `socket(AF_INET, SOCK_STREAM)`, `connect`, async wait/gating | TCP connect worker / RakNet bootstrap |
| `fcn.10056880` | stream socket + bind/listen + worker/thread handle path | `TCPInterface::Start` listen path |
| `fcn.10055e60` | object init chain + `WSAStartup` + member reset | network object constructor |
| `fcn.10055f30` | closesocket loop + container cleanup | network object destructor |
| `fcn.10056300` | teardown chain calling `10055f30` then nested container destructors | owner object destructor |

Artifacts:

- `analysis/generated/wsock_xrefs.txt`
- `analysis/generated/wsock_callers.tsv`
- `analysis/notes/fcn_10053a00_auto.txt`
- `analysis/notes/fcn_10053ab0_auto.txt`
- `analysis/notes/fcn_10053b70_auto.txt`
- `analysis/notes/fcn_10053bf0_auto.txt`
- `analysis/notes/fcn_10056880_auto.txt`
- `analysis/notes/fcn_10055e60_auto.txt`
- `analysis/notes/fcn_10055f30_auto.txt`
- `analysis/notes/fcn_10056300_auto.txt`

### 3.3 Connection UX and legacy source anchors

High-confidence retained user-facing flow:

- `"Connecting to %s:%d..."` -> `samp/client/net/netgame.cpp`
- `"Lost connection to the server. Reconnecting.."` -> `samp/client/net/netgame.cpp`
- `"Server has accepted the connection."` -> `samp/client/net/netgame.cpp`
- `"Connected to %.64s"` -> `samp/client/net/netrpc.cpp`

This strongly supports continuity of the `CNetGame` wait/connect/connected/reconnect state machine across versions.

## 4. Render/UI Subsystem Map

### 4.1 D3DX wrapper layer

- Imported D3DX APIs are wrapped through stubs in the `0x100c5c68..0x100c5d76` range (`sub.d3dx9_25.dll_*`).
- Real call sites are concentrated in render-ish clusters:
  - `fcn.10095d10` (10 wrapper calls)
  - `fcn.1006b8e0` (6)
  - `fcn.10092370` (5)
  - `fcn.1006a150` (4)
  - `fcn.10071b60`, `fcn.100708b0`, `fcn.100681d0` (3 each)

Artifacts:

- `analysis/generated/d3dx_wrapper_xrefs.txt`
- `analysis/generated/d3dx_wrapper_callers.tsv`

Legacy mapping signal:

- aligns with old `samp/client/d3dhook/*`, `samp/client/gui/*`, `samp/client/game/*`, and DXUT-backed rendering utility layers.

### 4.2 USER32-heavy UI cluster

High-density callers show window/input/layout flow concentration around:

- `fcn.1007fcf0` (32 USER32 import callsites)
- `fcn.10073550`, `fcn.10093ee0`, `fcn.10061ef0`

Artifacts:

- `analysis/generated/USER32_dll_xrefs.txt`

## 5. Audio Subsystem Map

BASS imports are wrapped as `sub.BASS.dll_*`, with main usage clustered at:

- `fcn.10066480` (8 wrapper calls): init/config/EAX path.
- `fcn.100665c0` (2): stream tag/query path.
- `fcn.10066560`, `fcn.10066960` (stream free / channel stop).

Artifacts:

- `analysis/generated/bass_wrapper_xrefs.txt`
- `analysis/generated/bass_wrapper_callers.tsv`

Legacy mapping signal:

- old client BASS usage for stream/music/sound control is retained in structure.

## 6. Code Density / Priority Regions

By function count and size (64 KB buckets):

1. `0x10000000-0x1000ffff`: 212 funcs, 1,501,747 bytes
2. `0x100c0000-0x100cffff`: 423 funcs, 1,034,289 bytes
3. `0x100a0000-0x100affff`: 284 funcs, 805,743 bytes
4. `0x10010000-0x1001ffff`: 134 funcs, 652,823 bytes
5. `0x10060000-0x1006ffff`: 255 funcs, 410,668 bytes

Interpretation:

- `0x100c****`: runtime/dispatch/thunk-heavy region with wrapper and bootstrap concentration.
- `0x1005****` and `0x10056***`: strong networking/TCP control concentration.
- `0x1006****`, `0x1007****`, `0x1009****`: render/audio/UI-heavy call patterns.

## 7. Crosswalk Status vs 0.2x Source

Current confidence:

- High:
  - boot dispatcher topology,
  - network socket init/bind/resolve/connect/send/recv primitives,
  - reconnect/chat UX strings and state transitions,
  - D3DX/BASS wrapper surfaces.
- Medium:
  - exact object/class layout around the `0x10055e60..0x10057200` network manager cluster,
  - mapping of major render-callers to concrete old class names.
- Low:
  - full callgraph ownership of giant functions (`0x10002340`, `0x100af280`, `0x100c3f50`, `0x10008060`, `0x10014950`),
  - anti-cheat / exception / patch internals not yet behavior-specified.

## 8. Immediate Reverse Priorities (Next Iteration)

1. Name and map the `0x10055e60..0x10057200` object cluster end-to-end (ctor/dtor/start/stop/connect/listen state machine).
2. Build structured callgraph slices for top render callers (`10095d10`, `1006b8e0`, `10092370`) to old `game/gui/d3dhook` modules.
3. Capture runtime traces for connect/lost/reconnect transitions and correlate to function addresses.
4. Convert each mapped cluster into spec IDs under `specs/` before Team-B implementation expansion.

## 9. Repeatable Pipeline

Use:

```bash
tools/generate_re_map.sh samp.dll analysis/generated
```

This regenerates import/function/xref/string crosswalk signals used by this map.
