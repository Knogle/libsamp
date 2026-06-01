# Network Survey (Team A) - 2026-03-04

This document summarizes reverse-analysis observations for networking-relevant logic in `samp.dll`.
It is intended as behavior evidence for clean-room specs.

## String Anchors

- `0x100e599c`: `Connecting to %s:%d...`
- `0x100e6060`: `Connected to {B9C9BF}%.64s`
- `0x100e6d54`: `Failed to connect to host`
- `0x100e5b58`: `Lost connection to the server. Reconnecting..`

## Winsock Import Profile

- DLL: `WSOCK32.dll` (legacy Winsock import set)
- APIs seen: `WSAStartup`, `WSACleanup`, `socket`, `connect`, `bind`, `closesocket`
- APIs seen: `gethostbyname`, `inet_addr`, `inet_ntoa`, `htons`, `ntohs`
- APIs seen: `send`, `recv`, `sendto`, `recvfrom`
- No `Ws2_32.dll` `getaddrinfo`/`freeaddrinfo` imports were found.

## Function-Level Notes (Inferred)

- `fcn.10053820`: one-time `WSAStartup(0x202, ...)` guarded by a global flag.
- `fcn.10053850`: conditional `WSACleanup()` and clear global flag.
- `fcn.10053870`: connect helper that builds `sockaddr_in` and calls `connect`.
- `fcn.100538b0`: socket setup path with several `setsockopt` calls, `ioctlsocket(..., 0x8004667e, ...)`, and `bind`.
- `fcn.100539c0`: resolver helper `gethostbyname(name)` -> first address -> `inet_ntoa`.
- `fcn.10055ff0`: connection worker that resolves hostname via `gethostbyname`, opens `socket(AF_INET, SOCK_STREAM, 0)`, then `connect`.

## Artifact References

- `analysis/notes/fcn_10053820_wsa_init.txt`
- `analysis/notes/fcn_10053850_wsa_cleanup.txt`
- `analysis/notes/fcn_10053870_connect_ipv4.txt`
- `analysis/notes/fcn_100538b0_socket_setup_bind.txt`
- `analysis/notes/fcn_100539c0_resolve_helper.txt`
- `analysis/notes/fcn_10055ff0_connect_worker.txt`
