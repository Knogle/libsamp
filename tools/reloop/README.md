# reloop: unattended SA-MP compatibility runs

`reloop` drives the existing bare open.mp server and `test_cmds` fixture through
the original and replacement Lutris prefixes. It records each run beneath
`artifacts/runs/` without truncating the append-only client or server logs.

## Quick start

```bash
tools/reloop/reloop doctor
tools/reloop/reloop build
tools/reloop/reloop install-device-helper
tools/reloop/reloop run --client replacement --group all
tools/reloop/reloop matrix --group all
```

Add `--interaction` to `run` or `matrix` to install
`reloop_control.asi` and drive the T/chat and TAB/scoreboard state machine.
The run then contains `ui-interaction-states.json`, which can be reduced to
explicit checks with:

```bash
python3 tools/reloop/analyze_interaction.py artifacts/runs/<run>
```

`--companion` additionally launches `ReLoopPeer` from the other prefix for
RPC23 scoreboard-click work. On Wayland the second Wine window may steal
effective focus, so companion runs are not freeze/camera evidence unless the
primary trace proves that T and TAB became active.

Crash verdicts use a three-attempt policy by default. A single
`PRECONNECT_CRASH` or `RUNTIME_CRASH` is retried automatically with the same
scenario; only three consecutive crash attempts are treated as reproducible.
Every attempt keeps its own artifact and the final attempt contains
`retry-series.json`. `--crash-attempts N` may raise the limit but never lowers
it below three.

The default profiles are:

- original: `/home/chairman/Games/san-andreas-multiplayer-legacy-legacy`
  (Lutris game ID 55);
- replacement: `/home/chairman/Games/san-andreas-multiplayer-legacy-libsamp`
  (Lutris game ID 57).

`reloop run` compiles `test_cmds`, builds/deploys the replacement DLL when the
replacement profile is selected, restarts only the exact configured bare-server
executable, and launches `samp.exe` through
`GE-Proton10-34/files/bin/wine` with the exact selected prefix. It passes
`127.0.0.1:7778 -nReLoop` to the SA-MP launcher; Lutris is not involved at run
time.
The configured Lutris IDs/YAML files are optional diagnostics only.

Before every run, the MIT-licensed helper supplied at
`/home/chairman/Downloads/1483123471_SkipDeviceSelection/SkipDeviceSelection.asi`
is hash-verified and installed in the selected GTA root. Its only behavior is
to press Enter when GTA creates the `Device Selection` window. The standalone
`install-device-helper` command installs the same verified binary in both test
prefixes.

The server-side request is one line in
`scriptfiles/test_cmds_request.txt`:

```text
<request_id> <all|vehicle|player|pvars|ui|labels> <delay_ms> <autospawn 0|1> <player_name|*>
```

The filterscript consumes this file once and emits request-scoped markers:
`REQUEST_ACCEPTED`, `AUTO_SPAWN`, `RUN_START`, `RUN_DONE`, or `RUN_ABORT`.
Interactive `/tbatch` and `/testcmds ...` use remain available and have
`request=0`.

## Safety and lifecycle

- `--client-mode replace` (default) stops only processes whose environment
  contains the exact configured Wine prefix. `--client-mode fail` leaves them
  untouched and aborts the run.
- `--server-mode replace` (default) stops only a process whose executable is
  exactly `omp-server-bare/omp-server`.
- `--server-mode fail` never stops an existing server.
- `--server-mode reuse` attaches to an already running server; use this only
  when the newly compiled filterscript is already loaded.
- The client process group and only newly created processes carrying the exact
  configured prefix in their environment are stopped after the result marker.
- `--keep-client` leaves the client running for manual visual inspection.
- `--screenshots` captures the desktop at each `OBSERVE` record when
  `gnome-screenshot` or `grim` is available. Screenshots are evidence, not yet
  automatic visual parity verdicts. A portal/backend timeout disables further
  captures for that run instead of blocking the state machine.

## Verdicts

- `PASS`: all server-verifiable checks passed.
- `PASS_WITH_WARNINGS`: checks passed, with visual observations or a crash only
  after `RUN_DONE`.
- `INFRA_FAIL`: launch/connect/spawn did not reach a test marker.
- `PRECONNECT_CRASH`: the client exited before `RUN_START`.
- `RUNTIME_CRASH`: the client exited or logged an exception after start.
- `PROTOCOL_MISMATCH`: a started run aborted or timed out.
- `STATE_MISMATCH`: `test_cmds` produced a failing result.
- `OPCODE_PARITY_UNVERIFIED`: state signatures match, but the selected
  scenarios do not yet have complete original handler/GTA-opcode trace tuples.

Each artifact contains `metadata.json`, `events.jsonl`, `verdict.json`, a short
`summary.md`, isolated server output, request-scoped test records, and only the
bytes appended to client logs during that run.

## Opcode parity contract

Matching server state is necessary but not sufficient. Compatibility requires
the same incoming RPC and, where the original handler delegates to GTA script,
the same GTA opcode path as SA-MP 0.3.7. Matrix output therefore reports a
separate `state_verdict` and `opcode_path_verdict`; a state-clean comparison is
`OPCODE_PARITY_UNVERIFIED` until original probe/static evidence covers its
opcode paths. See `docs/re/reloop_opcode_parity_contract_20260718.md`.

## Evidence scope

- `OPENMP_REF`: request polling and auto-spawn use the documented `fopen`,
  `fread`, `SetTimerEx`, `OnPlayerConnect`, `OnPlayerSpawn`, `SetSpawnInfo`,
  `SpawnPlayer`, and `IsPlayerSpawned` APIs.
- `INFERRED`: the default 1500 ms auto-spawn and 1000 ms post-spawn delay are
  deterministic fixture timing, not claims about original SA-MP behavior.
- Client compatibility verdicts still require `PROBE_TRACE` and visual review
  for `OBSERVE` steps; server-side state alone is not visual parity proof.
