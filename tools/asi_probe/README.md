# SA-MP ASI Probe

`samp_probe.asi` is an in-process analysis helper for the original or rebuilt `samp.dll`.

It is intended for local reverse-engineering and compatibility work:

1. wait for `samp.dll` to load,
2. dump its PE identity, sections, imports, and IAT targets,
3. hook selected `samp.dll` IAT entries,
4. log module-attributed Winsock/loader/patch calls,
5. optionally patch selected `WSOCK32.dll` exports inline,
6. optionally patch known `samp.dll` network RVAs for plaintext RakNet payloads,
7. watch known GTA-SA memory locations that SA-MP commonly patches,
8. emit decoded `state:` snapshots for the loading-to-session transition.

The first pass rewrites selected import slots inside `samp.dll`, so observed calls are attributable to `samp.dll` rather than process-global Wine/WinDbg noise.

If the target bypasses its visible IAT, the probe also installs process-wide inline hooks on selected `WSOCK32.dll` exports. These hooks log only when the return address is inside the loaded `samp.dll` image, and each line includes `caller_rva`. `send`, `sendto`, successful `recv`, and successful `recvfrom` lines include a first-256-byte payload hex preview. Set `SAMP_PROBE_LOG_ALL_API=1` only when intentionally debugging process-wide noise.

For the currently observed packed prefix DLL, the probe keeps guarded internal hook candidates for:

- `samp.SocketLayer.SendTo` at RVA `0x0004ffc0`, before the SA-MP client datagram transform.
- `samp.ProcessNetworkPacket` at RVA `0x0003b950`, after `recvfrom` and before RakNet packet handling.

`PROBE_TRACE` from the R5 golden run on 2026-06-05 shows these RVAs are stale for the tested original DLL, so internal `samp.dll` code hooks are disabled by default. Enable them only for a targeted verification run with `SAMP_PROBE_ENABLE_SAMP_CODE_HOOKS=1` or `samp_probe_samp_code_hooks.flag`. The normal socket trace now labels observed R5 Winsock callsites such as `samp.dll+0x00053b19` and `samp.dll+0x00053a57` without patching those callsites.

The hook matcher supports both named imports and selected `WSOCK32.dll` ordinals. This matters for packed/protected SA-MP DLLs that expose only sparse ordinal imports such as `WSOCK32 ordinal 17` (`recvfrom`).

## Build

On a host with MinGW:

```bash
tools/asi_probe/build_win32.sh
```

Through the reverse-engineering toolbox:

```bash
toolboxes/reverse-engineering/run.sh \
  bash -lc 'cd /home/chairman/Projects/sa-mp.dll-rebuild && tools/asi_probe/build_win32.sh'
```

Debug build through the `devbuild` toolbox, with DWARF debug sections and a linker map:

```bash
toolbox run -c devbuild bash -lc 'cd /home/chairman/Projects/sa-mp.dll-rebuild && \
  SAMP_PROBE_BUILD_DIR=build-asi-probe-debug \
  SAMP_PROBE_BUILD_TYPE=Debug \
  SAMP_PROBE_STRIP=0 \
  tools/asi_probe/build_win32.sh'
```

Output:

```text
build-asi-probe/samp_probe.asi
```

Debug output:

```text
build-asi-probe-debug/samp_probe.asi
build-asi-probe-debug/samp_probe.map
```

## Use

Copy `samp_probe.asi` next to `gta_sa.exe` in a setup that already has an ASI loader. Run the original client or the rebuild through the same scenario.

Default log output:

```text
samp_probe.log
```

The log is written next to the ASI file when that path can be resolved.

## Kill Switches

Create this file next to the ASI to disable IAT patching while keeping passive PE/import/watch dumps:

```text
samp_probe_no_hooks.flag
```

Environment variables are also supported:

```text
SAMP_PROBE_NO_HOOKS=1
SAMP_PROBE_NO_INLINE_HOOKS=1
SAMP_PROBE_NO_SAMP_CODE_HOOKS=1
SAMP_PROBE_ENABLE_SAMP_CODE_HOOKS=1
SAMP_PROBE_NO_WATCH=1
SAMP_PROBE_NO_STATE=1
SAMP_PROBE_STATE_ALWAYS=1
SAMP_PROBE_LOG_ALL_API=1
SAMP_PROBE_ASSET_PATHS=1
SAMP_PROBE_FILE_HOOKS=1
```

The asset trace can also be toggled through files next to the ASI:

```text
samp_probe_asset_paths.flag
samp_probe_file_hooks.flag
samp_probe_samp_code_hooks.flag
```

Use `samp_probe_asset_paths.flag` for normal original-DLL golden traces. It logs interesting SA-MP asset opens, size queries, seeks, and closes. `samp_probe_file_hooks.flag` additionally hooks `ReadFile`; keep that for short, targeted runs only because original 0.3.7 performs large overlapped reads against the SAMP archives.

## First Questions This Should Answer

1. Does the original `samp.dll` call Winsock through its own `WSOCK32.dll` IAT as expected?
2. If not, which `WSOCK32.dll` export is called through a dynamic/unpacked function pointer?
3. Which exact callsite RVA triggers `socket`, `connect`, `sendto`, `recvfrom`, or `select`?
4. Which exact imported calls happen before the first `Connecting to ...` UI banner?
5. Which GTA-SA code/data addresses change when SA-MP installs runtime hooks?
6. Does our rebuilt DLL produce the same module-attributed call sequence under the same scenario?
7. Are current Wine trace PASS rows real `samp.dll` calls or process-global side effects?
8. At the exact point where GTA leaves the loading screen, do `ENTRY`, menu flags, HUD, D3D pointers, and the graphics/game-loop hook targets match the original client?
