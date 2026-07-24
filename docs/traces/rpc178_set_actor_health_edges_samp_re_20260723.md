# RPC178 `samp_re` health-edge parity (2026-07-23)

## Capture identity

- Replacement run:
  `20260723_215016_rpc178_edge_replace_clean_77642b54`
- Native Windows lab: `sshadmin@192.168.3.180`, interactive user
  `DESKTOP-7PGINHC\defaultusr`
- Server fixture: `192.168.3.181:7778`, nickname `Rpc178Repl`
- Original reference:
  [rpc178_set_actor_health_edges_original_20260723.md](rpc178_set_actor_health_edges_original_20260723.md)
- Original `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- `samp_re.asi` SHA256:
  `c17791b13a28799fdfa9204528bf26fb06ed346b4cf56fe036419716bed7ee2e`
- Fixture `bare.amx` SHA256:
  `52ea0e048fd8b8312ce70379a553bf0a97a0366b0229d87461bb0e0abe5c83a1`
- Fetched run:
  `/tmp/rpc178-edge-replace-final/20260723_215016_rpc178_edge_replace_clean_77642b54`
- Final `manifest.json` SHA256:
  `8787a5688d8fd5dce997fb5bd383d85076f0b36a4f04dc6700f597c469bf8c99`
- Final `samp_re.log` SHA256:
  `647ca2de57db19229a5518dac51c30e72bceaddfa087ba3ae607ace7056557bb`
- Final `samp_probe.log` SHA256:
  `f1356e6c22fcac953bc8f708bef6a236b2cfcfc60f793470cb8075d9114b0a1d`

The manifest records both requested and effective `samp_re` mode as `replace`,
with only `rpc178_set_actor_health` enabled and both disable mechanisms off.
The overlay accepted the exact R5 identity, the guarded seven-byte prologue at
`samp.dll+0x0001dbe0`, and the downstream method at
`samp.dll+0x0009c5d0` before installing its replacement hook. The passive
probe intentionally ran with `actor_hooks=0`, leaving the RPC178 entry under
the overlay's sole ownership.

## Five-case comparison

The server log at lines `52144`-`52164` records five distinct actors, all five
stream-in callbacks and mask `0x1f`, successful application of every fixed
payload, matching server-side raw-bit readback, and destruction of all five
actors.

`OBSERVED_037 + PROBE_TRACE`: the original golden trace and the clean
replacement run produced the following immediate `CPed+0x540` values:

| Original seq | Replacement seq | Actor | Case | Original stored | Replacement input | Replacement stored | Original handler calls |
| ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 6 | 1 | 0 | positive infinity | `0x7f800000` | `0x7f800000` | `0x7f800000` | 0 |
| 7 | 2 | 1 | quiet NaN with payload | `0x7fc01234` | `0x7fc01234` | `0x7fc01234` | 0 |
| 8 | 3 | 2 | positive zero | `0x00000000` | `0x00000000` | `0x00000000` | 0 |
| 9 | 4 | 3 | negative zero | `0x80000000` | `0x80000000` | `0x80000000` | 0 |
| 10 | 5 | 4 | negative 25 | `0xc1c80000` | `0xc1c80000` | `0xc1c80000` | 0 |

The five replacement records are `samp_re.log` lines `8`-`12`. Every one
logs `result=applied`, `readback=1`, `health_match=1`, and
`original_calls=0`; no other RPC178 record occurs in that log.

`OBSERVED_037 + PROBE_TRACE + STATIC_037`: for these valid, exactly 48-bit
requests to streamed active actors, the replacement RPC handler has exact
raw-bit parity with R5. This includes preservation of the NaN payload and the
sign bit of zero. The original RPC178 handler was suppressed, while the
replacement still invoked the original downstream `CActor::SetHealth` method;
this is handler-level parity, not yet an OSS replacement of that downstream
method.

## Shutdown health

`PROBE_TRACE`:

- The server recorded a normal client quit at log lines `52168`-`52169`.
- The passive probe reached successful `closesocket` at line `6298` and
  successful `WSACleanup` at line `6314`.
- The final collection lists only the responsive SA-MP browser; the GTA
  process had exited. Its dump list is empty, and `application_events.tsv`
  contains no event.
- Neither final log contains an `exception_filter`, fallback, failed
  readback, health mismatch, ownership-loss, or unexpected original-handler
  call marker.
- No explicit `process_detach` marker was emitted, so shutdown is established
  by socket cleanup, GTA exit, and the empty dump/event evidence.

## Excluded run and remaining limits

The earlier actor-probe-conflict pre-run was aborted and is excluded; it
supplies no parity evidence.

- `TODO_VERIFY`: malformed bit lengths, actor ID 1000+, inactive slots, and
  null pool/actor/CPed branches remain outside this comparison.
- `TODO_VERIFY`: the external side effects of the original nonpositive helper
  at `samp.dll+0x000b2310` were not separately instrumented.
