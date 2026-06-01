# SA-MP DLL Clean-Room Workflow

This repository is organized to support a clean-room rebuild of `samp.dll` without copying original implementation code.

## Roles

1. Team A (Reverse Analysis)
2. Team B (Reimplementation)

`Team A` can inspect binaries, disassembly, strings, runtime behavior, and protocol traces.
`Team B` only consumes derived specifications, tests, and behavior docs produced by `Team A`.

## Repository Layout

- `artifacts/`: Original binaries and extracted payloads.
- `analysis/`: Reverse-engineering notes and machine-generated metadata.
- `specs/`: Behavior/API/protocol specifications intended for Team B.
- `reimpl/`: New source code implementation and tests.
- `tools/`: Repeatable scripts for metadata extraction and diffing.

## Clean-Room Rules

1. Do not copy decompiled/disassembled code into `reimpl/`.
2. Team B should not open raw disassembly or decompiler output.
3. All work items in `reimpl/` must reference a spec ID from `specs/`.
4. For each spec, record source artifacts (offsets, traces, test cases).
5. For each implementation, record which spec IDs it satisfies.
6. Prefer black-box behavior tests (I/O, protocol packets, UI behavior).

## Suggested Delivery Pipeline

1. Baseline capture: run `tools/collect_pe_baseline.sh samp.dll sa-mp-0.3.7-R5-1-install.exe`.
2. Mapping refresh: run `tools/generate_re_map.sh samp.dll analysis/generated` to regenerate function/import/xref crosswalk artifacts.
3. Feature inventory: build a subsystem list (networking, rendering hooks, audio, GUI, config, updater).
4. Spec writing: write behavior-first specs under `specs/` (inputs, outputs, errors, constraints).
5. Reimplementation: build modules in `reimpl/` from specs only.
6. Validation: compare runtime behavior against original binary with automated tests and trace diffing (`tools/compare_runtime_traces.sh <ref.log> <cand.log>`).

Project roadmap spec:

- `specs/SPEC-PLAN-001_FULL_REBUILD_ROADMAP.md`

## IPv6 Migration Strategy

1. Document current IPv4-only behavior (resolver, socket creation, connect flow, error handling).
2. Keep compatibility with existing IPv4 servers.
3. Add dual-stack path using modern APIs (`getaddrinfo`, `sockaddr_storage`, `AF_UNSPEC` fallback strategy).
4. Add integration tests for IPv4 literals, DNS A/AAAA, IPv6 literals, and timeout/retry behavior.
