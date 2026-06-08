# SPEC-PLAN-001: Full Clean-Room Rebuild Roadmap for `samp.dll`

Status: Draft  
Owner: Team A + Team B  
Date: 2026-03-05

## 1. Goal

Deliver a clean-room `samp.dll` rebuild that is behavior-compatible with the current reference binary while being maintainable, testable, and extensible.

Target reference:

- `path/to/original/samp.dll`
- SHA-256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`

## 2. Non-Negotiable Constraints

1. Team B must not consume raw disassembly/decompiler text.
2. Team B implements from `specs/` only.
3. Behavior parity is measured black-box first, then ABI/PE structure.
4. No code copying from old or current proprietary client binaries into `reimpl/`.

## 3. Workstreams

1. `WS-A`: Binary mapping and behavioral spec extraction (Team A).
2. `WS-B`: Reimplementation modules in `reimpl/` (Team B).
3. `WS-C`: Differential test harness (A/B integration).
4. `WS-D`: ABI/PE packaging parity and loader compatibility.

## 4. Phase Plan

### Phase 0: Baseline Freeze and Tooling

Deliverables:

1. Frozen reference hashes and installer extraction report.
2. Automated mapping pipeline (`tools/generate_re_map.sh`).
3. ABI parity check script baseline.

Exit criteria:

1. Deterministic metadata regeneration in CI/dev shell.
2. Baseline docs updated on every mapping refresh.

### Phase 1: Boot/Lifecycle Parity

Deliverables:

1. Stable attach/detach/thread dispatch parity (`SPEC-ABI-001`).
2. Launch-monitor style background thread behavior parity.
3. Rollback/teardown semantics on failed init stages.

Exit criteria:

1. Rebuild loads/unloads repeatedly without deadlocks/leaks.
2. Attach/detach sequence matches expected event order in trace harness.

### Phase 2: Network Core Parity (IPv4-compatible baseline)

Deliverables:

1. Reference-compatible resolver/connect/reconnect behavior.
2. Mapped UDP send/recv semantics from the `0x10053***` cluster.
3. Mapped listen/accept/control flow from the `0x10056880` cluster.

Exit criteria:

1. Connect/loss/reconnect behavior matches reference timelines.
2. Packet send/recv error and retry semantics pass diff tests.

### Phase 3: Network Modernization (Dual Stack)

Deliverables:

1. `SPEC-NET-001` and `SPEC-NET-002` full implementation in runtime path.
2. IPv6 literal + A/AAAA fallback + deterministic candidate iteration.
3. Compatibility gate to preserve old IPv4 UX/messages.

Exit criteria:

1. All parser + connect matrix tests pass (`IPv4`, `IPv6`, `DNS`, failover).
2. No regression in IPv4-era server connectivity.

### Phase 4: Render/UI/Audio Subsystems

Deliverables:

1. D3DX wrapper call-chain parity for high-traffic render functions.
2. USER32/GDI/UI state flow parity (window/input/menu/chat overlays).
3. BASS init/config/playback lifecycle parity.

Exit criteria:

1. Overlay/UI appears and updates at expected lifecycle points.
2. Audio init/play/stop behavior matches reference transitions.

### Phase 5: Gameplay Hook and Runtime Integration

Deliverables:

1. Hook installation chain parity (pre-game and in-game phases).
2. Script/RPC registration lifecycle parity.
3. Netgame state machine parity (`WAIT_CONNECT`, `CONNECTING`, `CONNECTED`, reconnect).

Exit criteria:

1. Equivalent state transitions under scripted session tests.
2. No missed critical callbacks/events in trace comparison.

### Phase 6: ABI/PE and Packaging Parity

Deliverables:

1. PE headers/sections/import-profile alignment (within accepted deltas).
2. Loader compatibility with SA-MP launcher injection path.
3. Reproducible Win32 build artifact pipeline.

Exit criteria:

1. `tools/check_pe_abi_parity.sh` passes agreed parity thresholds.
2. Produced `samp.dll` is accepted by launcher/runtime harness.

### Phase 7: Hardening and Release Readiness

Deliverables:

1. Soak/stability runs with reconnect loops and subsystem churn.
2. Crash telemetry hooks and guardrail assertions.
3. Release build profile and rollback strategy.

Exit criteria:

1. No crash/leak regressions in soak scenarios.
2. Tagged release candidate with full evidence bundle.

## 5. Execution Backlog (Concrete Next Tasks)

1. Complete conversion of mapped network cluster into formal `SPEC-NET-00x` docs (now covered by `SPEC-NET-003`, `SPEC-NET-004`, `SPEC-NET-005`, `SPEC-NET-006`; remaining depth: async worker queue internals and `10056300` owner teardown chain).
2. Add runtime behavior tests for:
   - connect timeout and retry cadence,
   - reconnect message/state sequence,
   - socket close/cleanup ordering.
3. Expand `reimpl/src/runtime_bridge.c` to explicit phase callbacks mirroring mapped chain.
4. Integrate maintained RakNet variant backend (`SPEC-NET-005`) as primary compatibility candidate.
5. Create subsystem stubs in `reimpl/src/`:
   - `net/socket_compat.c`
   - `render/d3dx_bridge.c`
   - `audio/bass_bridge.c`
6. Expand runtime trace diff harness (`tools/compare_runtime_traces.sh`) from keyword-sequence compare to subsystem-specific state-transition assertions.

## 6. Build and Validation Loop

Host tests:

```bash
cmake -S reimpl -B build-host -G Ninja -DSAMPDLL_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Win32 build:

```bash
reimpl/scripts/build_win32.sh
tools/check_pe_abi_parity.sh samp.dll build-win32/samp.dll
```

Mapping refresh:

```bash
tools/generate_re_map.sh samp.dll analysis/generated
```

## 7. Definition of Done (Project Level)

Project is done when all are true:

1. Rebuild is loadable and stable in real launcher-injected workflow.
2. Functional parity is achieved for boot, network, render/UI, and audio critical paths.
3. ABI/PE parity is within agreed tolerance and documented.
4. All implemented behavior traces back to clean-room specs.
5. Build and verification are reproducible from repository scripts.
