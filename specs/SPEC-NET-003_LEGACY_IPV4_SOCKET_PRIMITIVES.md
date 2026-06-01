# SPEC-NET-003: Legacy IPv4 Socket Primitives

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Define behavior for the IPv4-era low-level socket helpers mapped in current `samp.dll`, used by bootstrap/connect/packet paths around the `0x10053***` and `0x10056***` clusters.

## Mapped Function Evidence

1. `fcn.100538b0`: UDP socket create + option tuning + nonblocking + bind.
2. `fcn.10053ab0`: UDP send helper (`sendto`, `htons`, error retrieval).
3. `fcn.10053a00`: UDP recv helper (`recvfrom`, source port normalization).
4. `fcn.10053bf0`: local port query via `getsockname` + `ntohs`.
5. `fcn.10053b70`: host/local name resolution (`gethostname`, `gethostbyname`, `inet_ntoa`).
6. `fcn.10055ff0`: TCP connect worker (`gethostbyname`, `socket(AF_INET,SOCK_STREAM)`, `connect`).
7. `fcn.10056880`: TCP listen bootstrap (`socket`, `bind`, `listen`) plus worker/thread launch.

## Required Behavior

1. Runtime uses Winsock-compatible init/shutdown discipline.
2. UDP bind helper:
   - socket family/type `AF_INET/SOCK_DGRAM`,
   - best-effort option tuning (`SO_REUSEADDR`, buffers, broadcast),
   - optional nonblocking enable,
   - bind to `INADDR_ANY` or forced IPv4 literal.
3. UDP send helper:
   - takes destination IPv4 + host-order port,
   - normalizes port with `htons`,
   - returns byte count or failure.
4. UDP recv helper:
   - returns payload bytes and source `{IPv4,port}`,
   - normalizes source port with `ntohs`,
   - differentiates would-block from hard error.
5. Local port helper:
   - returns host-order bound port from `getsockname`.
6. Resolve helper:
   - uses IPv4-compatible first-record semantics for hostnames.
7. Local host IPv4 list helper:
   - uses `gethostname` then `gethostbyname`,
   - iterates `h_addr_list` with deterministic bounded count,
   - converts each candidate via `inet_ntoa`,
   - stores each result into fixed 16-byte IPv4 text slots (`xxx.xxx.xxx.xxx\0`).
8. TCP connect helper:
   - resolves hostname/literal to IPv4,
   - opens stream socket and connects.
9. TCP listen helper:
   - creates stream listener, binds to local port, and listens with backlog.

## Test Matrix (Minimum)

1. Create two UDP sockets on loopback, send/receive packet end-to-end.
2. Verify source port extraction from recv path.
3. Verify local port query returns assigned non-zero port.
4. Resolve `localhost` with non-zero result.
5. Enumerate local host IPv4 addresses and verify bounded output semantics.
6. Start TCP listener on ephemeral port and connect via loopback.
7. Accept connected client successfully.

## References

- `analysis/generated/wsock_xrefs.txt`
- `analysis/notes/fcn_10053a00_auto.txt`
- `analysis/notes/fcn_10053ab0_auto.txt`
- `analysis/notes/fcn_10053b70_auto.txt`
- `analysis/notes/fcn_10053bf0_auto.txt`
- `analysis/notes/fcn_10055ff0_connect_worker.txt`
- `analysis/notes/fcn_10056880_auto.txt`
