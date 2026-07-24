# Original 0.3.7-R5 RPC178 health-edge golden trace (2026-07-23)

## Capture identity

- Run ID: `20260723_213551_rpc178_edge_original_8ba9d540`
- Client: native Windows lab `sshadmin@192.168.3.180`, interactive user
  `DESKTOP-7PGINHC\defaultusr`
- Server fixture: `192.168.3.181:7778`, nickname `Rpc178Orig`
- Original `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- Probe artifact SHA256:
  `5252f62aee90c56ec00267b769827c774631130c27740600e635966c0e47f96e`
- Fixture `bare.amx` SHA256:
  `52ea0e048fd8b8312ce70379a553bf0a97a0366b0229d87461bb0e0abe5c83a1`
- `samp_re.asi` SHA256:
  `c17791b13a28799fdfa9204528bf26fb06ed346b4cf56fe036419716bed7ee2e`;
  its run profile and effective mode were both `bypass`
- Fetched run:
  `/tmp/rpc178-edge-original-final/20260723_213551_rpc178_edge_original_8ba9d540`
- Final `manifest.json` SHA256:
  `e71955d357f54c805261acb07ee4bc788af256b6dffd2a4ef11edd93de535084`
- Final `samp_probe.log` SHA256:
  `d2fccd7bd76ee13c42d8f4a72f6d885f4f1e0c3d0a9d7761a65d7414dbccfc`
- Final `samp_re.log` SHA256:
  `b88fbf995abc10fccdac8d9f500db61f807b3af02bb10ea1bb621c12fe158859`

The probe used the Actor profile (`actor_hooks=1`, `actor_heavy=0`). Its
relocation check for `samp.dll+0x0001dbe0` passed, all seven guarded Actor
hooks passed the set preflight, and the RPC178 hook installed with a
seven-byte trampoline. The static handler and downstream method are documented
in
[rpc178_set_actor_health_static_20260723.md](../re/rpc178_set_actor_health_static_20260723.md).

## Successful spawned fixture

After the player spawned, `/rpc178edge` created five distinct actors around the
player. The server log at lines `52061`-`52082` records all five stream-in
callbacks, the complete mask `0x1f`, five successful `SetActorHealth` calls,
matching server-side raw-bit readbacks, the hold phase, and destruction of all
five actors.

`OBSERVED_037 + PROBE_TRACE`: the original client received exactly five
RPC178 calls in the run. Each request had a readable, exactly 48-bit payload;
the target ActorPool slot was active, the CActor and CPed pointers were
readable, and the original trampoline returned exactly once. The immediate
post-handler readback from `CActor+0x48 -> CPed+0x540` matched all 32 request
bits:

| Sequence | Actor | Case | Payload bytes | Requested bits | Stored bits |
| ---: | ---: | --- | --- | ---: | ---: |
| 6 | 0 | positive infinity | `00 00 00 00 80 7f` | `0x7f800000` | `0x7f800000` |
| 7 | 1 | quiet NaN with payload | `01 00 34 12 c0 7f` | `0x7fc01234` | `0x7fc01234` |
| 8 | 2 | positive zero | `02 00 00 00 00 00` | `0x00000000` | `0x00000000` |
| 9 | 3 | negative zero | `03 00 00 00 00 80` | `0x80000000` | `0x80000000` |
| 10 | 4 | negative 25 | `04 00 00 00 c8 c1` | `0xc1c80000` | `0xc1c80000` |

The matching probe records are lines `41383`-`41399`. All five readbacks log
`readback=1`, `health_match=1`, `reason=ok`, and `original_called=1`; all five
begin records have matching end records. No other RPC178 call occurred in the
final probe log.

`OBSERVED_037 + PROBE_TRACE + STATIC_037`: this establishes that the original
R5 path preserves these special IEEE-754 bit patterns in `CPed+0x540`,
including the NaN payload and the sign bit of zero. For zero, negative zero,
and negative 25, the stored raw value was still intact after the statically
identified nonpositive path returned. The external effect and semantic purpose
of the helper at `samp.dll+0x000b2310` were not instrumented and remain
`TODO_VERIFY`.

## Shutdown health

`PROBE_TRACE`:

- Normal quit reached successful `closesocket` at probe-log line `41768` and
  successful `WSACleanup` at line `41783`.
- The final collection lists only the responsive SA-MP browser process; the
  GTA process had exited. Its dump list is empty, and the captured Application
  event table contains no event.
- No `exception_filter`, missing-trampoline, unmatched RPC178 end, or
  `original_called=0` marker occurs in the final probe log.
- As in the earlier positive-health golden run, the probe emitted no explicit
  `process_detach` record. Shutdown is therefore established by socket cleanup,
  GTA process exit, and the empty dump/event evidence, not by a detach callback.

## Explicitly excluded pre-spawn attempt

The earlier `/rpc178edge` invocation at server-log lines `52042`-`52049`
occurred while the client was still in class selection. Its actors were placed
around `(0, 0, 0)`, no actor streamed in (`mask=0x00`), the fixture timed out,
and `applied=0`; the client received no RPC178 from that attempt. It is a
fixture-gating diagnostic only and is not part of this golden sample.

## Remaining limits

- `TODO_VERIFY`: the helper at `samp.dll+0x000b2310` needs its own runtime
  call/side-effect observation.
- `TODO_VERIFY`: malformed bit lengths, actor ID 1000+, inactive slots, and
  null pool/actor/CPed branches are outside this capture.
- A replacement comparison must use the same five fixed payloads and require
  exact raw-bit readback, not ordinary floating-point equality.
