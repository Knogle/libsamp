# RPC175 `samp_re` raw-angle parity (2026-07-23)

## Capture identity

- Replacement run:
  `20260723_225240_rpc175_raw_replace_85785e07`
- Native Windows lab: `sshadmin@192.168.3.180`, interactive user
  `DESKTOP-7PGINHC\defaultusr`
- Server fixture: `192.168.3.181:7778`, nickname `Rpc175Repl`
- Original reference:
  [rpc175_set_actor_facing_raw_original_20260723.md](rpc175_set_actor_facing_raw_original_20260723.md)
- Original `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- `samp_re.asi` SHA256:
  `3cb3a4011026aba37464707502f6902680170646535df2e66041280422eae24d`
- Passive probe SHA256:
  `edabd095a121cd3ed5f0fe014f34a10278d1b0f1f8644456589ae1df03b2f3f7`
- Fixed raw-RPC component SHA256:
  `f2a5d0632f43f7cb8b73e9eeaf1cb67e65c57b53690536d956fbbca3616951af`
- Fixture `bare.amx` SHA256:
  `b913530f13e4ea570a02295ac47fae57e4c5cd851b2439e16754dbdc64040185`
- Fetched run:
  `/tmp/rpc175-raw-replace-final/20260723_225240_rpc175_raw_replace_85785e07`
- Final `manifest.json` SHA256:
  `de8ddd8cc1c74080f35a667691cec4004143128af75217a460f28dfa9d869e88`
- Final `samp_re.log` SHA256:
  `dfb4b13708957531dea3c1479a8da7c3e2cfa6c18276e90251723ad4cec5df7a`
- Final `samp_probe.log` SHA256:
  `a1a0b93a0240e6765e39ae24b5dd322210f3c414113a27f1e41810b4bf608197`

The manifest records requested and effective `samp_re` mode as `replace`,
with `rpc175_set_actor_facing_angle` and the already verified
`rpc178_set_actor_health` selected. Both disable mechanisms were off. The
probe ran in passive mode so that only the overlay owned the RPC175 entry.

The overlay accepted the exact R5 identity, the relocated seven-byte prologue
at `samp.dll+0x0001d9f0`, `CActor::SetFacingAngle` at
`samp.dll+0x0009c570`, the GTA ped-reference wrapper, the conversion helper,
and all five conversion constants. It then installed its RPC175 and RPC178
hooks and reported `startup result=active`.

## Nine-case comparison

The server had streamed actor 0 before the raw command. Server-log lines
`52917`-`52926` record the fixed 48-bit trigger and all nine successful sends.
The unique sequence arrived as replacement RPC175 sequences 5 through 13 at
`samp_re.log` lines `18`-`26`.

`OBSERVED_037 + PROBE_TRACE + STATIC_037`:

| Original seq | Replacement seq | Case | Input bits | Original stored | Replacement stored | Original handler calls |
| ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 49 | 5 | positive zero | `0x00000000` | `0x00000000` | `0x00000000` | 0 |
| 50 | 6 | negative zero | `0x80000000` | `0x80000000` | `0x80000000` | 0 |
| 51 | 7 | 180 degrees | `0x43340000` | `0x40490fdb` | `0x40490fdb` | 0 |
| 52 | 8 | 360 degrees | `0x43b40000` | `0x80000000` | `0x80000000` | 0 |
| 53 | 9 | negative 45 | `0xc2340000` | `0x00000000` | `0x00000000` | 0 |
| 54 | 10 | 540 degrees | `0x44070000` | `0x00000000` | `0x00000000` | 0 |
| 55 | 11 | positive infinity | `0x7f800000` | `0x00000000` | `0x00000000` | 0 |
| 56 | 12 | negative infinity | `0xff800000` | `0x00000000` | `0x00000000` | 0 |
| 57 | 13 | quiet NaN with payload | `0x7fc01234` | `0x7fc01234` | `0x7fc01234` | 0 |

Every replacement record reports:

- `result=applied`;
- an immediate successful `CPed+0x55c` readback;
- `heading_match=1`;
- `original_calls=0`.

No fallback, mismatch, or unexpected original-handler call occurs anywhere in
the run. Thus the replacement, rather than the original RPC175 handler or its
downstream conversion method, produced all nine observed field values.

The ordinary actor-cycle traffic is additional precision evidence, not part
of the fixed nine-case sample. Four separate background calls map raw
135 degrees (`0x43070000`) to `0x4016cbe5`, with matching readback and no
original-handler call. This corroborates that the replacement's register-only
x87 instruction sequence inherits GTA's observed PC24 state instead of
silently using a PC53 or algebraically simplified result.

## Build and regression checks

- Clean host configure/build and CTest passed both RPC175 and RPC178 suites,
  2/2.
- The RPC175 suite covers the nine raw golden mappings, all bit lengths below
  48, acceptance of trailing bits, and the discriminating 135-degree PC24
  result.
- The PE32/i386 logic test exited successfully under Wine.
- The deployed ASI hash matched the local artifact hash before launch.
- Background RPC178 calls continued to report exact health readback and
  `original_calls=0`, providing a smoke check that adding RPC175 did not break
  the preceding hook.

## Shutdown health

`PROBE_TRACE`:

- The server stopped the actor batch at line `53009` and recorded normal
  client quit at lines `53010`-`53011`.
- The passive probe reached successful `closesocket` at line `9158` and
  successful `WSACleanup` at line `9174`.
- The final collection lists only the responsive SA-MP browser process; GTA
  had exited. The dump list and Application event table are empty.
- No `exception_filter`, crash, failed readback, heading mismatch, fallback,
  or ownership-loss marker occurs.
- No explicit `process_detach` marker was emitted. Shutdown is established by
  socket cleanup, GTA exit, server quit, and empty dump/event evidence.

## Remaining limits

- `TODO_VERIFY`: signaling NaNs, subnormals, and values immediately adjacent
  to 0, 180, and 360.
- `TODO_VERIFY`: original and replacement behavior for truncated and
  extra-bit payloads beyond the current defensive policy.
- `TODO_VERIFY`: actor ID 1000+, inactive slots, and null
  ActorPool/CActor/CPed/stale-reference branches.
- `TODO_VERIFY`: live RPC175 kill-switch transition and shadow-mode readback.
- `TODO_VERIFY`: visual/current-rotation timing after only
  `CPed+0x55c` is written.
