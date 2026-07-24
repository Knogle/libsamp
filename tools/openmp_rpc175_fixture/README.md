# RPC175 fixed open.mp compatibility fixture

This is a deliberately narrow laboratory component for comparing the original
SA-MP 0.3.7 client with the replacement client. It sends nine fixed RPC 175
(`SetActorFacingAngle`) payloads to actor 0 when an eligible player enters
`/rpc175raw`.

It is not a general raw-RPC tool:

- the RPC ID, actor ID, bytes, 48-bit length, ordering channel, and sequence
  are compiled in;
- the exact command accepts no arguments;
- bots, non-0.3.7 clients, non-legacy transports, and uninitialized players are
  rejected;
- actor 0 must exist and be streamed to the player;
- each connection can trigger the sequence once.

Do not install this fixture on a public or production server.

## Fixed sequence

Every payload begins with actor ID 0 as a little-endian `uint16`, followed by
the listed raw little-endian float bits. Every call carries exactly 48 bits.

| Case | Actor ID | Angle bits | Payload bytes |
| --- | ---: | ---: | --- |
| `positive_zero` | 0 | `0x00000000` (+0) | `00 00 00 00 00 00` |
| `negative_zero` | 0 | `0x80000000` (-0) | `00 00 00 00 00 80` |
| `positive_180` | 0 | `0x43340000` (180) | `00 00 00 00 34 43` |
| `positive_360` | 0 | `0x43b40000` (360) | `00 00 00 00 b4 43` |
| `negative_45` | 0 | `0xc2340000` (-45) | `00 00 00 00 34 c2` |
| `positive_540` | 0 | `0x44070000` (540) | `00 00 00 00 07 44` |
| `positive_infinity` | 0 | `0x7f800000` (+inf) | `00 00 00 00 80 7f` |
| `negative_infinity` | 0 | `0xff800000` (-inf) | `00 00 00 00 80 ff` |
| `quiet_nan_payload_0x401234` | 0 | `0x7fc01234` (qNaN) | `00 00 34 12 c0 7f` |

All calls use `OrderingChannel_SyncRPC` and `dispatchEvents=false`. This avoids
other open.mp outgoing-event handlers rewriting or suppressing the laboratory
vectors.

## Evidence

`STATIC_037`: the original 0.3.7-R5 handler at `samp.dll+0x1D9F0` reads a
little-endian `uint16` actor ID followed by the raw 32-bit float value. The
analyzed DLL SHA256 is
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
See
[`../../docs/re/rpc175_set_actor_facing_static_20260723.md`](../../docs/re/rpc175_set_actor_facing_static_20260723.md).

`OPENMP_REF`: this component was implemented against the local
`open.mp/SDK` commit
`3ee7bc4ab20c22359c34c08c38f93815b44bffd5`. In that SDK,
`INetwork::sendRPC` documents the `Span<uint8_t>` size as a count of bits.
The matching local LegacyNetwork implementation constructs a bitstream and
sets its write offset to that exact count before calling RakNet.

## Build

The local server is an i386 ELF binary, so use the supplied wrapper:

```sh
tools/openmp_rpc175_fixture/build.sh
```

The output is `tools/openmp_rpc175_fixture/build/rpc175_fixture.so`. To select
the other local SDK checkout:

```sh
OMP_SDK_DIR=/home/chairman/Projects/LastBedStanding/deps/omp-sdk \
  tools/openmp_rpc175_fixture/build.sh
```

The build script verifies that the result is an ELF32 i386 shared object and
prints its SHA256. It does not copy the component into any server directory.

## Controlled use

Load the component only in an isolated copy of the test server. Ensure the
gamemode has created actor 0 and that actor 0 is streamed to the test client.
Connect with the 0.3.7 client and enter exactly:

```text
/rpc175raw
```

The server log records every fixed vector, bit count, and `sendRPC` result.
Reconnect before another run so traces cannot accidentally contain duplicate
fixture sequences.
