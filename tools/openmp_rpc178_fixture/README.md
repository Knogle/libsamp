# RPC178 fixed open.mp compatibility fixture

This is a deliberately narrow laboratory component for comparing the original
SA-MP 0.3.7 client with the replacement client. It sends five fixed RPC 178
(`SetActorHealth`) payloads when an eligible player enters `/rpc178raw`.

It is not a general raw-RPC tool:

- the RPC ID, bytes, bit lengths, ordering channel, and sequence are compiled
  in;
- the exact command accepts no arguments;
- bots, non-0.3.7 clients, non-legacy transports, and uninitialized players are
  rejected;
- actor 0 must exist and be streamed to the player;
- actor 999 must be absent, so the inactive-ID case is actually inactive;
- each connection can trigger the sequence once.

Do not install this fixture on a public or production server.

## Fixed sequence

| Case | Actor ID | Health bits | Payload bits | Bytes |
| --- | ---: | ---: | ---: | --- |
| `valid_48` | 0 | `0x42c80000` (100.0) | 48 | `00 00 00 00 c8 42` |
| `out_of_range_1000_48` | 1000 | `0x42aa0000` (85.0) | 48 | `e8 03 00 00 aa 42` |
| `inactive_999_48` | 999 | `0x428c0000` (70.0) | 48 | `e7 03 00 00 8c 42` |
| `truncated_47` | 0 | `0x425c0000` (55.0) | 47 | first 47 bits of `00 00 00 00 5c 42` |
| `extra_one_bit_49` | 0 | `0x42200000` (40.0) | 49 | `00 00 00 00 20 42`, then a fixed set bit |

All calls use `OrderingChannel_SyncRPC` and `dispatchEvents=false`. This avoids
other open.mp outgoing-event handlers rewriting or suppressing the laboratory
vectors.

## Evidence

`STATIC_037`: the original 0.3.7-R5 handler at `samp.dll+0x1DBE0` reads a
little-endian `uint16` actor ID followed by the raw 32-bit float value. The
analyzed DLL SHA256 is
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
See
[`../../docs/re/rpc178_set_actor_health_static_20260723.md`](../../docs/re/rpc178_set_actor_health_static_20260723.md).

`OPENMP_REF`: this component was implemented against the local
`open.mp/SDK` commit
`3ee7bc4ab20c22359c34c08c38f93815b44bffd5`. In that SDK,
`INetwork::sendRPC` documents the `Span<uint8_t>` size as a count of bits.
The matching local LegacyNetwork implementation constructs a bitstream and
sets its write offset to that exact count before calling RakNet. This is why
the 47- and 49-bit cases do not become byte-rounded.

## Build

The local server is an i386 ELF binary, so use the supplied wrapper:

```sh
tools/openmp_rpc178_fixture/build.sh
```

The output is `tools/openmp_rpc178_fixture/build/rpc178_fixture.so`. To select
the other local SDK checkout:

```sh
OMP_SDK_DIR=/home/chairman/Projects/LastBedStanding/deps/omp-sdk \
  tools/openmp_rpc178_fixture/build.sh
```

The build script verifies that the result is an ELF32 i386 shared object and
prints its SHA256. It does not copy the component into any server directory.

## Controlled use

Load the component only in an isolated copy of the test server. Ensure the
gamemode has created actor 0, that actor 0 is streamed to the test client, and
that actor 999 is unused. Connect with the 0.3.7 client and enter:

```text
/rpc178raw
```

The server log records every fixed vector, bit count, and `sendRPC` result.
Reconnect before another run so traces cannot accidentally contain duplicate
fixture sequences.
