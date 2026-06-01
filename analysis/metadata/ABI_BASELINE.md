# ABI Baseline (Original `samp.dll`)

## PE Identity

- Format: `PE32 DLL` (`i386`)
- Image base: `0x10000000`
- Entry point RVA: `0x000cbc90`
- Subsystem: `Windows GUI`
- Export directory: absent (`0`)

## Data Directories (Relevant)

- Import directory: present
- Resource directory: present
- Relocation directory: present
- TLS directory: absent

## Sections

1. `.text` size `0x000e33c4`
2. `.rdata` size `0x00018f1e`
3. `.data` size `0x0001b000`
4. `.rsrc` size `0x000006b0`
5. `.reloc` size `0x0000b9ae`

## Imported DLL Set

- `ADVAPI32.dll`
- `BASS.dll`
- `COMCTL32.dll`
- `GDI32.dll`
- `KERNEL32.dll`
- `PSAPI.DLL`
- `SHELL32.dll`
- `USER32.dll`
- `WINMM.dll`
- `WSOCK32.dll`
- `d3dx9_25.dll`

## Networking ABI-Relevant Imports

- `WSAStartup`, `WSACleanup`
- `socket`, `connect`, `bind`, `closesocket`
- `gethostbyname`, `inet_addr`, `inet_ntoa`
- `send`, `recv`, `sendto`, `recvfrom`
- `setsockopt`, `ioctlsocket`, `select`

## Clean-Room Constraint

Team B should use this as structural reference only. No decompiled control flow or copied implementation is allowed in `reimpl/`.
