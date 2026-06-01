# Legacy 0.2x Crosswalk to Current `samp.dll` (2026-03-05)

Purpose: use the old 0.2x codebase as a structure guide for reverse analysis of the current binary (`samp.dll`, SHA-256 `b72b5d...53a2`).

## Summary

The old source is very useful for structural orientation. The current binary appears to preserve the same high-level module layout:

1. DLL bootstrap / runtime dispatch
2. Game hook + D3D/UI setup
3. CNetGame state machine and reconnect loop
4. RakNet SocketLayer/TCPInterface networking core

## High-Confidence Mappings

### A) Network socket setup helper

- Current RE evidence:
- `fcn.100538b0` uses `socket(AF_INET, SOCK_DGRAM, 0)`, `setsockopt`, `ioctlsocket(FIONBIO)`, `bind`.
- Old source analog:
- `raknet/SocketLayer.cpp` `SocketLayer::CreateBoundSocket(...)`
- `/tmp/samp-0.2x-ref/raknet/SocketLayer.cpp:150`
- Strong opcode-level behavior match:
- `SO_REUSEADDR`, `SO_RCVBUF`, `SO_SNDBUF`, `SO_BROADCAST`, `FIONBIO`, then `bind`.

### B) Name resolve helper

- Current RE evidence:
- `fcn.100539c0`: `gethostbyname(name)` -> first address -> `inet_ntoa`.
- Old source analog:
- `SocketLayer::DomainNameToIP(...)`
- `/tmp/samp-0.2x-ref/raknet/SocketLayer.cpp:306`

### C) Winsock init/cleanup wrappers

- Current RE evidence:
- `fcn.10053820`: one-time `WSAStartup(0x0202, ...)`
- `fcn.10053850`: guarded `WSACleanup()`
- Old source analog:
- Multiple modules initialize winsock similarly (`TCPInterface`, `httpclient`, `rcon`).
- `/tmp/samp-0.2x-ref/raknet/TCPInterface.cpp:57`
- `/tmp/samp-0.2x-ref/saco/httpclient.cpp:39`

### D) Connect/reconnect user flow

- Current RE string anchors:
- `0x100e599c`: `Connecting to %s:%d...`
- `0x100e5b58`: `Lost connection to the server. Reconnecting..`
- `0x100e6060`: `Connected to {B9C9BF}%.64s`
- Old source analog:
- `CNetGame::Process` reconnect loop
- `/tmp/samp-0.2x-ref/saco/net/netgame.cpp:307`
- `Packet_ConnectionLost`
- `/tmp/samp-0.2x-ref/saco/net/netgame.cpp:683`
- `InitGame` debug connect message
- `/tmp/samp-0.2x-ref/saco/net/netrpc.cpp:143`

### E) DLL startup structure

- Current RE evidence:
- Entry point at `0x100cbc90` does CRT-style reason dispatch with internal handlers.
- Old source analog:
- `DllMain` + deferred launch monitor / init pipeline.
- `/tmp/samp-0.2x-ref/saco/main.cpp:110`
- Map file confirms `_DllMain@12` and `__DllMainCRTStartup@12`.
- `/tmp/samp-0.2x-ref/archive/samp_a.map:1645`
- `/tmp/samp-0.2x-ref/archive/samp_a.map:1763`

## What This Means for Rebuild

1. The old repo is a valid architecture reference for subsystem boundaries.
2. It should be used by Team A to derive behavior specs, not copied into Team B code.
3. The biggest current gap is not just size: it is missing startup/game-hook/game-loop behavior.

## Priority Reverse Targets (Current Binary)

1. Entry chain around `0x100cbc90` (bootstrap -> init handlers).
2. Main game init path analogous to old `DoInitStuff` and launch monitor.
3. CNetGame state machine equivalents for wait/connect/connected/reconnect.
4. RakNet socket setup + resolver + connect call sites (already partially mapped).
