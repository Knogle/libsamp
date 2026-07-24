# Original 0.3.7-R5 RPC175 raw-angle golden trace (2026-07-23)

## Capture identity

- Run ID: `20260723_222459_rpc175_raw_original_c1610b7a`
- Client: native Windows lab `sshadmin@192.168.3.180`, interactive user
  `DESKTOP-7PGINHC\defaultusr`
- Server: isolated open.mp fixture at `192.168.3.181:7778`, nickname
  `Rpc175Raw`
- Original `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- Probe artifact SHA256:
  `edabd095a121cd3ed5f0fe014f34a10278d1b0f1f8644456589ae1df03b2f3f7`
- Fixed raw-RPC component SHA256:
  `f2a5d0632f43f7cb8b73e9eeaf1cb67e65c57b53690536d956fbbca3616951af`
- Fixture `bare.amx` SHA256:
  `b913530f13e4ea570a02295ac47fae57e4c5cd851b2439e16754dbdc64040185`
- `samp_re.asi` SHA256:
  `c17791b13a28799fdfa9204528bf26fb06ed346b4cf56fe036419716bed7ee2e`;
  its requested and effective mode were both `bypass`
- Fetched run:
  `/tmp/rpc175-raw-original-final/20260723_222459_rpc175_raw_original_c1610b7a`
- Final `manifest.json` SHA256:
  `a1556b2a770c60db9a4efebc95735b9917a5d1c5924de86b1934c8418ff6dd32`
- Final `samp_probe.log` SHA256:
  `2af43ce27ab4049a0bed888014a647e640c170798c2eb0e01633c51c7c6dae0e`
- Final `samp_re.log` SHA256:
  `1a9cd4bc8829f71d6888e0d6f52d87fb2c86403c767a4dd498ca0daa30234dfb`

The probe used the Actor profile. Its R5 relocation preflight and the
seven-byte RPC175 entry-hook preflight passed before installing the original
trampoline at `samp.dll+0x0001d9f0`. The handler, ActorPool guards,
`CActor::SetFacingAngle`, conversion helper, and target field are documented
in
[rpc175_set_actor_facing_static_20260723.md](../re/rpc175_set_actor_facing_static_20260723.md).

## Why a raw fixture was required

The preceding Pawn-native `/rpc175edge` run is not a raw-angle golden sample.
open.mp normalized several values before transport: it changed negative zero
to positive zero, 360 degrees to a small positive remainder, negative 45 to
315, 540 to 180, infinities to NaN, and changed the sign of the qNaN in that
path. That run remains useful `OPENMP_REF`, but it cannot establish the
original client's behavior for the requested bit patterns.

The dedicated component under `tools/openmp_rpc175_fixture` therefore sends
only nine compiled-in RPC 175 payloads. The command accepts no arguments.
Every payload is exactly 48 bits, targets streamed actor 0, uses
`OrderingChannel_SyncRPC`, and bypasses outgoing open.mp event rewriting.
Server-log lines `52692`-`52701` record all nine fixed payloads and nine
successful transport results.

## Nine-case original result

`OBSERVED_037 + PROBE_TRACE`: the nine raw payloads arrived as one contiguous
RPC175 sequence, probe sequences 49 through 57. For every call:

- the request exposed the exact compiled-in 32 angle bits;
- ActorPool slot 0, `CActor`, and cached `CPed` were readable;
- the original RPC handler returned exactly once;
- the probe immediately read the result from `CActor+0x48 -> CPed+0x55c`;
- `converted_bits_match=1` and `reason=ok`.

| Sequence | Case | Payload bytes | Input bits | Stored `CPed+0x55c` bits |
| ---: | --- | --- | ---: | ---: |
| 49 | positive zero | `00 00 00 00 00 00` | `0x00000000` | `0x00000000` |
| 50 | negative zero | `00 00 00 00 00 80` | `0x80000000` | `0x80000000` |
| 51 | 180 degrees | `00 00 00 00 34 43` | `0x43340000` | `0x40490fdb` |
| 52 | 360 degrees | `00 00 00 00 b4 43` | `0x43b40000` | `0x80000000` |
| 53 | negative 45 | `00 00 00 00 34 c2` | `0xc2340000` | `0x00000000` |
| 54 | 540 degrees | `00 00 00 00 07 44` | `0x44070000` | `0x00000000` |
| 55 | positive infinity | `00 00 00 00 80 7f` | `0x7f800000` | `0x00000000` |
| 56 | negative infinity | `00 00 00 00 80 ff` | `0xff800000` | `0x00000000` |
| 57 | quiet NaN with payload | `00 00 34 12 c0 7f` | `0x7fc01234` | `0x7fc01234` |

The matching probe records are final `samp_probe.log` lines
`10832`-`10856`.

`OBSERVED_037 + PROBE_TRACE + STATIC_037`: the runtime bits corroborate the
static x87 branch and operation order:

- signed zero passes through the multiply path and preserves its sign;
- the `(180, 360]` path produces negative zero for exactly 360 degrees;
- finite values outside `[0, 360]` and both infinities load literal positive
  zero;
- 180 degrees stores the original float pi constant `0x40490fdb`;
- the tested quiet-NaN payload passes through both multiplications without
  losing or changing any bit.

This is the required replacement contract. Algebraically simplified angle
normalization, `isfinite` rejection, or `angle <= 0` cannot reproduce it.

## Trace isolation and shutdown health

The `/rpcactors` setup command also generated ordinary background RPC175
traffic while cycling four actors. Those records are not part of the golden
sample. The golden records are unambiguous because the raw component emitted
the exact nine-bit-pattern sequence at server-log lines `52692`-`52701`, and
the probe observed the same contiguous sequence 49-57 in one timestamp tick.

`PROBE_TRACE`:

- `/rpcactorsoff` stopped the setup batch and destroyed all four actors at
  server-log line `52780`.
- Normal quit reached successful `closesocket` at probe-log line `11353` and
  successful `WSACleanup` at line `11368`.
- The server recorded normal quit at lines `52786`-`52787`.
- The final collection lists only the responsive SA-MP browser process; GTA
  had exited. The dump list and Application event table are empty.
- No `exception_filter`, missing-trampoline, failed raw readback, unmatched
  golden call, or `original_called=0` marker occurs.
- No explicit `process_detach` marker was emitted. Shutdown is established by
  socket cleanup, GTA exit, and empty dump/event evidence.

## Remaining limits

- `TODO_VERIFY`: signaling NaNs, subnormals, and values immediately adjacent
  to 0, 180, and 360 are outside this capture.
- `TODO_VERIFY`: truncated payloads and the exact original behavior for extra
  payload bits remain outside this capture.
- `TODO_VERIFY`: actor ID 1000+, inactive slots, and null
  ActorPool/CActor/CPed branches still need isolated runtime samples.
- The completed replacement replay is documented in
  [rpc175_set_actor_facing_raw_samp_re_20260723.md](rpc175_set_actor_facing_raw_samp_re_20260723.md).
  It compares `CPed+0x55c` as raw `uint32` and matches all nine mappings while
  suppressing the original handler.
- The full rebuilt-DLL replay is documented in
  [rpc175_set_actor_facing_raw_reimpl_20260724.md](rpc175_set_actor_facing_raw_reimpl_20260724.md).
  Its first run exposed deferred burst coalescing; the corrected run retains
  and physically applies all nine revisions in order.
