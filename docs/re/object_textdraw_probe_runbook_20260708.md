# Object And TextDraw Probe Runbook - 2026-07-08

Goal: produce comparable `PROBE_TRACE` evidence for original SA-MP 0.3.7 and
the rebuilt DLL around custom object loading, object movement, sprites, and
TextDraw model previews.

## Evidence Scope

- `OBSERVED_037`: only after running this probe with the original 0.3.7 DLL.
- `PROBE_TRACE`: `samp_probe.log` lines emitted by the ASI in that run.
- `OPENMP_REF`: TextDraw font 4 sprite syntax and font 5 model preview
  semantics from open.mp docs:
  - https://open.mp/docs/scripting/resources/textdrawsprites
  - https://open.mp/docs/scripting/functions/TextDrawSetPreviewModel
- `GTA_REVERSED_REF`: selected `CModelInfo`/`CPhysical` field names used only
  for labels; the raw hex dumps are the authoritative trace material.
- `TODO_VERIFY`: exact `CModelInfo` field offsets still need static 0.3.7/GTA-SA
  corroboration before being treated as layout facts.

## ASI Flags

Place these files next to `gta_sa.exe` and `samp_probe.asi`:

```text
samp_probe_asset_paths.flag
samp_probe_gta_asset_hooks.flag
samp_probe_object_info.flag
samp_probe_textdraw_hooks.flag
samp_probe_textdraw_verbose.flag
samp_probe_textdraw_render.flag
```

Do not enable `samp_probe_file_hooks.flag` for normal runs; it is intentionally
noisy. Do not enable `samp_probe_samp_code_hooks.flag` unless the SA-MP internal
RVA candidates have been revalidated.

## Object Probe

Gamemode: `omp-server-bare/gamemodes/object_probe.amx`

Primary commands:

```text
/oprobe
/opmove
/opstop
/opreset
/opmat
/opplayer
/opdestroy
```

Expected server-side phases:

- global `CreateObject` for tracked SA-MP models including `19316`, `19894`,
  `18997`, `19425`, `18842`, `18818`, `19909`, `19907`, `19905`, and gates
  `985`/`986`;
- player-specific `CreatePlayerObject` for a smaller custom-object set;
- `SetObjectMaterialText`, `SetObjectMaterial`, and `SetObjectNoCameraCol`;
- deterministic `MoveObject` on the gate pair via `/opmove`, with `/opstop`
  and `/opreset` to isolate movement semantics.

Expected ASI lines:

```text
gta_asset: AddImageToList ...
gta_asset: LoadCdDirectory.begin ...
gta_asset: LoadCdDirectory.end ...
gta_asset: AddAtomicModel ...
gta_object_info: phase=AddAtomicModel ...
gta_object_info: phase=LoadCdDirectory.end.scan ...
gta_asset: CPhysical.Add.begin ...
gta_object_info: phase=CPhysical.Add.begin ...
```

The key diff targets are `model_info`, `txd_index`, `draw_distance`, `col_model`,
`raw`, and `col_raw` for the same model IDs across original and rebuild runs.

## TextDraw Probe

Gamemode: `omp-server-bare/gamemodes/textdraw_probe.amx`

Primary commands:

```text
/tdshow
/tdstrings
/tdpreview
/tdselect
/tdcancel
/tdrebuild
/tdhide
```

The probe now exercises:

- basic text fonts 0..3;
- Font 4 sprites: `LD_SPAC:white`, `LD_SPAC:black`, and `LD_BEAT:chit`;
- Font 5 model previews: skins, vehicles, vanilla objects, and SA-MP custom
  objects `19316` and `19894`;
- global and player TextDraw preview rotation/color updates via `/tdpreview`;
- selectable global/player TextDraw click/cancel flows.

Expected ASI lines:

```text
textdraw: CFont.SetFontStyle ... style=4
textdraw: CFont.PrintString ... text='LD_SPAC:white'
textdraw: CFont.SetFontStyle ... style=5
textdraw: CFont.PrintString ...
textdraw_render: D3DXCreateSprite ...
textdraw_render: D3DXCreateTextureFromFile...
textdraw_render: ID3DXSprite.Draw ...
textdraw_render: IDirect3DDevice9.SetTexture ...
textdraw_render: IDirect3DDevice9.DrawPrimitive...
```

The generic ASI coverage observes the GTA font path plus selected D3DX/D3D
render calls. Use the focused dispatch probe below when those calls alone do
not establish the Font 5 preview-object path.

## Focused Font 5 R5 Dispatch Probe

The focused dispatch hook is now available for the exact local original R5 DLL:

```text
samp_probe_font5_hooks.flag
```

Do not combine it with `samp_probe_samp_code_hooks.flag`; the latter still
contains unrelated stale network candidates. The Font 5 flag independently
enables the required CFont/D3D trace and installs only the identity- and
prologue-validated hooks at `samp.dll+0x000b34a0` (preview preparation) and
`samp.dll+0x000b3480` (draw dispatch). Supported original hash:
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
The install guards require these bytes before patching:

```text
samp.dll+0x000b34a0: 56 8b f1 83 be 87 09 00 00 05
samp.dll+0x000b3480: 83 b9 a3 09 00 00 ff
```

For a short UFW pre-spawn capture:

1. Start with a fresh `samp_probe.log` and only the focused Font 5 flag plus
   any passive state/asset flags needed by the scenario.
2. Join the same server and wait at class selection until the six mode-preview
   cards are visible for two or three frames.
3. End the `gta_sa.exe` process after the short capture. Do not dynamically
   unload the ASI: the focused code and COM-vtable hooks are process-bound and
   intentionally have no hot-unload restore path.
4. Keep these lines together by `font5_seq`:

```text
font5_code_hook: pair_preflight_ok hooks=2 ...
font5_code_hook: summary installed=2 ...
font5_dispatch: phase=prepare.begin ...
font5_dispatch: phase=prepare.end ...
font5_dispatch: phase=draw.begin ...
textdraw_rtt: IDirect3DDevice9.SetRenderTarget.begin ... font5_seq=...
textdraw_rtt: IDirect3DDevice9.SetDepthStencilSurface.begin ... font5_seq=...
textdraw_rtt: IDirect3DDevice9.SetViewport.begin ... font5_seq=...
textdraw_rtt: IDirect3DDevice9.Clear.begin ... font5_seq=...
textdraw_render: IDirect3DDevice9.Draw... ...
font5_dispatch: phase=draw.preview.end ...
```

Discard the run if it contains `pair_preflight_failed` or
`incomplete_install`, or if the summary does not report `installed=2`.

5. Extract the focused records without mixing in the older log blocks:

```bash
tools/analyze_textdraw_trace.sh \
  -o analysis/generated/font5_original \
  "/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas/samp_probe.log"
```

Missing `SetRenderTarget` records during a valid `draw.preview` span are also
useful evidence: they would indicate that the cached preview object is rendered
through an already-bound RenderWare target or a different path, and the next
hook should move below the statically observed preview renderer call rather
than assuming a D3D9 RTT lifecycle.
