# Initial Findings (2026-03-04)

## Inputs

- `samp.dll` (local file)
- `sa-mp-0.3.7-R5-1-install.exe` (NSIS installer)

## Integrity

- `samp.dll` SHA-256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- installer SHA-256: `550321da5b10c472bc719bca2c0df74b240f80eff828079eb5253598fe112ae3`
- `samp.dll` extracted from installer is bit-identical to local `samp.dll`.

## Binary Profile

- `samp.dll` is `PE32` x86 DLL, stripped symbols, no exports.
- Compile timestamp in PE header: `2022-11-14 23:39:26`.
- Embedded debug path string: `c:\sampcvs037R4\sampcvs\main\saco\Release\samp.pdb`.
- Imports include `WSOCK32.dll`, `d3dx9_25.dll`, `BASS.dll`, `USER32.dll`, `KERNEL32.dll`.

## Networking-Relevant Observation

- Networking import set is Winsock 1 style (`WSOCK32.dll`) with IPv4-era APIs:
- `socket`, `connect`, `bind`, `send`, `recv`, `sendto`, `recvfrom`
- `inet_addr`, `inet_ntoa`, `gethostbyname`, `htons`, `ntohs`, `WSAStartup`
- No imported `Ws2_32` modern resolver APIs like `getaddrinfo` were found.

## Direct Implication for IPv6

- Current binary design is strongly IPv4-centric.
- A clean-room rebuild for IPv6 should define a dual-stack resolver + connection layer first, then adapt higher-level RakNet-facing connection logic to consume it.
