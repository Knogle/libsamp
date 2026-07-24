# RPC175 rebuilt-DLL raw-angle burst parity (2026-07-24)

## Capture identity

- Passing run:
  `20260724_000122_rpc175_raw_reimpl_burstfix_89564604`
- Native Windows lab: `sshadmin@192.168.3.180`, interactive user
  `DESKTOP-7PGINHC\defaultusr`
- Isolated server fixture: `192.168.3.181:7778`, nickname
  `Rpc175Burst`
- Rebuilt `samp.dll` SHA256:
  `10ef0beb21682a3ab8f1b133a8e2afc2ab7bebb0172f43861bb6907dfca8c401`
- Original 0.3.7-R5 reference SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- `samp_re.asi` SHA256:
  `3cb3a4011026aba37464707502f6902680170646535df2e66041280422eae24d`;
  requested and effective mode were both `bypass`
- Passive probe artifact SHA256:
  `edabd095a121cd3ed5f0fe014f34a10278d1b0f1f8644456589ae1df03b2f3f7`
- Fixed raw-RPC component SHA256:
  `f2a5d0632f43f7cb8b73e9eeaf1cb67e65c57b53690536d956fbbca3616951af`
- Fixture `bare.amx` SHA256:
  `b913530f13e4ea570a02295ac47fae57e4c5cd851b2439e16754dbdc64040185`
- Fetched passing run:
  `/tmp/rpc175-burstfix-final/20260724_000122_rpc175_raw_reimpl_burstfix_89564604`
- Final artifact hashes:

  ```text
  manifest.json       0e189438fbdb8eef4da9e96e77fcbd5b603e141e3ccdbbc722ff457ce3a5b11f
  samp_runtime.log    37d389067d49fd642caf71dd5ac6a2493ced950fc364ca9063e80538b867a1d8
  samp_net_trace.log  345b8b1196fbc645789b31cecc47c88e4797502378b18ef59180ba9d24e08901
  samp_probe.log      2e31cbce364ed75e52f455da5aa584f4884865204029f779dc4d3de99131b322
  samp_re.log         fab0dc78dce9afcafe594324f88976cca8e85ad5e3267ca2b7ca8c657de64a40
  ```

The original-client contract is documented in
[rpc175_set_actor_facing_raw_original_20260723.md](rpc175_set_actor_facing_raw_original_20260723.md).
The handler, downstream method, target field, and x87 conversion path are
documented in
[rpc175_set_actor_facing_static_20260723.md](../re/rpc175_set_actor_facing_static_20260723.md).

## Discovered rebuilt-DLL burst defect

`PROBE_TRACE`: the first rebuilt-DLL replay used candidate
`11cd69831b80bc78ec1735a3ccc7e61e698f6fc6a608f8f8ad85f6103a60e097`
in run `20260723_233812_rpc175_raw_reimpl_e6532e31`. Its network adapter
retained all nine contiguous inputs as actor-event revisions 113 through 121
at `samp_net_trace.log` lines `9525`-`9541`, but the launch-monitor thread
folded them into one desired actor state before the graphics thread ran.
Only revision 121/qNaN reached the physical helper at `samp_runtime.log`
line `1344`.

That run therefore proved correct decoding and final-state conversion but
failed per-handler call parity. Its fetched logs are under
`/tmp/rpc175-raw-reimpl-final/20260723_233812_rpc175_raw_reimpl_e6532e31`;
the final manifest/runtime/net-log SHA256 values are respectively
`6283bc6797922e73813dfc972093ddf126d47b6e758d1d0dffb3f66eeb816c96`,
`c91520e83f2c9ff14f6cf94180e45bd85ab242e76e810d1f347cd8ea47e69113`,
and `57ecd9e100e74029a7a2d7fd5da1bb4fb6e67c7ab5b13f3477b191a9d8ece1a7`.

## Bounded ordered-history fix

`OBSERVED_037 + PROBE_TRACE + STATIC_037`: the rebuilt bridge now retains a
private 128-entry raw RPC175 history per actor, matching the maximum actor
event window exposed by one adapter snapshot. Each entry contains the actor
event revision and the untouched 32 angle bits. The graphics thread consumes
every known entry in order and calls the physical conversion/write helper
once per entry. It advances the history cursor even if the original-equivalent
ped guard or readback fails, so an RPC is never retried across frames.

The latest revision and latest raw bits remain a separate authoritative tail.
They are used only for resync/fallback and are compared together; this avoids
both a duplicate ninth call and same-revision errors involving signed zero or
different NaN payloads. A consumer lag beyond the bounded history is logged as
`facing_history_gap` and falls back explicitly to that tail.

No public include, export, calling convention, protocol structure, or PE
export directory changed.

## Nine-case passing result

The component logged one trigger and nine successful fixed sends at server
time `2026-07-24 00:06:00`. The rebuilt adapter recorded the exact sequence
as revisions 49 through 57 at final `samp_net_trace.log` lines
`3719`-`3736`. Final `samp_runtime.log` lines `1037`-`1045` contain exactly
nine corresponding physical applications:

| Revision | Case | Input bits | Stored `CPed+0x55c` bits | Readback |
| ---: | --- | ---: | ---: | --- |
| 49 | positive zero | `0x00000000` | `0x00000000` | exact |
| 50 | negative zero | `0x80000000` | `0x80000000` | exact |
| 51 | 180 degrees | `0x43340000` | `0x40490fdb` | exact |
| 52 | 360 degrees | `0x43b40000` | `0x80000000` | exact |
| 53 | negative 45 | `0xc2340000` | `0x00000000` | exact |
| 54 | 540 degrees | `0x44070000` | `0x00000000` | exact |
| 55 | positive infinity | `0x7f800000` | `0x00000000` | exact |
| 56 | negative infinity | `0xff800000` | `0x00000000` | exact |
| 57 | quiet NaN with payload | `0x7fc01234` | `0x7fc01234` | exact |

Every line reports `conversion_attempted=1`,
`source=ordered_rpc175_history`, and `confirmed=1`. There is no second
revision-57 application. No `facing_history_gap`, `event_gap`,
`authoritative_create_heading_fallback`, or unconfirmed facing readback occurs
in the run.

This matches the original run's nine immediate writes, including operation
order and GTA's observed PC24 x87 result.

## Build, regression, and shutdown checks

- Clean host CTest passed 9/9.
- The RPC175 unit test covers the golden conversions, finite/nonfinite
  classification, RPC171/RPC175 state separation, authoritative resync, the
  ordered nine-event history, and explicit history-overflow detection.
- Strict i686 MinGW compilation with `-Wall -Wextra -Werror` succeeded.
- The resulting PE32 test ran under Wine and printed
  `All RPC175 actor-facing parity checks passed`.
- The full devbuild-toolbox DLL build succeeded.
- An independent bounded review found no defect in the active-actor,
  one-snapshot, nine-event case. It measured a safe 1,868-byte
  `actor_compat_process_game_thread` stack frame.
- `git diff --check` passed for the touched files.
- The final Windows collection lists only the responsive SA-MP browser after
  GTA exited; `dumps=[]` and the Application event table is empty.
- The passive probe recorded successful `closesocket` calls at lines
  `1444`-`1445` and successful `WSACleanup` calls at lines `1448`-`1450`.
- No exception filter, access violation, fatal marker, connection-loss marker,
  or socket error occurs.

## Deliberate limits and next actor-bridge work

The ordered history is a small, observed-case fix, not a claim that the whole
deferred actor bridge is now globally call-order equivalent.

- `TODO_VERIFY`: more than 128 unconsumed RPC175 events are reduced to an
  explicitly logged authoritative tail because the current public adapter
  snapshot is itself bounded.
- `TODO_VERIFY`: a Create/Facing/Destroy or Destroy/Create/Facing sequence
  folded entirely before the graphics tick can still lose old-lifetime
  intermediate calls.
- `TODO_VERIFY`: authoritative event-ring resync currently preserves final
  state, not every known command as a lifetime barrier.
- `TODO_VERIFY`: physical processing is actor-ID ordered, so cross-actor RPC
  call order is not yet guaranteed.
- A lifecycle-aware private actor command FIFO covering Create, Destroy,
  Facing, Position, Health, and Animation is the follow-up architecture for
  full actor-RPC ordering parity.
- Signaling NaNs, subnormals, boundary-adjacent inputs, malformed payloads,
  inactive/null slot guards, and actor IDs 1000+ remain separate original-DLL
  probe cases.
