# RPC176 fixed open.mp compatibility fixture

This is a deliberately narrow laboratory component for comparing the original
SA-MP 0.3.7 client with the replacement client. It sends nine fixed RPC 176
(`ScrSetActorPos` / `SetActorPos`) payloads when an eligible player enters
`/rpc176raw`.

It is not a general raw-RPC tool:

- the RPC ID, actor IDs, coordinate bits, payload bytes, bit lengths, ordering
  channel, and order are compiled in;
- the exact command accepts no arguments;
- bots, non-0.3.7 clients, non-legacy transports, and uninitialized players are
  rejected;
- actor 0 must exist and be streamed to the player;
- actor 999 must be absent, so its nonfinite-coordinate case cannot target a
  live client Actor slot;
- every shortened payload uses out-of-range Actor ID 1000; there is no
  truncated Actor-0 payload;
- each connection can trigger the sequence once.

Do not install this fixture on a public or production server.

## Fixed sequence

Every complete payload starts with a little-endian `uint16` Actor ID followed
by raw little-endian X/Y/Z float bits.

| # | Case | Actor | X/Y/Z bits | Bits | Exact payload bytes |
| ---: | --- | ---: | --- | ---: | --- |
| 0 | `active_baseline_112` | 0 | `439b2000 / 44ffd000 / 41880000` | 112 | `00 00 00 20 9b 43 00 d0 ff 44 00 00 88 41` |
| 1 | `active_fractional_112` | 0 | `439b9000 / 44ffb800 / 41940000` | 112 | `00 00 00 90 9b 43 00 b8 ff 44 00 00 94 41` |
| 2 | `active_duplicate_112` | 0 | exact duplicate of case 1 | 112 | `00 00 00 90 9b 43 00 b8 ff 44 00 00 94 41` |
| 3 | `active_negative_zero_x_112` | 0 | `80000000 / 44ffd000 / 41880000` | 112 | `00 00 00 00 00 80 00 d0 ff 44 00 00 88 41` |
| 4 | `active_extra_one_bit_113` | 0 | `439ac000 / 44ffe800 / 41860000` | 113 | `00 00 00 c0 9a 43 00 e8 ff 44 00 00 86 41`, then one set bit |
| 5 | `inactive_999_nonfinite_112` | 999 | `7fc01234 / 7f800000 / ff800000` | 112 | `e7 03 34 12 c0 7f 00 00 80 7f 00 00 80 ff` |
| 6 | `out_of_range_1000_112` | 1000 | `42f68000 / c3e44000 / 429c4000` | 112 | `e8 03 00 80 f6 42 00 40 e4 c3 00 40 9c 42` |
| 7 | `truncated_after_actor_16` | 1000 | omitted | 16 | `e8 03` |
| 8 | `active_return_112` | 0 | `439b006f / 44ffdd44 / 41851eb8` | 112 | `00 00 6f 00 9b 43 44 dd ff 44 b8 1e 85 41` |

Case 4 uses a 15-byte backing array whose last byte is `0x80`, but passes a
113-bit span to `sendRPC`. Only the most-significant bit of that final byte is
part of the RPC. The component does not byte-round this case to 120 bits.

Cases 5 and 7 make potentially troublesome values harmless before any
position mutation: Actor 999 is required to be absent, while Actor ID 1000
fails the original unsigned `< 1000` pool guard. Case 8 restores the precise
server-side actor-0 fixture position after the active mutation cases.

All calls use `OrderingChannel_SyncRPC` and `dispatchEvents=false`. This avoids
other open.mp outgoing-event handlers rewriting or suppressing the laboratory
vectors.

## Evidence

`STATIC_037`: the original R5 registration maps ID 176 to the handler at
`samp.dll+0x1DAD0`. It reads a little-endian `uint16` Actor ID and three raw
32-bit coordinate words, then ignores remaining payload bits. The analyzed
DLL SHA256 is
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
See
[`../../docs/re/rpc176_set_actor_position_static_20260723.md`](../../docs/re/rpc176_set_actor_position_static_20260723.md).

`OPENMP_REF`: this component targets the local `open.mp/SDK`. In that SDK,
`INetwork::sendRPC` documents the `Span<uint8_t>` size as a count of bits.
The LegacyNetwork implementation sets its BitStream write offset to that exact
count before calling RakNet.

## Build

The local server is an i386 ELF binary, so use the supplied wrapper:

```sh
tools/openmp_rpc176_fixture/build.sh
```

The output is
`tools/openmp_rpc176_fixture/build/rpc176_fixture.so`. To select the other
local SDK checkout:

```sh
OMP_SDK_DIR=/home/chairman/Projects/LastBedStanding/deps/omp-sdk \
  tools/openmp_rpc176_fixture/build.sh
```

The build script verifies that the result is an ELF32 i386 shared object and
prints its SHA256. It does not copy the component into any server directory.

## Controlled use

Load the component only in an isolated copy of the test server. Ensure actor 0
exists and is streamed to the test client, and actor 999 is unused. Connect
with the 0.3.7 client and enter exactly:

```text
/rpc176raw
```

The allowlisted Windows-lab equivalent is:

```sh
tools/windows/remote_lab/samp_lab.sh key RPC176RAW rpc176_raw
```

The server log records each fixed vector, exact bit and byte counts, complete
transport bytes, and `sendRPC` result. Reconnect before another run so traces
cannot accidentally contain duplicate fixture sequences.
