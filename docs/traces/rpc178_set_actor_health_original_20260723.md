# Original 0.3.7-R5 RPC178 `SetActorHealth` Golden Trace (2026-07-23)

## Capture identity

- Run ID: `20260723_190924_rpc178_original_actor_only_1644967d`
- Client host: native Windows lab `sshadmin@192.168.3.180`;
  interactive session `DESKTOP-7PGINHC\defaultusr`
- GTA root (native Windows, no Wine prefix):
  `C:\Program Files (x86)\Rockstar Games\GTA San Andreas`
- Server fixture: `192.168.3.181:7778`, nickname `OrigActor`
- Original `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- GTA SA 1.0 US SHA256:
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`
- Probe profile: Actor hooks enabled, Actor-heavy hooks disabled; all seven
  guarded Actor RPC hooks installed
- Fetched probe log:
  `/tmp/samp-runs/20260723_190924_rpc178_original_actor_only_1644967d/logs/samp_probe.log`
- Probe-log SHA256:
  `6de8ce1e49ecf9c2047c22b0f5379844a1d09d0fc7dcbe2d2e02fa0264dc6036`

## Scenario

The client connected to the fixture and reached class selection. Sending
`/rpcactors` there produced no Actor RPCs, as expected for the unspawned test
state. The Spawn button was then clicked at client coordinates `(500, 530)`,
followed by `/rpcactors` in the spawned state.

`OBSERVED_037 + PROBE_TRACE`: the spawned command produced the complete Actor
fixture lifecycle: four RPC171 creates, RPC173 animation, RPC174 clear plus
RPC176 position and RPC178 health updates, RPC175 facing updates, and RPC172
removal. All begin records in the first lifecycle had matching end records
with `original_called=1`.

## First-cycle RPC178 payloads

`OBSERVED_037 + PROBE_TRACE`: each handler input was readable, exactly 48 bits
(six bytes), and the original R5 handler returned through the probe trampoline
exactly once.

| Actor | Health | Float bits | Payload bytes | Begin/end log lines |
| ---: | ---: | ---: | --- | --- |
| 0 | `100.0` | `0x42c80000` | `00 00 00 00 c8 42` | `82013` / `82014` (`seq=11`) |
| 1 | `85.0` | `0x42aa0000` | `01 00 00 00 aa 42` | `82019` / `82020` (`seq=14`) |
| 2 | `70.0` | `0x428c0000` | `02 00 00 00 8c 42` | `82025` / `82026` (`seq=17`) |
| 3 | `55.0` | `0x425c0000` | `03 00 00 00 5c 42` | `82031` / `82032` (`seq=20`) |

The same four payloads repeated later at lines `82142`-`82161`
(`seq=51,54,57,60`), again with `original_called=1`.

`STATIC_037`: the matching handler and downstream method reconstruction is
documented in
[rpc178_set_actor_health_static_20260723.md](../re/rpc178_set_actor_health_static_20260723.md).

## Run health and shutdown

`PROBE_TRACE`:

- 68 Actor `phase=begin` records had 68 matching `phase=end` records.
- No Actor end record had `original_called=0`; no missing-trampoline marker or
  `exception_filter` record occurred.
- Normal `/q` shutdown reached `closesocket` at line `82282` and
  `WSACleanup` at line `82297`, after which the native GTA process exited.
- The probe emitted no explicit `process_detach` record, so shutdown evidence
  is the socket cleanup plus observed process exit, not a detach callback.

## Excluded run

Run `20260723_190012_rpc178_original_golden_9fb20936` is not golden evidence.
It used the Actor-heavy profile, became non-responsive at the splash/startup
state, and produced no `connect`, `sendto`, or Actor RPC record. Its probe log
also contains no `exception_filter`, so this capture does not establish a
client crash or assign the stall to a specific hook. It is retained only as a
failed-run diagnostic and must not be compared against overlay shadow/replace
behavior.
