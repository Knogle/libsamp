# SPEC-PLAN-002: Binary Compatibility Strike Plan for `samp.dll`

Status: Draft
Owner: Reverse Analysis Cell + Reimplementation Cell
Date: 2026-03-09

## Current Operating Mode

Current project mode is compatibility-first.

Priority order for the current phase:

1. reproducibly build a fully ABI-compatible `samp.dll`,
2. ground behavior in original 0.3.7-DLL reverse engineering, Golden Traces, and public references,
3. keep provenance explicit so public-release hardening is part of the normal implementation workflow.

## 1. Mission

Deliver a `samp.dll` replacement that is accepted by the original launcher/runtime path and is ABI-compatible at three layers:

1. PE/loader ABI: entry RVA, section table, import surface, resources, relocations, stack/heap, subsystem, image base.
2. Runtime ABI: `DllMain` reason flow, thread creation/destruction, exception/filter behavior, hook installation order, socket/UI/render/audio lifecycles.
3. Behavioral ABI: same externally visible state transitions, messages, hooks, timings, reconnect behavior, and integration side effects.

This is not achieved by "roughly loadable". It is achieved only when the candidate survives the same loader, the same hook points, and the same scenario matrix as the reference binary.

## 2. Current Assessment

### Strengths Already Present

1. A clean-room workflow already exists and remains available if needed later.
2. Baseline artifacts, extracted installer payloads, and mapping scripts already exist.
3. The current binary map is non-trivial: `analysis/generated/RE_MAP_SUMMARY.md` reports 3935 analyzed functions.
4. Network reverse work has real traction: the `0x100538**` and `0x100568**` clusters are partially mapped and have clean-room specs.
5. Runtime trace trimming and baseline comparison already exist, which is better than guessing.

### Why The Current Effort Is Still Insufficient

1. The current candidate matches only coarse PE identity, not hard ABI shape.
   - Reference size: 1,204,224 bytes.
   - Candidate size: 582,158 bytes.
   - Reference entry RVA: `0x000cbc90`.
   - Candidate entry RVA: `0x000011f0`.
   - Reference file alignment: `0x1000`.
   - Candidate file alignment: `0x0200`.
2. The section table is materially wrong.
   - Reference: `.text`, `.rdata`, `.data`, `.rsrc`, `.reloc`.
   - Candidate: `.text`, `.data`, `.rdata`, `.eh_frame`, `.bss`, `.idata`, `.tls`, `.reloc`.
3. The import surface is materially wrong.
   - Reference imports 10 DLL families including `USER32.dll`, `COMCTL32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `PSAPI.DLL`, `WINMM.dll`, `WSOCK32.dll`, `d3dx9_25.dll`, `BASS.dll`.
   - Candidate imports only `GDI32.dll`, `KERNEL32.dll`, `WS2_32.dll`, `msvcrt.dll`, `libwinpthread-1.dll`.
4. Runtime parity is still missing visible lifecycle behavior.
   - `analysis/generated/runtime_baseline_bigone/BASELINE.md` shows one `PROCESS_DETACH` for `samp.dll`.
   - The current candidate trace reports zero `PROCESS_DETACH` calls.
   - The baseline shows 51 "Connecting to ..." banners and 17 "Connected to ..." banners; the candidate currently shows none in the captured runs.
5. The current runtime trace harness can produce false comfort.
   - It checks process-global module loads and API presence.
   - Example: `d3dx9_25.dll` and `BASS.dll` can appear in the process even when the candidate PE does not import them.
   - Therefore current "PASS" rows are necessary signals, not sufficient proof of `samp.dll` compatibility.
6. The implementation is still a scaffold.
   - `reimpl/src/runtime_bridge.c` contains boot scaffolding, environment-gated hooks, and hard-coded GTA address constants.
   - That is useful reconnaissance code, but it is not yet a recovered client-equivalent runtime.

## 3. Existing Assets To Preserve And Extend

This plan does not replace current work. It operationalizes it.

Keep and extend these existing assets:

1. `tools/collect_pe_baseline.sh`
   - keep as the fast first-pass inventory collector,
   - extend it with exact manifest generation instead of replacing it.
2. `tools/generate_re_map.sh`
   - keep as the machine-generated function/import/xref census,
   - use it to drive cluster ownership and subsystem mapping.
3. `tools/compare_runtime_traces.sh`
   - keep as the baseline/candidate diff harness,
   - harden it with module attribution so current process-global false positives are removed.
4. `analysis/generated/runtime_baseline_bigone`
   - keep as the current behavioral seed baseline,
   - treat it as scenario coverage number one, not as the only scenario.
5. Original-DLL static notes and golden traces under `analysis/metadata/` and `docs/traces/`
   - keep as the structural orientation aid for Team A,
   - do not let inferred behavior become a substitute for current-binary evidence.
6. `SPEC-NET-001` through `SPEC-NET-006`
   - keep them as recovered network knowledge,
   - use them as the nucleus of the broader subsystem-spec inventory.
7. `reimpl/src/runtime_bridge.c` and `reimpl/src/runtime_hook_bridge.c`
   - keep them as probe scaffolding and hypothesis-testing code,
   - progressively replace guessed behavior with spec-backed behavior.
8. `reimpl/scripts/build_win32.sh` and `reimpl/scripts/build_in_devbuild_toolbox.sh`
   - keep them as the candidate production loop,
   - add stricter validation after build instead of inventing a second disconnected build path.
9. Reverse-engineering source inventory recorded in `analysis/metadata/LOCAL_SOURCE_BASES.md`
   - use it only as broad architecture context for Team A clustering and spec extraction,
   - prioritize OBSERVED_037/PROBE_TRACE evidence for bootstrap, `CNetGame`, D3D/UI, archive, and RakNet layers.

## 4. Non-Negotiable Rules

1. No "probably good enough" merges. Every subsystem moves only after evidence.
2. No ABI acceptance based on process-global side effects alone.
3. No final compatibility claim until launcher-injected runs and soak tests pass.
4. Existing source bases may be used directly where they accelerate ABI reconstruction, but the target remains the current reference binary.
5. If a compatibility-first shortcut conflicts with maintainability, compatibility wins for this phase and maintainability is repaired afterward.

## 5. Intelligence-Grade Workstreams

### WS-A: Target Fingerprinting Cell

Goal: turn the reference DLL into an exact machine-readable manifest.

Required output:

1. PE header manifest:
   - entry RVA,
   - image base,
   - section/file alignment,
   - subsystem,
   - DLL characteristics,
   - stack/heap reserve and commit,
   - checksum,
   - timestamp,
   - debug directory presence,
   - TLS directory presence,
   - load-config presence.
2. Section manifest:
   - section order,
   - raw/virtual sizes,
   - characteristics,
   - entropy,
   - relocation density by section.
3. Import manifest:
   - exact DLL list,
   - exact imported symbol list per DLL,
   - ordinal-vs-name import mode,
   - thunk/IAT sizes.
4. Resource manifest:
   - icon/dialog/version/bitmap tree,
   - language IDs,
   - resource byte hashes.

Exit criterion:

The reference DLL can be fingerprinted into a deterministic manifest and diffed automatically against a candidate.

### WS-B: Dynamic Telemetry Cell

Goal: capture what the loader and runtime actually do, not what we think they do.

Required capture classes:

1. Loader telemetry:
   - `LoadLibrary*`,
   - `GetProcAddress`,
   - `DllMain` reasons,
   - TLS callbacks,
   - thread attach/detach ordering.
2. Runtime patch telemetry:
   - `VirtualProtect`,
   - `VirtualAlloc`,
   - `FlushInstructionCache`,
   - modified code ranges.
3. Hook telemetry:
   - call/jmp patch sites,
   - overwritten bytes,
   - original targets,
   - hook install timing relative to game state.
4. UI/render/audio telemetry:
   - window class registration,
   - WndProc swaps,
   - `CreateWindowEx*`,
   - `SendMessage*`,
   - `Direct3DCreate9`,
   - D3DX wrapper usage,
   - BASS init/load/play/stop lifecycle.
5. Network telemetry:
   - `WSAStartup`/`WSACleanup`,
   - `gethostbyname`,
   - `inet_addr`,
   - `inet_ntoa`,
   - `socket`,
   - `bind`,
   - `connect`,
   - `sendto`,
   - `recvfrom`,
   - retry cadence and timeout behavior.

Execution rule:

Run under Wine and, if available later, a native Windows harness. Wine is good for repeatability; a native run is the tie-breaker for loader edge cases.

### WS-C: Function Attribution Cell

Goal: turn 3935 analyzed functions into subsystem-owned targets instead of a flat blob.

Priority clusters:

1. Entry/bootstrap around `0x100cbc90`.
2. Network socket and connect/reconnect around `0x100538**` and `0x100568**`.
3. Render/D3DX-heavy callers in the `0x1006****` to `0x1009****` regions.
4. Audio/BASS-heavy callers around the `0x100664**` cluster.
5. UI/window message chains that dominate the runtime baseline.

Deliverables:

1. Cluster owner table.
2. Named behavior specs per cluster.
3. Hook-point and data-layout manifests.

### WS-D: Binary Shape Reproduction Cell

Goal: build a candidate whose PE profile is intentionally shaped to the reference instead of accepting compiler defaults.

Required actions:

1. Reproduce the import surface exactly, even if internal code later forwards into cleaner helpers.
   - Keep `WSOCK32.dll` thunks if the reference uses them.
   - Recreate D3DX and BASS wrapper imports whether or not they remain thin.
2. Remove MinGW artifacts that do not belong in the target profile.
   - `.eh_frame`
   - `libwinpthread-1.dll`
   - `msvcrt.dll` drift if not part of the target ABI
   - accidental `.tls`
3. Recreate `.rsrc` parity.
4. Force entry/section/import/header parity through post-link shaping if the compiler cannot emit it directly.
5. Treat the PE as a packaged artifact, not just "whatever the linker produced".

Decision rule:

If MinGW cannot get us close enough, switch the packager, not the goal. The objective is compatibility, not compiler loyalty.

### WS-E: Behavioral Acceptance Cell

Goal: fail fast when a candidate is only cosmetically similar.

Acceptance must include:

1. exact critical PE checks,
2. exact loader lifecycle checks,
3. exact import-surface checks,
4. module-attributed runtime checks,
5. launcher/injection acceptance,
6. reconnect and teardown soak tests.

## 6. Technical Strategy

### 5.1 Freeze A Golden Manifest

Create one canonical manifest per supported target DLL version. That manifest becomes the source of truth for Team B packaging and validation.

The manifest must include:

1. PE header fields,
2. section table with flags and sizes,
3. import list by DLL and symbol,
4. resource tree hash list,
5. relocation directory metadata,
6. string anchors used only for Team A clustering,
7. runtime baseline counters.

### 5.2 Separate "Implementation ABI" From "Packaged ABI"

Do not force the clean-room implementation to mirror the final PE shape at all times.

Instead:

1. Team B builds a maintainable internal runtime.
2. A packager/post-link stage reshapes the binary into reference-compatible form.
3. ABI validation happens on the packaged artifact, not on intermediate objects.

This avoids corrupting implementation quality just to satisfy section/import cosmetics too early.

### 5.3 Recreate The Import Surface Exactly

The import table is a contract.

Therefore:

1. Match the imported DLL list exactly.
2. Match imported symbol names exactly.
3. Match ordinal imports if any exist.
4. Preserve wrapper layers where the original DLL used them.
5. Only then forward internally to modern helpers if needed.

### 5.4 Recreate Loader Semantics Exactly

At minimum the candidate must match:

1. `DllMain` entry reason handling,
2. `PROCESS_ATTACH` and `PROCESS_DETACH`,
3. thread attach/detach counts under the same scenario,
4. TLS callback presence or absence,
5. exception filter registration timing,
6. hook-install order,
7. teardown ordering.

### 5.5 Recreate Runtime Hook Contracts

For every hook or code patch:

1. capture original bytes,
2. capture the exact patch address,
3. capture calling convention,
4. capture live register/stack assumptions,
5. capture unpatch/rollback behavior,
6. capture which game state gates the installation.

No hook is accepted based only on "it seems to work".

### 5.6 Validate On Scenario Matrices, Not Single Runs

Minimum matrix:

1. cold attach and clean exit,
2. attach with online connect,
3. failed connect,
4. connect then disconnect,
5. reconnect loop,
6. in-game render/UI activity,
7. audio initialization and teardown.

## 7. Immediate Backlog

1. Replace the current coarse PE check with a strict manifest diff that includes entry RVA, section layout, and exact imports.
   - `tools/check_pe_abi_strict.sh` now covers the diff side.
   - `tools/collect_pe_manifest.sh` should be used to freeze the reference manifest.
2. Add module-attributed runtime checks so process-global DLL loads stop generating false positives.
3. Capture a clean reference manifest for the current target version and store it under `analysis/metadata`.
4. Create a dedicated `reverse-engineering` toolbox with Wine, debuggers, PE tools, MinGW, and Python reverse-engineering packages.
5. Add a repeatable Wine trace capture wrapper for baseline and candidate runs.
6. Map the full bootstrap cluster around `0x100cbc90` into formal spec slices.
7. Map D3DX and BASS caller clusters into subsystem specs.
8. Recover resource and version metadata requirements.
9. Decide whether MinGW plus post-link shaping is sufficient; if not, add an alternate packaging path.
10. Treat launcher-injected acceptance as a hard gate before any "ABI-compatible" claim.

## 8. Acceptance Gates

### Gate 0: Static Manifest

Pass only if:

1. entry RVA matches,
2. image base matches,
3. section order matches,
4. required sections match,
5. forbidden sections are absent,
6. import DLL set matches,
7. import symbol set matches,
8. stack/heap and alignment fields match.

### Gate 1: Loader Parity

Pass only if:

1. attach/detach both occur,
2. thread lifecycle counts stay within agreed tolerance,
3. patch-install order matches,
4. teardown ordering matches.

### Gate 2: Module-Attributed Runtime Parity

Pass only if:

1. observed API chains can be tied to `samp.dll`,
2. network/UI/render/audio calls occur in the right order,
3. timing and retry cadence match within tolerance.

### Gate 3: User-Visible Behavior

Pass only if:

1. connect banners match,
2. connected banners match,
3. reconnect behavior matches,
4. no missing critical UI or audio transitions remain.

### Gate 4: Real Acceptance

Pass only if:

1. the launcher accepts the DLL,
2. the runtime remains stable in soak tests,
3. no hard parity regressions remain open.

## 9. Definition Of "Compatible"

`samp.dll` is compatible only when the packaged candidate passes all gates above.

Until then the correct statement is:

"The rebuild has partial behavioral scaffolding and partial subsystem mapping, but it is not yet ABI-compatible."
