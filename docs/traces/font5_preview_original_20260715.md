# Original 0.3.7-R5 Font 5 Preview Trace (2026-07-15)

## Capture identity

- Prefix: `/home/chairman/Games/san-andreas-multiplayer-legacy-legacy`
- Scenario: UFW pre-spawn/class-selection menu, held until all six preview
  cards had completed preparation and repeated draw passes
- Original DLL SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Probe log: GTA root `samp_probe.log`
- Hook guard: `pair_preflight_ok hooks=2`, `installed=2 requested=2`
- Hook RVAs: prepare `samp.dll+0x000b34a0`, draw dispatch
  `samp.dll+0x000b3480`

## Observed sequence

`OBSERVED_037` + `PROBE_TRACE`:

- 73 complete `prepare.begin`/`prepare.end` pairs and 73 `draw.begin`
  calls were captured.
- Seven first draws ended on the normal-font branch while the preview slot was
  still `-1`; later calls ended as 66 `draw.preview.end` events after the
  cached previews existed.
- The six UFW cards used models `18778`, `415`, `11737`, `1584`, `10281`, and
  `429`, with the transmitted rotations, zoom values, vehicle colours, and
  background `0xaa464646` visible at the dispatch boundary.
- Preview preparation switched to a 256x256 A8R8G8B8 camera-texture surface,
  installed a depth-stencil surface and 256x256 viewport, cleared target plus
  Z, rendered the model, and later bound the resulting D3D texture for the
  screen draw.

## Static correlation

`STATIC_037` + `GTA_REVERSED_REF`:

- `samp.dll+0x000b34a0` dispatches model-info vtable `0x0085bdc0`
  (`CPedModelInfo` in GTA SA 1.0 US) to `samp.dll+0x0006c140`, vehicle models `400..611` to
  `samp.dll+0x0006c3c0`, and remaining model-info instances to
  `samp.dll+0x0006c9b0`.
- The renderer creates a 256x256 raster with flags `0x505`, wraps it in an
  `RwTexture`, uses a shared 256x256 Z raster, and renders a model-info instance
  through a dedicated RenderWare camera.
- Camera construction uses far clip `300.0`, near clip `0.01`, view window
  `(0.5, 0.5)`, an ambient white light, and a frame translated to Z `50.0`
  then rotated 90 degrees about X.
- The special `0x0085bdc0` path uses `{0, -2.25 * zoom, 50.05}`.
- The normal-vehicle path uses collision radius but not centre:
  `{0, (-1 - 2 * radius) * zoom, 50}`. It applies both transmitted preview
  colours through the vehicle wrapper when neither is `-1`.
- The generic path uses collision-sphere center/radius and the observed
  translation `{-center.x, (-0.1 - 2.25 * radius) * zoom, 50 - center.z}`.
  It does not call `RwFrameSetIdentity` before translation. Transmitted X/Y/Z
  rotations are pre-concatenated in all paths.
- Atomic instances call their RenderWare render callback at `atomic+0x48`;
  clump instances use GTA `RpClumpRender`.

## Replacement boundary

The replacement implementation follows the verified special/vehicle/generic
placement policies on guarded atomic/clump model-info instances and caches its
RenderWare texture per active TextDraw. Vehicle instances bracket rendering
with GTA's editable-material colour substitution. It snapshots and restores
D3D render target, depth-stencil, and viewport around preview preparation. A
preview remains on the existing shaped proxy when the model is not safely
registered/resident.

`TODO_VERIFY`: complete original CObject/CVehicle/ped wrapper construction, ped
animation, wrapper-only lighting/component state, and unload ownership after
cache creation are not yet reproduced.
