# Current Status and Unknown-DLL Reverse Plan (2026-05-31)

Purpose: document the current `samp.dll` rebuild status and define a practical workflow for reverse-engineering an unknown DLL version when older versions/source bases are available.

## 1. Current Project State

The project is already beyond first reconnaissance. There is a frozen reference DLL, generated binary metadata, runtime trace material, multiple specs, a buildable reimplementation scaffold, host-side tests, and PE shaping experiments.

Important caveat: the top-level workspace currently does not behave as a normal Git repository. `git status --short` fails because the top-level `.git` is not recognized as a repository. Nested source trees have their own `.git` metadata, but the rebuild workspace itself should be treated as a file workspace unless Git is repaired.

## 2. Reference Target

Reference binary:

- `samp.dll`
- SHA-256: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Size: `1,204,224` bytes
- Format: `PE32` x86 DLL
- Image base: `0x10000000`
- Entry RVA: `0x000cbc90`
- Sections: `.text`, `.rdata`, `.data`, `.rsrc`, `.reloc`
- No export directory
- No TLS directory
- Compile timestamp recorded previously: `2022-11-14 23:39:26`
- Embedded PDB path recorded previously: `c:\sampcvs037R4\sampcvs\main\saco\Release\samp.pdb`

The local `samp.dll`, `artifacts/binaries/samp_local.dll`, and `artifacts/binaries/samp_installer.dll` are bit-identical.

Reference imported DLL families:

- `ADVAPI32.dll`
- `BASS.dll`
- `COMCTL32.dll`
- `GDI32.dll`
- `KERNEL32.dll`
- `PSAPI.DLL`
- `SHELL32.dll`
- `USER32.dll`
- `WINMM.dll`
- `WSOCK32.dll`
- `d3dx9_25.dll`

New PE manifests generated today:

- `analysis/metadata/pe_manifest/reference_samp_0_3_7_r5/`
- `analysis/metadata/pe_manifest/candidate_build_win32/`
- `analysis/metadata/pe_manifest/candidate_norak_shaped/`

## 3. Reverse-Engineering Knowledge Already Captured

Static map:

- `analysis/generated/RE_MAP_SUMMARY.md` reports `3935` discovered functions.
- `analysis/metadata/CURRENT_DLL_DETAILED_MAP.md` maps the entry/bootstrap path, network helpers, D3DX wrapper clusters, USER32-heavy UI clusters, and BASS audio cluster.
- The high-confidence boot chain starts at `entry0 @ 0x100cbc90` and flows through `fcn.100cbb0f`, `fcn.100c5270`, and `fcn.100c50c0`.
- The high-confidence network cluster includes `fcn.10053820`, `fcn.10053850`, `fcn.100539c0`, `fcn.10053870`, `fcn.100538b0`, `fcn.10053a00`, `fcn.10053ab0`, `fcn.10053b70`, `fcn.10055ff0`, and `fcn.10056880`.

Runtime baseline:

- `analysis/generated/runtime_baseline_bigone/BASELINE.md`
- Baseline shows one `samp.dll` process attach, one process detach, 20 thread attaches, 9 thread detaches.
- Baseline user-visible network signals include 51 `Connecting to ...` banners and 17 `Connected to ...` banners.
- Top API activity is dominated by USER32 window/message calls and Winsock receive/send/resolve calls.

Runtime probe update from 2026-06-01:

- The Wine prefix currently contains a different packed-looking `samp.dll` than the stored R5 reference. Prefix SHA-256 is `7e30f3c9cd99d5e2932410f486e8139affa2dad19bd65ad9c328f6a4071943f7`; the stored reference SHA-256 is `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
- The prefix DLL has sparse visible imports and sections named `.poop0`, `.poop1`, and `.poop2`, with entry RVA `0x0031df13`.
- The first ASI probe installed only visible `samp.dll` IAT hooks for `WSOCK32.dll` ordinal 17 (`recvfrom`) and `KERNEL32.dll!LoadLibraryA`.
- A server connect still succeeded, but no Winsock `call:` lines were emitted. That strongly suggests the active network path bypasses the visible `samp.dll` IAT, likely through unpacked or dynamically resolved function pointers.
- Runtime watchpoints show SA-MP replacing the GTA game-process hook site at `0x0058C246` with an indirect jump through `0x0053BED1`. In the observed run, the storage value pointed into the loaded `samp.dll` image around RVA `0x0009d900`.
- `tools/asi_probe` has therefore been extended with filtered inline hooks on selected `WSOCK32.dll` exports. New successful runs should produce `caller_rva` values for dynamic Winsock callsites if the path goes through WSOCK32.
- The first inline-hook run succeeded: `15/15` WSOCK32 export hooks installed, and the connect path produced concrete callsites. Key network RVAs are `socket=0x0004fdd0`, `bind=0x0004feb8`, `recvfrom=0x0004ff67`, `sendto=0x00050029`, `closesocket=0x0003cd40`, and `WSACleanup=0x0004fd6f`. The run is archived as `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_114846_inline_success.log`.
- The probe was then extended again to log payload previews for `send/sendto/recv/recvfrom`; the first payload run used a 96-byte preview, and the current ASI build uses a 256-byte preview.
- A successful-connect payload run is archived as `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_115416_payload.log` with SHA-256 `a1c3c4b78141e466629102aaabfd9101e1e3b23e2ff548dfdefecb8d2874bf42`. Its decoded summary is `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_115416_payload.summary.md`.
- Decoding outbound socket bytes with the SA-MP client datagram transform gives `sendto #1 -> 18 69 69`, `recvfrom -> 1a 89 9e`, and `sendto #2 -> 18 e0 f7`; `0xf7e0 ^ 0x6969 == 0x9e89`, so the probe has captured the exact 0.3.7 open-connection-cookie exchange implemented in `samp/raknet/RakPeer.cpp`.
- The connect path also emits reliable post-handshake traffic, including a transformed payload containing generated auth/GPCI-looking ASCII bytes before/around the join phase. This confirms that the original behavior is above raw socket traffic and should be compared at RakNet/RPC level.
- Cross-checking with the current reimpl build shows the RakNet backend is not yet a 1:1 SA-MP client transport. `reimpl/CMakeLists.txt` forces `RAKNET_BUILD_SAMP_GLUE=OFF`, so `third_party/raknet-knogle/SAMPRakNet.hpp` uses stub `Decrypt`/`Encrypt` implementations that return bytes unchanged. If the vendored glue is enabled, that path is still server/open.mp oriented and only conditionally encrypts in an OMP path. The original client source transforms every outbound datagram under `RAKSAMP_CLIENT`.
- `tools/asi_probe` now includes guarded internal `samp.dll` RVA hooks for `SocketLayer.SendTo` (`0x0004ffc0`) and `ProcessNetworkPacket` (`0x0003b950`) on the observed prefix DLL. A debug ASI was built in `devbuild` and copied into the prefix; SHA-256 is `37ef0a23d1e7694b9819ceb8399f39f74868baf4d86dc286923ba9cade760f05`, map SHA-256 is `0b5be95009cee9ae22d6e2a014822d96e497c554af3e500a8f10e85d18d70da0`.
- The internal-plaintext run after this ASI update is archived as `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_130617_internal_plain.log` with SHA-256 `c9715268304ed7453fccc518b899961b1db8dde6b4a23372fd564b1fb891bf1d`. Its summary is `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_130617_internal_plain.summary.md`. The run captured `28` internal sends and `40` internal receives; the first four plaintext handshake frames are `18 69 69`, `1a bb 7f`, `18 d2 16`, and `19 00`.
- Later 2026-06-01 client runs moved past singleplayer/black-screen failures into a partially controlled world view. The server-side join succeeds and the client now reaches the GTA sky/camera phase, but the stock intro cutscene text/audio (`Los Santos International Airport`) still appears. This means RakNet and the basic MP session bridge are no longer the primary blocker; the active blocker is suppressing or replacing the original GTA main SCM/story script path.
- The sky/intro-cutscene run is archived in `analysis/generated/asi_probe_logs/2026-06-01_1740_sky_intro_story_scm/`. Key evidence: `InitGame`, `RequestClass`, camera/player RPCs, graphics callback, and repeated `mp_session_bridge` applies all occur; the GTA state later remains around `entry=9`/`game_started=0`, matching the story/cutscene phase.
- Cross-check with 0.2x source shows two missing compatibility areas: `ApplyInGamePatches()` and the SA-MP archive/file hook path. 0.2x calls `ApplyInGamePatches()` before installing game hooks, and its `filehooks.cpp` serves files from `samp.saa` by filename. `ArchiveFS.h` explicitly names archive hashes for `main.scm` and `script.img`; without that path, GTA can load its original story `main.scm`.
- Candidate `2026-06-01 17:xx` now adds a focused story-suppression runtime test: a small subset of 0.2x in-game patches, online-mode suppression of `CTheScripts::Process`, and a longer MP session bridge window. Deployed candidate SHA-256: `7cd243dec17ca952b3368d949713196749ede4c3e38ee699c70f12b30066da62`. Escape hatch: set `SAMPDLL_RUN_GAME_SCRIPTS=1` to allow the original script process again for comparison.
- That story-suppression run reached the `Ganton` load/zone screen instead of story mode, then crashed in GTA code at `0x0046ff3e` reading `0x00000474`. The run is archived in `analysis/generated/asi_probe_logs/2026-06-01_1748_ganton_no_story_hang/`. Interpretation: disabling `CTheScripts::Process` removes the story, but also prevents the SA-MP/minimal SCM bootstrap from creating a valid player/script state.
- Candidate `2026-06-01 17:50` therefore switches strategy: `CTheScripts::Process` is enabled by default again, while the prefix `data/script/main.scm` and `data/script/script.img` are temporarily replaced with the 0.2x SA-MP archive versions. Original prefix files were backed up both in the prefix and under `analysis/generated/asi_probe_logs/2026-06-01_1750_samp_minimal_scm/`. Deployed DLL SHA-256: `ac29cc0a62e0919105ea9a2f2a62eb9cefb692f740fea18efc35c9e66f539bd1`; deployed minimal SCM hashes: `main.scm=39fcda70b53af665d98be0f4ee665efa96dc755786af2666ce3f9af63de1743b`, `script.img=0a128efaf5d06f453dd3bd66d0cf93c489ccc6b352110b84b96e10671f75ae24`.

Source/reference anchors:

- `samp/` is a local SA-MP client/server source base, described as SA-MP Client 0.2.5 with network updates toward 0.3.7.
- `gta-samp-0.2x-source-code/` provides older client/server/RakNet source structure.
- `analysis/metadata/LOCAL_SOURCE_BASES.md` records that these are useful structural references, but current binary evidence remains the target authority.

## 4. Reimplementation State

Current `reimpl/` scope:

- `DllMain` dispatch into `samp_runtime_dispatch`.
- Phased boot scaffold in `reimpl/src/runtime_bridge.c`.
- LaunchMonitor-style worker thread scaffold.
- Settings parsing for legacy command-line fields.
- Dual-stack endpoint parser and resolver foundation.
- Legacy IPv4 socket compatibility layer.
- TCP bootstrap manager lifecycle scaffold.
- Optional Knogle RakNet bridge and adapter.
- Runtime hook bridge experiments for graphics/script/game-process call points.
- Resource section exists again in current Win32 builds.

Host tests run today:

```text
dual_stack                 PASS
legacy_ipv4_compat         PASS
tcp_bootstrap_manager      PASS
raknet_bridge              PASS
```

Result: `4/4` host tests pass.

## 5. Candidate ABI Status

The project is not ABI-compatible yet.

`build-win32/samp.dll` strict check:

- Entry RVA mismatch: reference `0x000cbc90`, candidate `0x000011f0`.
- Section count mismatch: reference `5`, candidate `9`.
- Import DLL count mismatch: reference `11`, candidate `5`.
- Import symbol count mismatch: reference `320`, candidate `128`.
- Unexpected sections: `.eh_frame`, `.bss`, `.idata`, `.tls`.
- Unexpected imports: `WS2_32.dll`, `libwinpthread-1.dll`, `msvcrt.dll`.

`build-win32-norak/samp-shaped.dll` strict check:

- Better import-family parity: all reference DLL families are present.
- Still has one unexpected DLL: `msvcrt.dll`.
- Entry RVA still mismatches: reference `0x000cbc90`, candidate `0x000011f0`.
- Section count/order still wrong: reference `5`, candidate `10`.
- Import symbol count still wrong: reference `320`, candidate `247`.
- Missing import symbols: `107`.
- Unexpected import symbols: `34`.
- TLS and `.eh_frame` still exist, both absent from the reference.

Conclusion: the shaper proves import-surface reconstruction is feasible, but it is not yet a complete PE packager.

## 6. Runtime Parity Status

Recent runtime diff evidence is mixed:

- The latest after-winpthread diff shows `samp.dll` native load, process attach, process attach start/end, and process detach all passing.
- Network primitive calls are present (`WSAStartup`, `WSACleanup`, `gethostbyname`, `inet_addr`, `inet_ntoa`, `sendto`, `recvfrom`).
- UI/window calls are present at a coarse level.
- The candidate still shows `0` `Connecting to ...` banners where the reference has `51`.
- The candidate still shows no useful `Connected to ...` banner parity in the captured report.

Important interpretation: current trace PASS rows are necessary signals, not sufficient proof. Some checks are still process-global and can be satisfied by modules other than the candidate `samp.dll`. The next trace harness improvement must attribute calls to the owning module/call stack.

## 7. Main Gaps

1. PE packaging is the dominant hard blocker:
   - entry RVA,
   - section order,
   - CRT/TLS removal,
   - exact import table,
   - resource/version parity,
   - relocation shape.
2. Runtime behavior is still a scaffold:
   - connect banners are not emitted in the real UI path,
   - reconnect/connected state is not behavior-equivalent,
   - hook install order is not proven against the current target.
3. Dynamic telemetry is not strong enough:
   - current checks still risk process-global false positives,
   - native Windows trace capture should become the tie-breaker,
   - hook patch sites need exact byte/register/stack contracts.
4. Function attribution is incomplete:
   - boot and networking are partially mapped,
   - render/UI/audio clusters need formal spec slices,
   - giant functions and anti-cheat/exception paths remain low-confidence.

## 8. How To Reverse Engineer An Unknown DLL When Previous Versions Are Known

The reliable path is version-diff first, decompiler last.

### Phase A: Freeze The Corpus

For each DLL version, store:

- original filename,
- SHA-256,
- file size,
- PE timestamp,
- image base,
- entry RVA,
- sections,
- imports,
- exports,
- resources,
- relocations,
- debug/PDB strings,
- installer/source of acquisition.

Recommended layout:

```text
artifacts/versions/<version>/raw/<dll>
analysis/metadata/versions/<version>/pe_manifest/
analysis/generated/versions/<version>/
```

Use the existing manifest script as the baseline:

```bash
tools/collect_pe_manifest.sh <dll> analysis/metadata/versions/<version>/pe_manifest
```

### Phase B: Build Static Diffs Across Versions

Diff from newest known version to unknown version, not from oldest to unknown.

Required diff layers:

1. PE header and section diff.
2. Import/export/resource diff.
3. String diff, especially user-visible messages and protocol/version strings.
4. Function inventory diff using normalized image base.
5. Callgraph and CFG similarity matching.
6. Import-callsite clustering.
7. Vtable/global-data/string-reference matching.

Tools that fit this project:

- current scripts: `tools/generate_re_map.sh`, `tools/check_pe_abi_strict.sh`, `tools/collect_pe_manifest.sh`;
- Rizin/radare for function/import/xref inventory;
- Ghidra Version Tracking or Diaphora/BinDiff if available;
- LIEF/pefile for manifest and packager experiments.

### Phase C: Promote Matches Into A Version Crosswalk

Create a table like:

```text
old_version_va | old_name_or_role | unknown_va | confidence | evidence | notes
```

Evidence should be concrete:

- same string xrefs,
- same import call pattern,
- same callgraph neighbors,
- same constants,
- same resource IDs,
- same protocol packet IDs,
- same branch/state behavior in traces.

Avoid trusting a decompiler match just because the pseudocode looks familiar. Compiler drift and inlining can make that misleading.

### Phase D: Dynamic Trace The Same Scenarios

Run every known and unknown version through the same scenario matrix:

1. cold load and clean exit,
2. load with missing dependencies,
3. failed connect,
4. successful connect,
5. disconnect and reconnect loop,
6. render/UI activity,
7. audio init/play/stop,
8. launcher-injected path.

Trace at least:

- `DllMain` reasons,
- TLS callbacks,
- `LoadLibrary*`,
- `GetProcAddress`,
- `VirtualProtect`,
- `VirtualAlloc`,
- `FlushInstructionCache`,
- hook patch writes,
- Winsock calls,
- USER32 window/WndProc calls,
- D3D/D3DX calls,
- BASS calls.

The trace must be module-attributed. A process-global `BASS.dll loaded` or `recvfrom called` is not enough.

### Phase E: Recover Contracts, Not Just Code

For each changed function or cluster, write down:

- inputs and outputs,
- calling convention,
- ownership/lifetime rules,
- error paths,
- timing/retry cadence,
- global state touched,
- external API calls,
- user-visible messages,
- exact hooks/patch bytes if it patches another module.

Only after this should implementation work start.

### Phase F: Rebuild With Two Separate ABIs

Keep two layers separate:

1. Implementation ABI:
   - maintainable source,
   - tests,
   - internal helper APIs.
2. Packaged ABI:
   - reference-like PE shape,
   - exact import surface,
   - expected entry RVA,
   - expected resources,
   - expected section table,
   - no accidental CRT/TLS/unwind artifacts.

This project already has the right direction: use `reimpl/` for implementation and a post-link shaping/packaging stage for compatibility.

## 9. Immediate Next Steps For This Repo

1. Make `candidate_norak_shaped` the packaging experiment target, not the RakNet-enabled `build-win32` artifact.
2. Remove or avoid `msvcrt.dll`, `.tls`, `.eh_frame`, `.bss`, and duplicate `.idata` from the packaged candidate.
3. Extend `tools/shape_pe_imports.py` or add a dedicated packager that can also control section layout and entry thunk placement.
4. Add exact import-symbol completion for the missing `107` symbols in `candidate_norak_shaped`.
5. Make runtime trace checks module-attributed before treating UI/audio/network PASS rows as meaningful.
6. Map the user-visible connect path from legacy `CNetGame` to the current target and wire candidate state so `Connecting to ...` appears through the same UI/message path, not only as debug trace text.
7. Convert D3DX/BASS/USER32 high-density clusters into specs, because import parity without behavior parity will still fail real launcher/runtime acceptance.
8. Add a native Windows capture pass for the reference once the Wine loop is stable enough to compare repeatably.

## 10. Current Compatibility Statement

The rebuild currently has partial boot, network, hook, and packaging scaffolding. It is useful for experiments and already passes host-side module tests, but it is not yet an ABI-compatible `samp.dll` replacement.

The best next engineering focus is PE packager correctness plus module-attributed runtime traces. Those two pieces will turn the existing reverse-engineering work from useful notes into a reproducible compatibility loop.

## 11. 2026-06-01 Client Transport Update

The successful original-`samp.dll` connect probe captured the legacy client UDP transport transform before and after the socket boundary:

- plaintext `18 69 69` becomes wire bytes `08 1e 77 da`,
- plaintext `18 d2 16` becomes wire bytes `88 1e e3 9b`,
- inbound legacy server handshake packets remain plaintext at the client receive boundary.

The reimpl now enables `SAMPDLL_ENABLE_LEGACY_SAMP_CLIENT_TRANSPORT` by default for the vendored Knogle RakNet target. In this mode `SocketLayer::SendTo` applies the SA-MP client datagram transform for IPv4 outbound packets, while `RecvFrom` forwards inbound packets without the server-side decrypt/drop-checksum path. The new host test `samp_client_transport` validates the two captured original-client vectors.

Current deployed candidate in the GTA prefix:

- build artifact: `build-win32/samp.dll`
- SHA-256: `3aeb9cb8a386740512b822425179b1119999cb7cf69a532f234bb153ee7197d4`
- previous prefix DLL archived under `analysis/generated/prefix_backups/`

The deployed ASI probe was also refreshed to hook `WS2_32.dll` imports in addition to `WSOCK32.dll`, because the current RakNet-enabled candidate imports `WS2_32.dll`. Its original-`samp.dll` RVA hooks remain guarded by prologue checks and should skip on the reimpl. Deployed probe SHA-256: `668af1d6152a2f0bd89a869a94a233d35791240a8937da5058ab6bd56560e64c`.

First runtime attempt with the candidate loaded GTA into singleplayer. The ASI log showed `probe: timed out waiting for samp.dll`, so the failure happened before RakNet: `samp.exe` did not successfully inject/load `samp.dll`. The candidate imports `libwinpthread-1.dll`, which was missing from the GTA directory; the 32-bit MinGW runtime DLL was copied into the prefix as a temporary loader unblocker. Runtime DLL SHA-256: `cb162ef05de88685883b65af6b35ce1d9b39a581a6ba5d3057131406eae15972`.

Second runtime attempt loaded the candidate and reached the GTA loading screen. The trace showed GTA hook writes and successful `d3dx9_25.dll`/`BASS.dll` loads, but RakNet repeatedly sent wire bytes `0a 71 8a` to `127.0.0.1:7777`. This decodes as the SA-MP datagram transform applied to a wrong plaintext open-connection request (`0a 00`), proving the transform layer was active but the vendored RakNet packet IDs were not SA-MP-compatible. The candidate was updated to use SA-MP legacy packet IDs (`ID_OPEN_CONNECTION_REQUEST == 0x18`, cookie `0x1a`, accepted `0x22`), to send the initial `18 69 69` request, and to answer `ID_OPEN_CONNECTION_COOKIE` by XORing the cookie with `0x6969`.

Current deployed candidate after this fix:

- `samp.dll` SHA-256: `6642f484a8ff09b68a79ecff384163c6f26ab22c3d7cc8ffd4cd6ff7c6251e03`
- `samp_probe.asi` SHA-256: `7a6101d20e8c06616e3215e6194aa213a3de8a939ca02d924b8812754de7b19c`
- archived wrong-packet-ID trace: `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_135805_candidate_wrong_packet_ids.log`

## 12. 2026-06-01 Blackscreen After Open Reply

The next candidate advanced past the white loading screen and then stayed black before crashing/closing. The archived trace is:

- `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_142258_candidate_blackscreen_after_open_reply.log`

Important network sequence from that run:

- outbound `08 1e 77 da`, decoded as plaintext `18 69 69`,
- inbound cookie `1a 05 f1`,
- outbound `a8 1e 5d b5`, decoded as the expected cookie response for `0xf105 ^ 0x6969 == 0x986c`,
- inbound `19 00`, i.e. `ID_OPEN_CONNECTION_REPLY`,
- no later `sendto` before the crash.

The missing post-`19 00` send was traced to the standalone `SAMPRakNet` stub: `minimumSendBitsPerSecond_` was initialized to `0.0f`. In this build configuration `ReliabilityLayer::Send` could queue the reliable `ID_CONNECTION_REQUEST`, but `ReliabilityLayer::Update` never had any bandwidth budget, so the queued packet was never flushed to `SocketLayer::SendTo`.

Fix applied:

- `third_party/raknet-knogle/SAMPRakNet.hpp` standalone default changed to `96000.0f`, matching `SAMPRakNet.cpp`.
- `reimpl/tests/test_samp_client_transport.cpp` now asserts the standalone stub has a non-zero minimum send rate.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure`
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `db001cb68b5f037ece9453c3b3414b367b2b4cf001f1285796ea4b1a8b958f67`
- `samp_probe.log` in the GTA prefix was cleared after deployment for the next run.

## 13. 2026-06-01 Auth-Key Stage

The next runtime attempt still appeared as a blackscreen to the user, but the trace had advanced:

- archived trace: `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_143151_candidate_auth_key_missing.log`
- after inbound `19 00`, the candidate now sends reliable plaintext `00 00 43 80 0b`, i.e. the queued `ID_CONNECTION_REQUEST` is flushing.
- the server ACKs it with `e3 00 00`.
- the server then sends reliable payload `0c 10 35 38 39 30 31 43 46 34 35 31 43 39 33 45 33 00`.
- decoded payload: `ID_AUTH_KEY`, length `0x10`, challenge string `58901CF451C93E3\0`.
- the correct response is present in the 0.3.7 auth table: `A24C762722180B42D75D32641BA1F5BD5705498A`.

This confirms the 0.2x RakNet flow is still a useful structural map, but 0.3.7 adds an auth challenge step between `ID_CONNECTION_REQUEST` and `ID_CONNECTION_REQUEST_ACCEPTED`.

Fix applied:

- `third_party/raknet-knogle/Source/RakPeer.cpp` now forwards `ID_AUTH_KEY` packets to the client packet queue, matching `samp/raknet/RakPeer.cpp`.
- `reimpl/src/net/raknet_client_adapter.cpp` now answers `ID_AUTH_KEY` by looking up the challenge in `samp/raknet/SAMP/samp_auth.cpp`.
- `reimpl/CMakeLists.txt` links the SA-MP auth table into `sampdll_net`.
- `reimpl/tests/test_samp_client_transport.cpp` now asserts the captured challenge/response vector.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure`
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `892b4e0d78627c1775579c2e5d4faf24d385abc382e3ed39dade5986b8b7d68f`
- `samp_probe.log` in the GTA prefix was cleared after deployment for the next run.

## 14. 2026-06-01 Story-Mode After Auth/Accepted

The next runtime attempt reached the server accept stage but then dropped into GTA story mode instead of receiving the full SA-MP world bootstrap.

- archived trace: `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_144751_candidate_auth_join_story_mode.log`
- candidate SHA-256 from that run: `892b4e0d78627c1775579c2e5d4faf24d385abc382e3ed39dade5986b8b7d68f`

Important network sequence from that run:

- auth key was answered successfully.
- server sent reliable payload with `ID_CONNECTION_REQUEST_ACCEPTED` (`0x22`).
- candidate sent the same small reliable housekeeping frame as the original client.
- candidate did **not** send the later original-style large `RPC_ClientJoin` frame (original trace has a `sendto` of 108 wire bytes at this point).
- after that, no large server bootstrap packets arrived, while GTA local state advanced through entry gate `8` and `9`, matching the observed story-mode fall-through.

Root cause:

- `send_client_join()` parsed the accepted packet incorrectly.
- The original client reads a 16-bit `PLAYERID` after address and port.
- The reimpl was reading a 6-byte `RakNet::PlayerID`, consuming the 16-bit player id plus the 32-bit challenge, so the following challenge read failed and `RPC_ClientJoin` was never sent.

Fix applied:

- `reimpl/src/net/raknet_client_adapter.cpp` now reads `RakNet::PlayerIndex` from `ID_CONNECTION_REQUEST_ACCEPTED`.
- The default Join client version was aligned with the legacy source to `0.3.7` in both the adapter and runtime profile defaults.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure`
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `7f7087f1596d5ec26d7b701e7fd5b075db800a224e55ef4716c6cad7a5e5c1f0`
- `samp_probe.log` in the GTA prefix was cleared after deployment for the next run.

Expected signal in the next trace:

- after `ID_CONNECTION_REQUEST_ACCEPTED`, there should be an additional large reliable client datagram around the original `RPC_ClientJoin` size (roughly 100+ wire bytes), followed by the server's larger bootstrap packets.

## 15. 2026-06-01 Server Join Confirmed, Client Still Story-Mode

The next run confirmed the `RPC_ClientJoin` fix:

- archived trace: `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_150944_candidate_server_join_singleplayer.log`
- candidate SHA-256 from that run: `7f7087f1596d5ec26d7b701e7fd5b075db800a224e55ef4716c6cad7a5e5c1f0`
- server log reported `incoming connection` and `[join] Knogle has joined the server`, followed later by `[part]`.
- the trace now contains the large post-accept reliable frame (`sendto` length 101, plaintext reliable payload beginning with `01 00 42 38 ...`), matching the expected `RPC_ClientJoin` stage closely enough for server admission.

Current interpretation:

- transport, cookie, auth-key, accepted-packet parsing, and `RPC_ClientJoin` are now functional enough for the server-side join path.
- the remaining story-mode fall-through is now client-side post-join handling, not server admission.
- RakNet `Receive()` consumes `ID_RPC` internally and only calls registered RPC callbacks. The reimpl had no client RPC registrations, so server bootstrap RPCs such as `RPC_InitGame` (`139`), `RPC_RequestClass` (`128`), `RPC_RequestSpawn` (`129`), and script RPCs could be dropped before the runtime bridge sees them.

Probe/fix applied:

- `reimpl/src/net/raknet_client_adapter.cpp` now registers an RPC observer for SA-MP RPC IDs `1..200`.
- the observer writes focused RPC diagnostics to `samp_net_trace.log` in the GTA directory.
- when `RPC_InitGame` is observed, the adapter sends `RPC_RequestClass(0)`.
- when `RPC_RequestClass` is observed, the adapter sends `RPC_RequestSpawn`.
- auth-key and client-join sends are also mirrored into `samp_net_trace.log`.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure`
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `3120e0b405f50badc2ad5dcd25915c8bfedc2f4cbad339bdbbfed2703cafcfe4`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log` and `samp_net_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_net_trace.log` should contain `rpc-probe registered ids=1..200`, then `auth-key`, `ClientJoin`, and ideally `rpc-in id=139 name=InitGame`.
- if `InitGame` appears, `rpc-auto-out id=128 name=RequestClass` should follow.
- if the server answers class selection, `rpc-in id=128 name=RequestClass` and `rpc-auto-out id=129 name=RequestSpawn` should follow.
- if `InitGame` does not appear, the blocker is still lower in RakNet packet/RPC parsing despite the server-side join.

## 16. 2026-06-01 RPC Bootstrap Confirmed, Local Session Still SP-Like

The next successful-connect run archived both the ASI and RPC probe logs:

- `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_152257_candidate_rpc_bootstrap_story_mode.log`
- `analysis/generated/asi_probe_logs/samp_net_trace_2026-06-01_152257_candidate_rpc_bootstrap_story_mode.log`
- candidate SHA-256 from that run: `3120e0b405f50badc2ad5dcd25915c8bfedc2ad339bdbbfed2703cafcfe4`

Important RPC evidence:

- auth-key response was sent.
- `RPC_ClientJoin` was sent with nickname `Knogle` and version `0.3.7`.
- the server sent many `RPC_ServerJoin` packets for player/NPC bootstrap.
- the server sent `RPC_ScrDialogBox` (`61`) with first bytes `5d 00 02 2b ...`, decoded as dialog id `93`, style `2`, title beginning `Select your language`.
- the server sent `RPC_InitGame` (`139`).
- the adapter sent `RPC_RequestClass(0)`.
- the server answered `RPC_RequestClass` with outcome `1` and spawn-info bytes.
- the adapter sent `RPC_RequestSpawn`.
- the server answered `RPC_RequestSpawn` with outcome `0`, so a legacy client would not call `CLocalPlayer::Spawn()`.

Interpretation:

- the remaining failure is no longer handshake or server admission.
- the adapter is observing server RPCs, but the local GTA/SA-MP runtime still lacks the client-side state application that 0.2x performs in `InitGame`, `RequestClass`, and `RequestSpawn`.
- the early language dialog is probably gating gamemode spawn approval. The denied `RequestSpawn` is consistent with not answering `OnDialogResponse` before asking to spawn.
- the ASI log also shows the GTA menu flag later changing to `1`, which matches the observed story-mode fall-through.

Fix applied after this run:

- `reimpl/src/net/raknet_client_adapter.cpp` now tracks RPC bootstrap milestones and exposes them to the runtime bridge.
- the first `RPC_ScrDialogBox` is auto-answered with `RPC_DialogResponse`, dialog id from the packet, response `1`, list item `0`, and input text `English`.
- `RPC_RequestClass` and `RPC_RequestSpawn` outcomes are logged explicitly.
- `reimpl/src/runtime_bridge.c` no longer treats raw RakNet transport connection as full netgame connection; it promotes to `SAMP_NETGAME_CONNECTED` only after `RPC_InitGame` has been observed.
- after `RPC_InitGame`, the runtime keeps `startgame/menu` flags clamped to avoid falling back into the GTA menu/story path.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `0faf33f77d9edfffbafe6340ea89d26d152d0461aa7585df6e93fcf02e0b53f4`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log` and `samp_net_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_net_trace.log` should contain `rpc-auto-out id=62 name=DialogResponse`.
- after the dialog response, `RPC_RequestSpawn` should ideally return outcome `1` or `2`; if it still returns `0`, the next target is the exact local class-selection/waiting-for-spawn state contract.
- `samp_probe.log` should no longer show `menu_flag` becoming `01` after `RPC_InitGame`; if it does, the local GTA-state clamp is still incomplete.

## 17. 2026-06-01 Second Dialog And Premature Spawn

The next run kept the same user-visible behavior, but the logs found a concrete new issue:

- `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_1535xx_dialog_response_still_story_mode.log`
- `analysis/generated/asi_probe_logs/samp_net_trace_2026-06-01_1535xx_dialog_response_still_story_mode.log`
- candidate SHA-256 from that run: `0faf33f77d9edfffbafe6340ea89d26d152d0461aa7585df6e93fcf02e0b53f4`

Important evidence:

- dialog id `93` (`Select your language`) was auto-answered successfully.
- the server then sent dialog id `94`, style `0`, with title text beginning `You have successfully set your...`.
- the previous adapter did not answer this second dialog because it only sent one dialog response per session.
- `RPC_RequestClass` still returned outcome `1`.
- `RPC_RequestSpawn` still returned outcome `0`.

Legacy-code cross-check:

- `CGame::StartGame()` applies in-game patches/hooks, calls `InitScripting()`, then writes `ADDR_ENTRY=8`, `ADDR_GAME_STARTED=1`, `ADDR_MENU=0`, `ADDR_STARTGAME=0`.
- `InitGame` sets `GAMESTATE_CONNECTED` and calls `HandleClassSelection()`.
- `HandleClassSelection()` disables local control, sends `RequestClass`, and starts a class-selection timer.
- `ProcessClassSelection()` only sends `RequestSpawn` after the class has been accepted, the initial selection delay has elapsed, and the player presses SHIFT.

Fix applied after this run:

- all `RPC_ScrDialogBox` packets are now auto-answered, not only the first one.
- list dialogs use input text `English`; messagebox-style dialogs use empty input.
- the first `RPC_RequestSpawn` is delayed by about `2.2s` after class acceptance to mimic the legacy class-selection delay.
- if `RPC_RequestSpawn` returns `0`, the adapter schedules a small number of delayed retries instead of giving up immediately.
- if `RPC_RequestSpawn` returns `1` or `2`, the adapter sends `RPC_Spawn` (`52`) as the legacy `CLocalPlayer::Spawn()` path would do after local spawn setup.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `e3f5669d856f695ec46f2b2f6413ee74a674bf7ef6ae989cfe074ae1c08f22f2`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log` and `samp_net_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_net_trace.log` should contain two `rpc-auto-out id=62 name=DialogResponse` rows: one for dialog `93`, one for dialog `94`.
- `rpc-auto-out id=129 name=RequestSpawn` should occur later than before and include `attempt=`/`retry=` details.
- if spawn becomes accepted, `rpc-auto-out id=52 name=SpawnNotify` should appear.
- if spawn remains denied, the server/gamemode is still blocking spawn and the next target is either exact dialog/login completion or the missing local SA-MP class-selection/player-state implementation.

## 18. 2026-06-01 Dialogs Reclassified As Gameplay

User correction: the observed dialogs are intended multiplayer gameplay and should not be auto-answered by the probe. The current issue remains that the server-side multiplayer session can exist while the local GTA client still behaves like singleplayer.

Fix applied after this correction:

- `RPC_ScrDialogBox` packets are now observe-only again.
- the adapter logs dialog id/style as `rpc-state id=61 ... observe_only=1`.
- no code path sends `RPC_DialogResponse` (`62`) automatically.
- the delayed `RPC_RequestSpawn` and accepted-spawn `RPC_Spawn` probe paths remain, because they are still useful for identifying the class/spawn transition contract.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `d134b85ade3877ccbd0132e4e186c6b0afa83c1712c353b293aff0438be9e6aa`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log` and `samp_net_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_net_trace.log` should not contain `rpc-auto-out id=62 name=DialogResponse`.
- dialog packets should appear only as `rpc-in id=61 name=ScrDialogBox` plus `observe_only=1`.
- if the server still logs a successful join while the client remains in story/singleplayer behavior, the next target is not dialog automation but the missing local SA-MP state transition: `StartGame`/menu suppression plus applying class-selection, spawn, camera, and script RPC state to GTA.

## 19. 2026-06-01 Graphics Hook Default Candidate

Fresh run after disabling dialog auto-response still showed the same user-visible singleplayer behavior. RakNet is now treated as good enough for this stage.

Archived logs:

- `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_graphics_hook_missing_singleplayer.log`
- `analysis/generated/asi_probe_logs/samp_net_trace_2026-06-01_graphics_hook_missing_singleplayer.log`

Important evidence:

- no automatic `RPC_DialogResponse` was sent.
- `RPC_InitGame` arrived, `RPC_RequestClass` returned outcome `1`, and server script/camera RPCs arrived.
- `RPC_RequestSpawn` returned outcome `0`, which is acceptable for now because gameplay/dialog flow may intentionally gate spawning.
- local GTA state moved `ADDR_ENTRY` `5 -> 7 -> 8 -> 9`; `ADDR_GAME_STARTED` moved `1 -> 0` after loading.
- the legacy code does not rely on network alone here: `GameInstallHooks()` patches the graphics loop at `0x53EB13`, and `TheGraphicsLoop()` calls `DoInitStuff()` and `CNetGame::Process()` every frame.
- the reimplementation had a graphics hook bridge, but it only enabled the legacy `0x53EB13` hook when an env flag was set. The monitor thread kept RakNet alive, so networking could work while the actual SA-MP frame loop was not proven to be active.

Fix applied:

- Online/debug mode now enables the legacy graphics hook by default.
- If no custom graphics hook address is configured, the runtime uses `0x53EB13`, matching `InstallGameAndGraphicsLoopHooks()` from the legacy client.
- runtime traces are written to `samp_runtime.log` even without `SAMPDLL_TRACE`.
- hook bridge traces are written to `samp_hook_trace.log` even without `SAMPDLL_TRACE`.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `1148fe4df1af7dbe58406555cba73d27df75a3199b22ad50eb617c91166d4750`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log`, `samp_net_trace.log`, `samp_runtime.log`, and `samp_hook_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_hook_trace.log` should contain a primary install success for the graphics hook at `0x53EB13`.
- `samp_runtime.log` should contain `hook_callback: call #1` and early `do_init_stuff` calls from the graphics callback path.
- if the hook installs and callbacks fire but the client remains in story/singleplayer behavior, the next missing layer is the local SA-MP frame/gameplay state: the legacy pre-connected `CNetGame::Process()` path repeatedly hides HUD and moves the local player/camera to the class-selection/lobby position until the server marks the game connected.

## 20. 2026-06-01 Graphics Callback Crash

The first candidate with default `0x53EB13` graphics hook crashed.

Archived logs:

- `analysis/generated/asi_probe_logs/samp_probe_2026-06-01_graphics_hook_callbacks_crash.log`
- `analysis/generated/asi_probe_logs/samp_net_trace_2026-06-01_graphics_hook_callbacks_crash.log`
- `analysis/generated/asi_probe_logs/samp_runtime_2026-06-01_graphics_hook_callbacks_crash.log`
- `analysis/generated/asi_probe_logs/samp_hook_trace_2026-06-01_graphics_hook_callbacks_crash.log`

Important evidence:

- `samp_hook_trace.log` proved the primary graphics hook installed successfully at `0x53EB13`.
- `samp_runtime.log` proved the graphics callback fired at least three times.
- The crash did not produce a logged structured exception in the runtime filter.
- The runtime still had the monitor thread calling `launch_do_init_stuff_compat()` every 16 ms while the graphics callback also called the same function. That means the network/init path could run concurrently from two threads, unlike the legacy client where `LaunchMonitor` exits after `StartGame()`.

Fix applied:

- `launch_do_init_stuff_compat()` now has a reentry guard.
- reentry skips are logged for the first few occurrences.
- once the graphics callback has fired, the monitor thread stops driving the init/network pump and becomes only the fallback/maintenance thread.
- before the first graphics callback, the monitor still drives init/network as a fallback in case the hook does not fire.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `8bcbd96cf0d7c05dee21afd441157924bd6e30fa5a0b5a0d3c1deef51f722357`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log`, `samp_net_trace.log`, `samp_runtime.log`, and `samp_hook_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- hook install and `hook_callback` should still appear.
- `do_init_stuff: reentry skip` may appear at most early in the run; it should not grow continuously.
- if the crash disappears but singleplayer remains, implement the missing local pre-connected SA-MP frame state rather than changing RakNet.

## 21. 2026-06-01 Pivot To Session-State Differential Probe

Fresh run after the graphics-callback reentry fix still reaches server-side join/connect but user-visible GTA remains in the singleplayer/story path. At this point RakNet is treated as good enough for the current phase.

Current interpretation:

- The missing behavior is no longer "connect to server".
- The missing behavior is the local SA-MP session transition after loading: the original client's graphics loop builds local UI/D3D state, creates `CNetGame`, processes class-selection/spawn/camera state every frame, and suppresses the stock GTA menu/story path.
- Our current runtime writes/clamps a few GTA flags, but the logs show GTA can still advance to `ADDR_ENTRY=9`, clear `ADDR_GAME_STARTED`, and briefly raise menu state. That means we are reacting to the singleplayer path rather than proving the original multiplayer state was established.

ASI probe upgrade applied:

- added decoded `state:` snapshots for the loading-to-session transition.
- snapshots include `ADDR_ENTRY`, start/game/menu/HUD flags, `0x52CF10` time-passing patch byte, script gate/call target, graphics-loop call target, game-process hook storage, Render2D storage, HWND/D3D/D3DDevice pointers, ped/vehicle table pointers, current player, camera mode, and loaded `samp.dll` image range.
- added passive watchpoints for the same high-value addresses plus bypass-video patch bytes.
- added `SAMP_PROBE_NO_STATE=1` and `SAMP_PROBE_STATE_ALWAYS=1` toggles.

Validation:

- `toolbox run -c devbuild ... tools/asi_probe/build_win32.sh`

Current deployed probe:

- `build-asi-probe-debug/samp_probe.asi`
- SHA-256 in prefix: `92535abb2e5b4e2d59a21942711f1233004edff4a97c8e9134e62c2eb0aacd92`
- deployed to the GTA Wine prefix as `samp_probe.asi`
- `samp_probe.log`, `samp_runtime.log`, `samp_hook_trace.log`, and `samp_net_trace.log` were cleared after deployment for the next run.

Next comparison target:

- Run the rebuilt DLL once with the new probe and archive `state:` lines.
- Run the original `samp.dll` through the same prefix/scenario with the same probe and archive `state:` lines.
- Diff the first transition where rebuilt and original diverge. The likely high-value checks are whether the original keeps `ADDR_ENTRY` out of story mode, when it clears/sets `ADDR_GAME_STARTED`, whether `0x53EB12/0x53EB13` targets the SA-MP graphics loop at the same point, and whether D3D/HUD/session pointers are initialized before `CNetGame::Process` starts.

## 22. 2026-06-01 Singleplayer Fallback After InitGame

Fresh probe run with the state-differential ASI confirms the rebuilt DLL receives the multiplayer handshake and initial server script RPCs, but GTA still falls back into the story/singleplayer state.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1610_singleplayer_after_initgame/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1610_singleplayer_after_initgame/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1610_singleplayer_after_initgame/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1610_singleplayer_after_initgame/samp_hook_trace.log`

Important evidence:

- `samp_runtime.log` reaches `RakNet state CONNECTED (InitGame observed)` and writes `entry 7->8 game_started 0->1`.
- `samp_probe.log` later shows GTA forcing `entry=9` and `game_started=0` while our graphics and script hooks remain installed.
- `samp_net_trace.log` contains the expected initial MP RPCs: `InitGame`, `ScrSetCameraPos`, `ScrSetCameraLookAt`, `ScrSetPlayerPos`, `ScrSetPlayerFacingAngle`, `ScrSetInterior`, `Weather`, and client messages.
- `time_patch=0x56` throughout the run, while the legacy `DoInitStuff()` path calls `pGame->ToggleThePassingOfTime(0)`, which patches `0x52CF10` to `0xC3`.

Fix applied:

- `DoInitStuff()` now applies the legacy time-passing patch in online mode: `0x52CF10 -> 0xC3`.
- After `InitGame`, `maintain_online_session_state()` now keeps `ADDR_ENTRY=8` and `ADDR_GAME_STARTED=1` active instead of only clearing menu/start flags.
- Dialog RPCs remain observe-only; no automatic dialog answering was added.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `2d2d50d7a4e1eccabf3360b2c513623fc9ae360aaa7125fbe6bca9dd904648e1`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log`, `samp_runtime.log`, `samp_net_trace.log`, and `samp_hook_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_runtime.log` should include `do_init_stuff: time passing disabled ... 0x56->0xc3`.
- `samp_probe.log` should show `time_patch=0xc3`.
- If GTA tries to force story mode again, `samp_runtime.log` should show `online_session_clamp: ... entry 9->8 game_started 0->1`.
- If the user still lands in singleplayer, the next required layer is applying the observed server RPCs to GTA state through the legacy script-command path, especially camera/player position/interior/weather.

## 23. 2026-06-01 Black Screen/Crash After Hard Entry Clamp

Fresh run after the time-passing patch reached the two-stage loading flow, then black screen/crash. This is progress beyond the earlier immediate singleplayer fallback, but the last candidate forced GTA into a state that the legacy client does not continuously force.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1712_black_screen_clamp_crash/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1712_black_screen_clamp_crash/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1712_black_screen_clamp_crash/samp_net_trace.log`

Important evidence:

- `samp_runtime.log` shows `InitGame`, `RequestClass` outcome `1`, and `RequestSpawn` outcome `0`; RakNet remains good enough for this stage.
- The crash path contains `online_session_clamp: ... entry 9->8 game_started 0->1`, followed by `exception_filter: code=0xc0000005 addr=0x015632b0 ... callbacks=0`.
- The ASI probe confirms the graphics-call patch points to our thunk, but the runtime graphics callback did not fire before the crash in this path.
- Comparing with both legacy sources (`sa-mp-legacy-rebuild/samp` and `gta-samp-0.2x-source-code`) shows the original `LaunchMonitor()` calls `pGame->StartGame()` once when `ADDR_ENTRY == 7` and then exits. It does not permanently re-clamp `ADDR_ENTRY` back to `8` after GTA advances.
- The original class-selection flow sends `RequestClass()` automatically, but `RequestSpawn()` is sent from `ProcessClassSelection()` only after the user presses SHIFT and the local player is cleared to spawn. The previous rebuild sent `RequestSpawn()` automatically and retried on outcome `0`, which is not the legacy client flow.

Fix applied:

- Kept the one-shot `StartGame` equivalent after `InitGame`, but changed `maintain_online_session_state()` to observe `entry/game_started` drift instead of writing them back continuously.
- The online maintainer still clears unexpected start/menu flags, but it no longer forces `ADDR_ENTRY=8` or `ADDR_GAME_STARTED=1` after GTA has moved on.
- Disabled automatic `RequestSpawn` by default. It is now only enabled with `SAMPDLL_AUTO_REQUEST_SPAWN=1`; dialogs remain observe-only.
- Extended RakNet diagnostics to decode and log the initial gameplay RPCs: player position, facing angle, weather, interior, camera position, and camera look-at. The new `network_prepare: game_rpc ... observe_only=1` line tells us exactly which state the server has provided without yet applying it to GTA memory/script commands.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `07896fa70507b58c582b4212dc4282c7a9174bc50cb445218f2210fd6f3798b5`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log`, `samp_runtime.log`, and `samp_net_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- No `online_session_clamp` line should appear.
- If GTA moves to `entry=9/game_started=0`, it should now be logged as `online_session_drift: ... action=observe_only`, not overwritten.
- `samp_net_trace.log` should show `rpc-state id=128 auto RequestSpawn disabled observe_only=1` after class approval.
- `samp_runtime.log` should emit `network_prepare: game_rpc ...` once camera/player/interior/weather RPCs arrive.
- If black screen remains without the clamp crash, the next implementation target is the missing local SA-MP application layer: applying the observed camera/player/interior/weather/class-selection state to GTA in the same thread/lifecycle window that the legacy `TheGraphicsLoop()` and `CNetGame::Process()` used.

## 24. 2026-06-01 Local MP Session Bridge Candidate

Fresh run after disabling the hard entry clamp no longer crashed, but the user-visible result remained singleplayer/story-mode. The server-side session and the initial gameplay RPCs are already present.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1733_singleplayer_before_mp_session_bridge/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1733_singleplayer_before_mp_session_bridge/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1733_singleplayer_before_mp_session_bridge/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1733_singleplayer_before_mp_session_bridge/samp_hook_trace.log`

Important evidence:

- `samp_runtime.log` shows `RakNet state CONNECTED (InitGame observed)`, `RequestClass` outcome `1`, and decoded game RPCs for player position, facing, weather, interior, camera position, and camera look-at.
- `samp_net_trace.log` shows `auto RequestSpawn disabled observe_only=1`, so the client is now following the legacy class-selection flow more closely.
- `samp_probe.log` shows the graphics callback is installed and firing, ped/vehicle tables eventually become visible, and GTA later drifts to `entry=9/game_started=0`.
- The legacy source confirms the original client calls `pNetGame->Process()` from `TheGraphicsLoop()` every frame. That process applies class-selection camera/player/interior/weather state locally; the previous rebuild only logged those RPCs.

DLL side-check:

- `BASS.dll` is audio/stream handling in the legacy client. It is loaded successfully in the current runtime and does not explain a valid server join plus local story-mode fallback.
- `d3dx9_25.dll` is needed for SA-MP UI drawing, fonts, sprites, and loading/class-selection presentation. Missing D3DX behavior can explain absent SA-MP artwork/text, but not the failure to apply camera/player/interior state.
- `samp.saa`/asset hooks remain relevant for the white loading image and UI resources, but the current blocker is still the local session-processing layer.

Fix applied:

- Added a minimal local MP session bridge in `runtime_bridge.c`, invoked from the graphics-loop callback.
- The bridge copies decoded RakNet game RPC data into runtime state and applies it on the GTA main/render thread for the first session frames.
- It uses the legacy script-command route through `CRunningScript::ProcessOneCommand` for `set_camera_position`, `point_camera`, `select_interior`, `link_actor_to_interior`, `refresh_streaming_at`, `set_actor_z_angle`, and `toggle_player_controllable`.
- It applies weather directly to GTA weather globals, hides HUD/radar for class selection, writes ped facing/health defensively, and teleports the local ped to the server-provided class-selection position.
- Dialog automation remains disabled, and `RequestSpawn` remains disabled unless `SAMPDLL_AUTO_REQUEST_SPAWN=1` is explicitly set.

Validation:

- `cmake --build build-host`
- `ctest --test-dir build-host --output-on-failure` (`5/5` passed)
- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `abbabd1776001d0c8e49ecfd1bf96371bcca1b79de00f5de1b63c4f5a32ad816`
- deployed to the GTA Wine prefix as `samp.dll`
- `samp_probe.log`, `samp_runtime.log`, `samp_net_trace.log`, and `samp_hook_trace.log` were cleared after deployment for the next run.

Expected signal in the next trace:

- `samp_runtime.log` should contain `mp_session_bridge: apply #...` lines after `network_prepare: game_rpc ...`.
- If the script-command bridge works, the camera should be forced to the server class-selection camera instead of drifting through story-mode camera control.
- If it still shows black screen/singleplayer, the next check is whether script commands fail, whether local actor handle `1` is wrong for `set_actor_z_angle/link_actor_to_interior`, or whether the original client creates/recreates `CPlayerPed` before these RPCs are applied.

## 25. 2026-06-01 SA-MP Loading/Class State Reached

Fresh run with the local MP session bridge no longer entered story mode and did not crash. The client reached the SA-MP loading/class-selection camera state: visually a teal/blue high-altitude loading screen, with the expected wind ambience, but no further transition into controllable gameplay.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1755_minimal_scm_samp_loading_stuck/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1755_minimal_scm_samp_loading_stuck/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1755_minimal_scm_samp_loading_stuck/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1755_minimal_scm_samp_loading_stuck/samp_hook_trace.log`

Important evidence:

- `InitGame` and `RequestClass` are clean: `RequestClass` returns outcome `1`.
- The server sends the class/loading camera state at `pos=(-1.000,-0.968,2499.743)`, `cam=(0.000,-8.838,2500.000)`, `look=(0.000,0.000,2500.000)`.
- The graphics callback fires and the local bridge applies repeatedly without crashing.
- The client remains at `entry=9/game_started=0`, which matches the SA-MP class/loading stage, not the GTA story intro.
- The blocker is now explicit in `samp_net_trace.log`: `auto RequestSpawn disabled observe_only=1`. We intentionally stopped before the next legacy step.

Legacy comparison:

- In `netrpc.cpp`, `RequestClass` reads `byteRequestOutcome` and a full `PLAYER_SPAWN_INFO`; when accepted, it stores that spawn info and marks class selection as accepted.
- `CLocalPlayer::RequestSpawn()` sends empty RPC `129`.
- On accepted `RequestSpawn`, `CLocalPlayer::Spawn()` restores camera, shows HUD, enables player control, refreshes streaming, applies skin/position/rotation, disables fade, and finally sends RPC `52` (`Spawn`) to notify the server.

Fix applied for next candidate:

- `RequestClass` decoding now extracts and logs the embedded `PLAYER_SPAWN_INFO`.
- Automatic `RequestSpawn` is enabled by default again after accepted class selection; it can still be disabled with `SAMPDLL_AUTO_REQUEST_SPAWN=0`.
- Dialog RPCs remain observe-only. No automatic dialog response was added.
- The runtime bridge now detects accepted `RequestSpawn` outcome `1` or `2` and runs a one-shot spawn finalization path: restore camera, jumpcut, set camera behind player, enable HUD/radar, toggle player controllable, set entry/game-start flags, request/load/apply skin, patch the camera fade return, and teleport to the decoded spawn position.
- After spawn is accepted, the bridge stops applying the high-altitude class-selection camera so it should not pin the client back to the loading view.

Validation:

- `reimpl/scripts/build_in_devbuild_toolbox.sh`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `514a7debdef42c6a8cb68dd6f5fb795fa2523a04e5070c5e321b985942eca022`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were archived under `analysis/generated/asi_probe_logs/2026-06-01_1805_autospawn_spawn_finalize_candidate/` and then cleared for the next run.

Expected signal in the next trace:

- `samp_net_trace.log` should show `rpc-state spawn_info source=RequestClass ...`.
- `samp_net_trace.log` should show `rpc-auto-out id=129 name=RequestSpawn ...`.
- If the server accepts spawn, `samp_net_trace.log` should show `rpc-state id=129 request_spawn_outcome=1` or `2`, followed by `rpc-auto-out id=52 name=SpawnNotify`.
- `samp_runtime.log` should show `mp_session_bridge: spawn_finalize ...`.
- If the client still sticks in the loading view, the next likely gap is in local ped creation/model application or missing SA-MP UI/input state, not RakNet connect or story suppression.

## 26. 2026-06-01 Auto RequestSpawn Still Denied

Fresh run with automatic `RequestSpawn` and spawn-finalization code still remained in the sky/loading view.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1810_autospawn_still_sky/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1810_autospawn_still_sky/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1810_autospawn_still_sky/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1810_autospawn_still_sky/samp_hook_trace.log`

Important evidence:

- The new code worked far enough to decode spawn info from `RequestClass`: `team=255 skin=45 pos=(0.000,0.000,0.000) rot=0.000`.
- `RequestSpawn` is now sent automatically: `rpc-auto-out id=129 name=RequestSpawn attempt=1`.
- The server rejects every spawn request: five `RPC_RequestSpawn` replies arrive with `request_spawn_outcome=0`.
- Because outcome is `0`, `spawn_ready=0`, and the one-shot `mp_session_bridge: spawn_finalize ...` path correctly does not run.
- The local client remains in `entry=9/game_started=0`, camera `15`, HUD hidden, which is the class/loading sky state.

Current conclusion:

- The latest blocker is not the local finalization path. The server is explicitly not allowing spawn yet.
- The real client comparison should focus on what the original `samp.dll` sends or applies between `RequestClass` accepted and the first accepted `RequestSpawn`.
- Strong candidates are a missing local class-selection state flag/SHIFT-equivalent, a dialog-driven gamemode gate, or a required sync/update RPC before the server accepts `RequestSpawn`.
- Dialog automation must remain disabled; the next analysis should observe the original DLL flow rather than blindly auto-answering dialogs.

## 27. 2026-06-01 Original DLL Pre-Connect Frontend Difference

The original `samp.dll` was swapped into the prefix for comparison.

Important evidence:

- Prefix `samp.dll` SHA-256 matched the original reference DLL: `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
- Before connecting to the server, the original client already shows the SA-MP frontend state: Santa Maria Beach camera, no story intro, and the top-left SA-MP startup line.
- Probe trace confirms the original installs its graphics-loop hook before this state, then reaches `entry=9/game_started=0/camera=15` with ped/vehicle pools allocated before network connect.
- The legacy code matches this: `CNetGame::Process()` teleports the local player to `1500.0,-887.0979,32.56055`, points the camera from `1497.803,-887.0979,62.56055` at `1406.65,-795.7716,82.2771`, and delays the actual RakNet connect by roughly 3000 ms.
- Our reimplementation was connecting from the primed `DoInitStuff()` path before this pre-connect frontend could be established.

Fix applied for next candidate:

- Added a pre-connect frontend bridge that waits for the local ped, forces the Santa Maria camera/teleport state, hides HUD/radar, sets `entry=9/game_started=0`, and delays RakNet connect until that frontend has been visible for the configured pre-connect delay.
- Added `SAMPDLL_PRECONNECT_DELAY_MS` override; default remains 3000 ms.
- `RequestSpawn` automation now defaults off and is explicitly suppressed while a server dialog is pending. It can still be enabled for controlled experiments with `SAMPDLL_AUTO_REQUEST_SPAWN=1`, but dialog-driven servers will not be bypassed.

Expected signal in the next trace:

- `samp_runtime.log` should show `preconnect_bridge: ready ... cam=(1497.803,-887.098,62.561) look=(1406.650,-795.772,82.277) delay_ms=3000`.
- The server connection should begin only after that line and the delay.
- `samp_net_trace.log` should show `auto RequestSpawn suppressed pending_dialog ...` if the gamemode sends the language dialog first.

## 28. 2026-06-01 InitGame Frontend Hold Fix

Fresh reimplementation run reached the Santa Maria frontend and the server-side dialog/InitGame flow, but then crashed immediately after the runtime promoted GTA state to ingame.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_1919_preconnect_reimpl_crash/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1919_preconnect_reimpl_crash/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1919_preconnect_reimpl_crash/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_1919_preconnect_reimpl_crash/samp_hook_trace.log`

Important evidence:

- `preconnect_bridge: ready ... entry=9 game_started=0 ...` confirms the pre-connect Santa Maria frontend is now established before RakNet connect.
- The server sends the language dialog first: `rpc-state id=61 dialog_id=93 style=2 observe_only=1`.
- `InitGame` arrives and `RequestClass` is sent, so RakNet bootstrap is still healthy.
- The crash happens immediately after `startgame_flags: mode=LEGACY_INGAME state=2 entry 9->8 game_started 0->1`.
- Probe confirms the faulting instruction pointer `0x015632b0` is outside `samp.dll`, and the access violation is a write to `0x28`, consistent with entering a GTA game state whose target object is not valid yet.

Fix applied for next candidate:

- `InitGame` now only promotes the internal NetGame state to CONNECTED.
- GTA frontend flags remain held at `entry=9/game_started=0`, HUD hidden, radar blank, and menu/start flags clear until an accepted `RequestSpawn` reply is observed.
- `maintain_online_session_state()` now explicitly preserves that frontend/class-selection state while awaiting spawn acceptance.
- `apply_multiplayer_session_bridge_compat()` also keeps the frontend hold during class-select/game RPC camera updates.
- Dialogs remain observe-only and `RequestSpawn` automation remains disabled by default.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `61c084d42b4ff4f36043b01916919bf1a65e3aedb6952b9252a07d786e2424a8`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `samp_runtime.log` should show `network_prepare: RakNet state CONNECTED (InitGame observed)`.
- It should then show `session_frontend_hold: reason=initgame_no_spawn ...`, not `startgame_flags: mode=LEGACY_INGAME`.
- If the client stays alive, the next blocker is likely class-selection/dialog UI state or missing UI rendering, not the pre-connect or InitGame transition.

## 29. 2026-06-01 Manual Dialog/Spawn Gate Candidate

Fresh run with candidate `61c084d42b4ff4f36043b01916919bf1a65e3aedb6952b9252a07d786e2424a8` stayed alive in the SA-MP frontend/class-selection state, but did not progress into controllable gameplay.

Archived logs:

- `analysis/generated/asi_probe_logs/2026-06-01_2049_manual_dialog_needed/samp_runtime.log`
- `analysis/generated/asi_probe_logs/2026-06-01_2049_manual_dialog_needed/samp_net_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_2049_manual_dialog_needed/samp_probe.log`
- `analysis/generated/asi_probe_logs/2026-06-01_2049_manual_dialog_needed/samp_hook_trace.log`
- `analysis/generated/asi_probe_logs/2026-06-01_2049_manual_dialog_needed/cleo.log`

Important evidence:

- The pre-connect Santa Maria frontend now works before RakNet connect: `preconnect_bridge: ready ... cam=(1497.803,-887.098,62.561) look=(1406.650,-795.772,82.277) delay_ms=3000`.
- `InitGame` is observed and the runtime correctly holds GTA at `entry=9/game_started=0` instead of promoting into story mode or crashing.
- The server sends `RPC_ScrDialogBox` (`id=61`) with dialog id `93`, style `2`, title bytes for the language dialog.
- `RequestClass` is sent and accepted, and spawn info is decoded: `skin=45 pos=(0.000,0.000,0.000)`.
- `RequestSpawn` is suppressed because a dialog is pending: `auto RequestSpawn suppressed pending_dialog id=93 observe_only=1`.
- Runtime repeats the class camera at `pos=(-1.000,-0.968,2499.743)`, `cam=(0.000,-8.838,2500.000)`, which is the expected hold while the client is not cleared to spawn.

Legacy comparison:

- The 0.2x client path still matches this model: `InitGame` calls `HandleClassSelection()`, `RequestClass` stores `PLAYER_SPAWN_INFO` and marks `m_bClearedToSpawn`, and `ProcessClassSelection()` sends `RequestSpawn()` only after the 2000 ms selection delay and a manual SHIFT press.
- The original client renders this via D3D/DXUT UI (`CSpawnScreen`, `CChatWindow`, `CCmdWindow`) from the D3D hook. We do not have the 0.3.7 dialog UI yet, so the immediate unblocker is a small manual Win32 dialog bridge.

Fix prepared for next candidate:

- `RPC_ScrDialogBox` is decoded with RakNet `StringCompressor`.
- A manual topmost Win32 dialog window is opened for the server dialog; the user selects the language and confirms. This sends `RPC_DialogResponse` (`id=62`) only after manual input.
- Dialogs are not auto-answered.
- After the dialog is gone, class selection follows the legacy gate: accepted class, 2000 ms delay, then manual SHIFT schedules `RPC_RequestSpawn` (`id=129`).

Current build candidate:

- `build-win32/samp.dll`
- SHA-256: `fec92a2035671c8dd4c77ae62d0c6420a33f0212efd4c476f5ced5034bfa7640`

Expected signal in the next trace:

- `samp_net_trace.log` should show `rpc-state id=61 ... manual_response=1`.
- It should show `dialog-ui: opened manual dialog window ...`.
- After the user confirms the dialog, it should show `rpc-manual-out id=62 name=DialogResponse ...`.
- After class selection is accepted and the user presses SHIFT, it should show `rpc-manual: scheduled RequestSpawn from SHIFT key` and then `rpc-auto-out id=129 name=RequestSpawn ...`.

## 30. 2026-06-01 Pre-Connect Chat Overlay Gap

User observation after the manual-dialog candidate: the pre-connect frontend is still visibly incomplete because the top-left SA-MP status/chat lines are missing. This is a separate frontend/UI gap from the RakNet/session-state work.

Legacy comparison:

- 0.2x/legacy creates `CChatWindow` from the D3D setup path and draws it every frame from `IDirect3DDevice9Hook`.
- `CNetGame::Process()` writes the status lines through `pChatWindow->AddDebugMessage(...)`, including `Connecting to %s:%d...`.
- On connection bootstrap, the legacy path also writes connection/join messages before the server dialogs become visible.

Fix prepared for next candidate:

- Added a small `chat_compat` queue in the runtime for status messages.
- Added a quick GDI overlay renderer as an interim frontend probe. It compensates for the centered 4:3 viewport inside the wider Wine window, so the text should land inside the game viewport instead of the black side bar.
- Wired visible messages for:
  - `SA-MP 0.3.7-R5 Started`
  - `Connecting to <host>:<port>...`
  - `Connected. Joining the game...`
- This does not answer dialogs automatically and does not change the manual SHIFT spawn gate.
- Escape hatch: `SAMPDLL_CHAT_OVERLAY=0`.
- Vertical tuning: `SAMPDLL_CHAT_Y=<pixels>`.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `831b3ef90bd8266c578d2e78e7de07f53cbf04bf5ad97b45a2d4394fbd7c83f7`
- deployed to the GTA Wine prefix as `samp.dll`

Expected signal in the next trace:

- `samp_runtime.log` should include `chat_compat: SA-MP 0.3.7-R5 Started`.
- It should include `chat_compat: Connecting to 127.0.0.1:7777...`.
- After InitGame, it should include `chat_compat: Connected. Joining the game...`.
- If the overlay actually draws, it should include `chat_overlay: drawing enabled hwnd=... lines=...`.

## 31. 2026-06-01 D3D Chat Renderer Candidate

User run with candidate `831b3ef90bd8266c578d2e78e7de07f53cbf04bf5ad97b45a2d4394fbd7c83f7` confirmed the bootstrap chat text is visible in the pre-connect frontend, but the GDI overlay is only an interim probe and can disappear once GTA owns the D3D frame.

Legacy comparison:

- The 0.2x client keeps chat in a ring buffer and draws it from the D3D render path.
- `CChatWindow::AddClientMessage()` converts SA-MP message color with `(dwColor >> 8) | 0xFF000000`.
- `CChatWindow::RenderText()` draws four black outline passes and then the final colored text using `ID3DXFont::DrawText`.

Fix applied for next candidate:

- Added a D3D9 `EndScene` vtable hook once the GTA `IDirect3DDevice9` pointer at `0x00C97C28` is available.
- The D3D device/vtable pointers are guarded with `VirtualQuery` before patching to avoid crashing on early or invalid device state.
- Added a dynamically resolved `D3DXCreateFontA` path against `d3dx9_25.dll` and an `ID3DXFont`-compatible vtable shim, without adding a static import.
- The runtime chat renderer now draws via D3D with SA-MP-style outline and keeps the old GDI pass as fallback/probe.
- The chat queue now stores per-line ARGB colors.
- RakNet RPC `93` (`ClientMessage`) is decoded into an 8-entry probe ring and copied into the runtime chat queue, so server-side info/chat messages are rendered instead of only the local bootstrap lines.
- Dialogs remain manual; this change does not alter the dialog response or SHIFT spawn gate.
- Escape hatches/tuning:
  - `SAMPDLL_CHAT_OVERLAY=0`
  - `SAMPDLL_CHAT_Y=<pixels>`
  - `SAMPDLL_CHAT_FONT_SIZE=<8..32>`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `8becdc2e68dbb88583ff79b0d287fb5c44df1e6226baa00cdfeaec4c0682d861`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `samp_runtime.log` should show `chat_d3d: EndScene hook installed ...`.
- It should show `chat_d3dx: font created ...` and then `chat_d3dx: drawing enabled ...`.
- `samp_net_trace.log` should show `rpc-state id=93 client_message seq=... color=... text='...'` for server messages.
- Those decoded server messages should appear in the same upper-left chat area as the bootstrap lines.

## 32. 2026-06-01 White Loading Regression Fix

User run with candidate `8becdc2e68dbb88583ff79b0d287fb5c44df1e6226baa00cdfeaec4c0682d861` regressed to the white GTA loading screen.

Important evidence:

- Runtime never reached RakNet connect; this is a frontend/loading problem, not a network problem.
- The last repeated state was `preconnect_bridge: waiting for local ped ... entry=8 game_started=0`.
- The new D3D `EndScene` hook was installed very early, while the client was still in the white loading/entry 7 state.
- No `preconnect_bridge: ready ...` line was emitted.

Fix applied for next candidate:

- D3D chat rendering is no longer hooked during the early white loading phase. It is delayed until `preconnect_ready=1`.
- Escape hatch: `SAMPDLL_CHAT_D3D=0`; early hook can be forced only with `SAMPDLL_CHAT_D3D_EARLY=1`.
- Preconnect no longer blocks forever on `FindPlayerPed == NULL`. After a short entry-8 fallback window, it can continue to set the Santa Maria camera without a local ped.
- Strict old behavior can be forced with `SAMPDLL_PRECONNECT_REQUIRE_PED=1`.
- RakNet was intentionally ignored for this analysis per user instruction.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `14371c3ced38b6453afd81400caa31a6f695271fb9d98deeb9187c63f2db8f3b`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `chat_d3d: EndScene hook installed ...` should appear only after the frontend is ready.
- If no local ped appears, runtime should log `preconnect_bridge: continuing without local ped ...` and then `preconnect_bridge: ready ped=0x00000000 no_ped_fallback=1 ...`.
- The white loading screen should advance to the Santa Maria frontend rather than waiting until `apply=7200`.

## 33. 2026-06-01 World Render Settle Fix

User run with candidate `14371c3ced38b6453afd81400caa31a6f695271fb9d98deeb9187c63f2db8f3b` improved chat rendering and server message visibility, but the world still rendered as the white GTA loading screen behind the UI.

Important evidence:

- Chat and the manual dialog draw correctly, including server `ClientMessage` lines.
- Runtime can reach `preconnect_bridge: ready ...` and the D3D chat hook draws.
- Probe still shows the game at `entry=9/game_started=0` while the world is visually still the loading screen.
- In older/reference-good traces, GTA spent time in `entry=8/game_started=1` after world/ped tables appeared before switching to SA-MP's `entry=9/game_started=0` frontend.

Fix applied for next candidate:

- When the local ped first appears, preconnect now holds `entry=8/game_started=1` for a 4000 ms world-settle window before switching to `entry=9/game_started=0`.
- This gives GTA time to finish world/streaming/render setup before the SA-MP frontend camera is applied.
- Without a local ped, preconnect now warms GTA at `entry=8/game_started=1` instead of treating the white loading screen as ready.
- The no-ped frontend fallback is disabled by default. It can still be explicitly allowed with `SAMPDLL_PRECONNECT_REQUIRE_PED=0`.
- RakNet was not used as the diagnosis target here; the issue is loading/world state ordering.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `deda4f86269299d7877ac06cef4ccac6396b6412044204f25ee49f9c01ead5b6`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `preconnect_bridge: local ped observed ... settle_ms=4000`
- repeated or at least one `preconnect_bridge: settling world ... entry=8 game_started=1`
- only after the settle window: `preconnect_bridge: ready ped=... entry=9 game_started=0 ...`
- visually, the white loading screen should clear before chat/dialog interaction continues.

## 34. 2026-06-01 Post-Loading Close After World Settle

User run with candidate `deda4f86269299d7877ac06cef4ccac6396b6412044204f25ee49f9c01ead5b6` closed immediately after the loading screen.

Important evidence:

- Runtime crashes before any network/session callback work: `net=0`, `callbacks=0`.
- The fault is the same GTA-side access violation seen in earlier hard-entry experiments: `exception_filter: code=0xc0000005 addr=0x015632b0 ... entry=8`.
- The crash happens during the new post-ped settle window:
  - `preconnect_bridge: local ped observed ... entry=8 game_started=1 settle_ms=4000`
  - `preconnect_bridge: settling world ... elapsed_ms=32/4000 entry=8 game_started=1`
- This proves that holding GTA in `entry=8/game_started=1` after the local ped appears is unsafe with the current story-suppression and frontend patches.

Fix applied for next candidate:

- The default post-ped world-settle window is now `0 ms`.
- Preconnect still uses `entry=8/game_started=1` to warm the world while no local ped exists.
- As soon as the local ped is observed, the bridge switches directly to the SA-MP pre-connect state (`entry=9/game_started=0`) and applies the Santa Maria camera.
- The unsafe settle window remains available only as an explicit diagnostic knob: `SAMPDLL_PRECONNECT_WORLD_SETTLE_MS=<0..10000>`.
- RakNet is intentionally ignored for this stage.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `3b9cbc7b8ea8e1f46bf0a2d967eb85ac18944dd33115062cead919de2e8bdb31`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `preconnect_bridge: warming world without local ped ... entry=8 game_started=1`
- `preconnect_bridge: local ped observed ... settle_ms=0`
- no `preconnect_bridge: settling world ...` lines by default
- then `preconnect_bridge: ready ... entry=9 game_started=0 ...`

## 35. 2026-06-01 Chat Renderer Flicker Fix

User run with candidate `3b9cbc7b8ea8e1f46bf0a2d967eb85ac18944dd33115062cead919de2e8bdb31` reached the Santa Maria frontend again and rendered chat, but the chat text flickered and visually overlapped.

Important evidence:

- Runtime logged both chat render paths in the same run:
  - `chat_overlay: drawing enabled ...`
  - `chat_d3dx: drawing enabled ...`
- This means the unsynchronized GDI fallback was still drawing into the window while the D3D `EndScene` renderer was active.
- Server chat still displayed raw SA-MP inline color tags such as `{CCCCCC}`, making lines longer and harder to read.

Fix applied for next candidate:

- Once the D3D `EndScene` hook is installed, the GDI overlay returns immediately and no longer draws in parallel.
- GDI remains available only as a fallback before D3D is hooked or when D3D chat is disabled.
- Default chat line height was increased from `17` to `18`.
- `{RRGGBB}` inline color tags are stripped from visible text for now. Full segment-colored chat rendering remains a later parity step.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `e27552570a4a234358e48a05e0ff9bcd494033183afec7515f3857ae38fdbda3`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

Expected signal in the next trace:

- `chat_d3d: EndScene hook installed ...`
- `chat_d3dx: drawing enabled ...`
- no `chat_overlay: drawing enabled ...` after D3D hook installation in the normal path
- server chat text should no longer contain literal `{CCCCCC}`/`{99CC00}` tags

## 36. 2026-06-01 Preconnect Camera Source Parity

The original/source pre-connect camera path was verified in:

- `/home/chairman/Projects/sa-mp-legacy-rebuild/samp/client/net/netgame.cpp`
- `/home/chairman/Projects/sa-mp.dll-rebuild/gta-samp-0.2x-source-code/saco/net/netgame.cpp`

Relevant legacy behavior:

- While not connected, `CNetGame::Process()` repeatedly obtains `pGame->FindPlayerPed()` and `pGame->GetCamera()`.
- If the player is in a vehicle, it calls `pPlayer->RemoveFromVehicleAndPutAt(1500.0f,-887.0979f,32.56055f)`.
- Otherwise it calls `pPlayer->TeleportTo(1500.0f,-887.0979f,32.56055f)`.
- It then calls:
  - `pCamera->SetPosition(1497.803f,-887.0979f,62.56055f,0.0f,0.0f,0.0f)`
  - `pCamera->LookAtPoint(1406.65f,-795.7716f,82.2771f,2)`
  - `pGame->DisplayHud(FALSE)`
- The camera wrapper itself is just script commands `set_camera_position` and `point_camera`.

Fix applied for next candidate:

- The preconnect coordinates were already source-identical.
- The missing vehicle branch is now implemented:
  - `IN_VEHICLE` is mirrored as `dwStateFlags & 0x100` at ped offset `1132`.
  - If set, preconnect uses script opcode `0x0362` (`remove_actor_from_car_and_put_at`) with local actor id `1`.
  - Otherwise it keeps the existing `TeleportTo`-style path.
- Camera position and look-at still use opcodes `0x015F` and `0x0160`, matching the legacy `CCamera` wrapper.

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `0d590f521e223406b77aec1c6229d8ed5b4eb569bbe80a11b73b52410c855550`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.

## 37. 2026-06-01 Crash After Enabling Source Vehicle Branch

User run with candidate `0d590f521e223406b77aec1c6229d8ed5b4eb569bbe80a11b73b52410c855550` crashed directly after the loading/preconnect transition.

Important evidence:

- Runtime reached the Santa Maria preconnect stage:
  - `preconnect_bridge: ready ... entry=9 game_started=0 ...`
  - D3D chat hook and font were created.
- Crash then occurred before networking:
  - `exception_filter: code=0xc0000005 addr=0x004d463c phase=7 entry=9 target=7 net=0 hooks=1 callbacks=0`
- Probe reports a read from `0x3c`, still outside `samp.dll`.
- The only new behavior in this candidate was the source-derived `RemoveFromVehicleAndPutAt` branch in preconnect positioning.

Fix applied for next candidate:

- The source vehicle branch remains implemented but is no longer enabled by default.
- Default preconnect positioning returns to the previous stable `TeleportTo`-style path.
- The vehicle-removal branch can be explicitly enabled for diagnostics with:
  - `SAMPDLL_PRECONNECT_REMOVE_FROM_VEHICLE=1`

Current deployed candidate:

- `build-win32/samp.dll`
- SHA-256: `c0c1d5cc0800deb5143adf9383eab418c6c9663bd7f228b61f0754bd55efec38`
- deployed to the GTA Wine prefix as `samp.dll`
- previous prefix logs were cleared for the next run.
