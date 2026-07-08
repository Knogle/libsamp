# SA-MP ASI Probe

`samp_probe.asi` is an in-process analysis helper for the original or rebuilt `samp.dll`.

It is intended for local reverse-engineering and compatibility work:

1. wait for `samp.dll` to load,
2. dump its PE identity, sections, imports, and IAT targets,
3. hook selected `samp.dll` IAT entries,
4. log module-attributed Winsock/loader/patch calls,
5. optionally patch selected `WSOCK32.dll` exports inline,
6. optionally patch known `samp.dll` network RVAs for plaintext RakNet payloads,
7. optionally patch selected GTA-SA asset-loading functions for original-DLL
   object loading traces,
8. watch known GTA-SA memory locations that SA-MP commonly patches,
9. emit decoded `state:` snapshots for the loading-to-session transition.

The first pass rewrites selected import slots inside `samp.dll`, so observed calls are attributable to `samp.dll` rather than process-global Wine/WinDbg noise.

If the target bypasses its visible IAT, the probe also installs process-wide inline hooks on selected `WSOCK32.dll` exports. These hooks log only when the return address is inside the loaded `samp.dll` image, and each line includes `caller_rva`. `send`, `sendto`, successful `recv`, and successful `recvfrom` lines include a first-256-byte payload hex preview. Set `SAMP_PROBE_LOG_ALL_API=1` only when intentionally debugging process-wide noise.

For the currently observed packed prefix DLL, the probe keeps guarded internal hook candidates for:

- `samp.SocketLayer.SendTo` at RVA `0x0004ffc0`, before the SA-MP client datagram transform.
- `samp.ProcessNetworkPacket` at RVA `0x0003b950`, after `recvfrom` and before RakNet packet handling.

`PROBE_TRACE` from the R5 golden run on 2026-06-05 shows these RVAs are stale for the tested original DLL, so internal `samp.dll` code hooks are disabled by default. Enable them only for a targeted verification run with `SAMP_PROBE_ENABLE_SAMP_CODE_HOOKS=1` or `samp_probe_samp_code_hooks.flag`. The normal socket trace now labels observed R5 Winsock callsites such as `samp.dll+0x00053b19` and `samp.dll+0x00053a57` without patching those callsites.

The hook matcher supports both named imports and selected `WSOCK32.dll` ordinals. This matters for packed/protected SA-MP DLLs that expose only sparse ordinal imports such as `WSOCK32 ordinal 17` (`recvfrom`).

## Goals And Non-Goals

The probe is meant to answer concrete compatibility questions:

- Which original-DLL callsites touch the network, loader, file, and patching
  APIs?
- Which GTA-SA state changes happen during load, connect, spawn, dialog,
  TextDraw, vehicle, object, and shutdown scenarios?
- Which runtime calls are unique to `samp.dll` and which are process-global
  loader/Wine noise?
- Does the rebuilt DLL produce the same high-level sequence under the same
  scenario?

The probe is not a gameplay feature, not a public-server tool, and not a
replacement for protocol bounds checks inside the rebuilt client. Keep it for
local original-vs-rebuild comparisons.

## Build

On a host with MinGW:

```bash
tools/asi_probe/build_win32.sh
```

Through the reverse-engineering toolbox:

```bash
toolboxes/reverse-engineering/run.sh \
  bash -lc 'cd /path/to/libsamp && tools/asi_probe/build_win32.sh'
```

Debug build through the `devbuild` toolbox, with DWARF debug sections and a linker map:

```bash
toolbox run -c devbuild bash -lc 'cd /path/to/libsamp && \
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

Recommended scenario names:

```text
load_init
connect_handshake
spawn
chat_dialog_textdraw
vehicle_state
remote_player_state
object_state
disconnect
```

For a golden trace, keep the run short and deterministic:

1. clear the previous `samp_probe.log`,
2. start the original DLL or rebuilt DLL with the same server and nickname,
3. perform exactly one scenario,
4. stop the client cleanly,
5. normalize pointers, module bases, timestamps, and random values before
   comparing runs.

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
SAMP_PROBE_GTA_ASSET_HOOKS=1
SAMP_PROBE_OBJECT_INFO=1
SAMP_PROBE_TEXTDRAW_HOOKS=1
SAMP_PROBE_TEXTDRAW_VERBOSE=1
SAMP_PROBE_TEXTDRAW_RENDER=1
```

The asset trace can also be toggled through files next to the ASI:

```text
samp_probe_asset_paths.flag
samp_probe_file_hooks.flag
samp_probe_samp_code_hooks.flag
samp_probe_gta_asset_hooks.flag
samp_probe_object_info.flag
samp_probe_textdraw_hooks.flag
samp_probe_textdraw_verbose.flag
samp_probe_textdraw_render.flag
```

Use `samp_probe_asset_paths.flag` for normal original-DLL golden traces. It logs interesting SA-MP asset opens, size queries, seeks, and closes. `samp_probe_file_hooks.flag` additionally hooks `ReadFile`; keep that for short, targeted runs only because original 0.3.7 performs large overlapped reads against the SAMP archives.

Use `samp_probe_gta_asset_hooks.flag` only for short custom-object traces. It
patches GTA-SA engine asset paths observed through gta-reversed symbols:
`CStreaming::AddImageToList`, `CStreaming::LoadCdDirectory`,
`CModelInfo::AddAtomicModel`, `CModelInfo::AddTimeModel`,
`CModelInfo::AddClumpModel`, `CColStore::AddColSlot`, and
`CColStore::LoadCol`. It also traces `CPhysical::Add` at GTA-SA `0x00544A30`
for short object-flood runs; these lines include the entity model id, model
info pointer, RW object pointer, and current model store counts. Log lines
include `caller_samp_rva` when the caller is inside `samp.dll`, so a run with
the original 0.3.7 DLL can be used as `PROBE_TRACE` evidence for the original
custom-object loading path.

Add `samp_probe_object_info.flag` for focused custom-object runs. This emits
`gta_object_info` snapshots for tracked SA-MP object IDs and for custom model
IDs `18631..19999` after `AddModel`, `LoadCdDirectory`, `CColStore::LoadCol`,
and first `CPhysical::Add` observation. Each snapshot includes the GTA
`CModelInfo` pointer, selected inferred fields, and bounded raw hex dumps of
the model-info and collision-model memory. Treat those field names as
`GTA_REVERSED_REF`/`TODO_VERIFY`; the raw bytes are the durable `PROBE_TRACE`
evidence.

Use `samp_probe_textdraw_hooks.flag` for focused TextDraw runs. It hooks the
known GTA `CFont` calls and logs font style, alignment, box, color, and printed
strings with SA-MP caller RVAs where available. Add
`samp_probe_textdraw_verbose.flag` only when the short run needs dense CFont
state transitions.

Add `samp_probe_textdraw_render.flag` for Font 4 sprite and Font 5 model-preview
runs. This enables the CFont hooks and then traces SA-MP's D3DX/D3D render path
by hooking `D3DXCreateSprite`, selected D3DX texture creation imports,
`ID3DXSprite::Begin/Draw/End`, and selected `IDirect3DDevice9` vtable entries
such as `SetTexture`, `SetRenderState`, `SetFVF`, and `DrawPrimitive*`. The
render probe tracks texture sources and marks short render windows after
Font 4/5 CFont hints, so lines with the prefix `textdraw_render:` are the main
diff target. Treat these as `PROBE_TRACE`/`TODO_VERIFY` evidence and keep runs
short because this flag patches COM vtables.

## Example Log Output

The exact format can evolve, but stable lines should remain module-attributed
and include RVAs where possible.

PE/module identity:

```text
[probe] loaded module=samp.dll base=0x10000000 size=0x003a0000 path=C:\Games\GTA San Andreas\samp.dll
[probe] pe sha256=<normalized-sha256> image_base=0x10000000 entry_rva=0x000cbc90
[probe] section name=.text rva=0x00001000 size=0x001d7000 characteristics=0x60000020
```

IAT and Winsock attribution:

```text
[probe] iat module=samp.dll dll=WSOCK32.dll import=recvfrom slot_rva=0x002f1234 target=0x7bc12345
[api] recvfrom caller=samp.dll+0x00053b19 socket=924 len=146 flags=0 ret=42 data=84e1...
[api] sendto caller=samp.dll+0x00053a57 socket=924 len=22 flags=0 ret=22 data=53414d50...
```

State snapshots:

```text
[state] phase=preconnect menu=0 hud=0 paused=0 camera_mode=2 player_ptr=0x00000000
[state] phase=spawn menu=0 hud=1 paused=0 local_player=0x1a2b3c00 ped=0x1a300000 vehicle=0x00000000
```

Asset/file tracing:

```text
[asset] CreateFileA caller=samp.dll+0x0004a210 path=C:\Games\GTA San Andreas\SAMP\samp.img access=0x80000000
[asset] GetFileSize caller=samp.dll+0x0004a2c1 handle=0x00000124 size=10485760
```

GTA asset tracing:

```text
[probe] gta_code_hook: installed name=gta.CStreaming.LoadCdDirectory addr=0x005b6170 requested=0x005b6170 target=0x005b6170 trampoline=0x12340000 len=6
[probe] gta_asset: AddImageToList count=1 caller=0x1004a210 caller_samp_rva=0x0004a210 path='SAMP\samp.img' not_player_img=1 result=5 evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY
[probe] gta_asset: LoadCdDirectory.begin count=1 caller=0x1004a2c0 caller_samp_rva=0x0004a2c0 path='SAMP\samp.img' image_id=5 atomic=0 time=0 clump=0 evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY
[probe] gta_asset: AddAtomicModel count=1 caller=0x005b6abc caller_samp_rva=0x00000000 model=19300 store_before=0 store_after=1 result=0x12345678 model_info_ptr=0x12345678 evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY
[probe] gta_object_info: phase=AddAtomicModel caller=0x1004a2c0 caller_samp_rva=0x0004a2c0 model=19316 model_info=0x12345678 vtable=0x0086abcd key=0x12345678 txd_index=42 draw_distance=299.000000 col_model=0x23456789 raw=... col_raw=... evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY
[probe] gta_asset: CPhysical.Add.begin count=42 caller=0x0054abcd caller_samp_rva=0x00000000 entity=0x12345678 readable=1 vtable=0x0086abcd rw_object=0x23456789 model=1383 status=0x04 sector_link=0x00000000 model_info_ptr=0x00ab1874 atomic=15417 time=160 clump=71 evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY
```

Unknown or stale hook candidates must be logged as evidence gaps, not treated as
facts:

```text
[probe] samp-code-hooks disabled: stale candidate rva=0x0004ffc0 evidence=PROBE_TRACE run=golden_037_20260605_pre_sampimg
[probe] TODO_VERIFY unknown state transition: phase=post_load expected=preconnect
```

## First Questions This Should Answer

1. Does the original `samp.dll` call Winsock through its own `WSOCK32.dll` IAT as expected?
2. If not, which `WSOCK32.dll` export is called through a dynamic/unpacked function pointer?
3. Which exact callsite RVA triggers `socket`, `connect`, `sendto`, `recvfrom`, or `select`?
4. Which exact imported calls happen before the first `Connecting to ...` UI banner?
5. Which GTA-SA code/data addresses change when SA-MP installs runtime hooks?
6. Does our rebuilt DLL produce the same module-attributed call sequence under the same scenario?
7. Are current Wine trace PASS rows real `samp.dll` calls or process-global side effects?
8. At the exact point where GTA leaves the loading screen, do `ENTRY`, menu flags, HUD, D3D pointers, and the graphics/game-loop hook targets match the original client?
