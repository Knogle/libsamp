# Textdraw Render Stack

## Scope

This note captures the textdraw path currently used by the replacement client.
Evidence tags: `STATIC_037`, `PROBE_TRACE`, `GTA_REVERSED_REF`, `OPENMP_REF`,
`INFERRED`, `TODO_VERIFY`.

## Network Ingress

`PROBE_TRACE` + `OPENMP_REF`: open.mp/0.3.7 textdraw RPCs arrive as a fixed
core transmit block plus text payload. The decoder keeps the recovered core
fields and stores the additional selectable/preview-model data in
`samp_raknet_textdraw_transmit`.

- Show: `WORD id`, transmit data, then bounded text.
- Hide: `WORD id`.
- Edit: `WORD id`, then replacement text.

## Pool Lifecycle

`STATIC_037` + `PROBE_TRACE`: the original-DLL behavior is slot based. Show
activates a textdraw slot, hide clears it, edit replaces the text without
recreating unrelated slot state. Draw iterates active slots from the render path
and suppresses normal textdraw drawing while TAB/scoreboard is active.

Current replacement equivalent:

- decode in `reimpl/src/net/raknet_client_adapter.cpp`
- queue ring event in `samp_raknet_textdraw_event`
- apply to `g_runtime.textdraw_slots`
- render from the D3D overlay hook

## Render Order

`OBSERVED_037` + `PROBE_TRACE` + `GTA_REVERSED_REF`: the original 0.3.7 DLL
routes normal textdraw text and boxes through GTA `CFont`.

Trace: `analysis/generated/textdraw_probe_037_verbose_20260706`, original
`samp.dll` SHA256=`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`,
GTA SA exe SHA256=`a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`.

The replacement keeps a D3DX fallback for preview-model placeholders and
unverified edge cases.

### Early class-selection phase

`OBSERVED_037` + `PROBE_TRACE`: `ENABLE_HUD` is not an original-client
readiness signal for normal TextDraws. In
`analysis/generated/textdraw_probe_037_verbose_20260706/samp_probe_verbose_slice.log`,
the original R5 client is at `entry=9`, `game_started=0`, `hud=0` at line
57455 and starts the complete normal `CFont` sequence at line 57512, nine
milliseconds later.

The replacement therefore permits the register-preserving GameProcess path in
that exact early state. It does not relax the EndScene or graphics-callback
paths, because their earlier `ip=0` failures remain `TODO_VERIFY`. When the
GameProcess hook is unavailable or has not actually drawn during the recent
frame window, normal TextDraws fall back to D3DX instead of disappearing merely
because GameProcess mode is configured.

`OBSERVED_037` + `PROBE_TRACE`: the original client composites a cached Font 5
quad and then invokes `CFont::PrintString` for the following normal slot in the
same ordered TextDraw pass. The replacement now keeps expensive
RenderWare-to-texture preparation in EndScene, but submits an already cached
Font 5 quad from the register-preserving `CHud::DrawScriptText` epilogue at
`0x58C246` while iterating the same slot order as normal GTA `CFont` TextDraws.
EndScene suppresses the later duplicate composite in this mode.

The 2026-07-16 13:11 replacement run tested deferring overlapping normal slots
to the EndScene D3DX fallback. That is not compatible: centred labels collapsed
into `GZombieNo caigas`, the counter moved to the viewport edge, and the run
ended without `process_detach`. Normal slots therefore remain exclusively in
the GTA `CFont` path. Font 5 cache preparation retains the transmitted clear
colour and alpha; the former transparent-alpha workaround is no longer needed
because the cached quad is submitted before later foreground slots.

`TODO_VERIFY`: the next replacement run must confirm a
`textdraw_gta_font: prespawn_ready` marker and a successful
`textdraw_gta_font_gameprocess` marker before `spawn_finalize_begin`, followed
by `textdraw_preview: draw ... phase=game_process` and
`textdraw_layer: slot_order`, with no later EndScene duplicate,
`draw_state_restore_failed`, or `exception_filter`.

Important GTA addresses:

- `0x719380`: `CFont::SetScale`
- `0x719430`: `CFont::SetColor`
- `0x719490`: `CFont::SetFontStyle`
- `0x7194D0`: `CFont::SetWrapx`
- `0x7194E0`: `CFont::SetCentreSize`
- `0x719510`: `CFont::SetDropColor`
- `0x719570`: `CFont::SetDropShadowPosition`
- `0x719590`: `CFont::SetEdge`
- `0x7195B0`: `CFont::SetProportional`
- `0x7195C0`: `CFont::SetBackground`
- `0x7195E0`: `CFont::SetBackgroundColor`
- `0x719600`: unknown font state setter in this branch
- `0x719610`: justification/orientation state
- `0x71A700`: `CFont::PrintString`

Observed normal textdraw call sequence:

1. set scale from script-space letter size and HUD scale
2. set letter color
3. set unknown font state to `0`
4. set justification/orientation from left/center/right flags
5. set wrap/centre size from line width/height
6. set background box and box color
7. set proportional flag
8. set drop color
9. set outline when `outline != 0`, otherwise set shadow
10. set font style
11. print at script-space converted to screen-space
12. reset outline to `0`

`OBSERVED_037`: the traced stack does not issue a matching `SetShadow(0)` after
`PrintString`.

## String Conversion Risk

`GTA_REVERSED_REF`: `0x69E160` is
`CMessages::InsertPlayerControlKeysInString`, not a generic underscore
converter. `STATIC_037` + `TODO_VERIFY`: the original-DLL text conversion thunk
near `0x69DE90` still needs a focused call-convention trace.

`OBSERVED_037` + `PROBE_TRACE`: original `PrintString` input keeps SAMP
color/newline tokens such as `~r~`, `~g~`, and `~n~`, while underscores are
visible as spaces and `~k~~VEHICLE_ENTER_EXIT~` becomes the key name.

## Implementation Plan

Stable default path:

- keep D3DX as the default replacement draw path for now
- keep GTA `CFont` as an opt-in experiment for normal textdraw text and
  background boxes until it is moved out of the EndScene overlay
- preserve SAMP color/newline tags on the GTA `CFont` path and normalize `_` to
  spaces before `PrintString`
- render font `5` preview-model textdraws through a cached GTA/RenderWare
  camera texture, retaining the visible D3D proxy only while the requested
  model is not safely resident or preview resource creation fails
- keep generic font `4` sprite/TXD textdraws as a placeholder path, with only
  common solid `LD_SPAC:white`/`LD_SPAC:black` sprites special-cased

Model-preview note:

`OPENMP_REF`: open.mp documents font `5`
textdraws as model previews configured by `TextDrawSetPreviewModel`,
`TextDrawSetPreviewRot`, and `TextDrawSetPreviewVehCol`. The replacement
decodes these fields from the open.mp textdraw transmit block and now also
extracts the classic 0.3.7 preview tail when a compact font `5` payload carries
one.

`OBSERVED_037` + `PROBE_TRACE` + `STATIC_037` + `GTA_REVERSED_REF`: the
2026-07-15 original-R5 run established the preview dispatcher at
`samp.dll+0x000b34a0`. Model-info vtable `0x0085bdc0` (`CPedModelInfo` in GTA
SA 1.0 US) routes to `samp.dll+0x0006c140`, vehicles `400..611` to
`samp.dll+0x0006c3c0`, and remaining model-info instances to
`samp.dll+0x0006c9b0`. All three create 256x256
A8R8G8B8 camera-texture rasters, use one shared 256x256 Z raster and ambient
light, clear with the transmitted background colour, render a model instance
using the transmitted rotation/zoom, then draws the cached D3D texture as a
screen quad. The replacement now follows that shape using GTA SA 1.0 US
RenderWare entry points. It snapshots and restores the active D3D render
target, depth-stencil surface, and viewport around each cache fill so the
EndScene fallback cannot leave the backbuffer replaced by the preview target.

Model streaming is deliberately guarded. A preview is created only when its
`CModelInfo`, streaming row, resident RenderWare object, instance factory, and
render callback are readable. A missing vanilla model is requested once per
retry interval; a custom SA-MP model must first pass the existing custom-model
registration and archive-mapping checks. Until then, the old shaped D3D proxy
remains visible. This avoids reproducing the known replacement crash from
blindly registering models beyond the vanilla atomic store.

`STATIC_037` + `GTA_REVERSED_REF`: the replacement preserves those three
placement policies. The special `0x0085bdc0` path uses
`{0, -2.25 * zoom, 50.05}`;
normal vehicles use `{0, (-1 - 2 * collisionRadius) * zoom, 50}`; the generic
path retains collision-centre placement. It also leaves the created root frame
intact, matching the absence of `RwFrameSetIdentity` in the original generic
renderer. Vehicle previews choose GTA's four default colours, override the
first two when both transmitted colours are present, and bracket rendering with
`CVehicleModelInfo::SetEditableMaterials`/`ResetEditableMaterials`.

`TODO_VERIFY`: the replacement still uses guarded model-info instances rather
than constructing SA-MP's complete CObject/CVehicle/ped wrappers. Ped preview
animation and wrapper-only lighting/component state remain parity gaps, not
decoder gaps.

Viewport note:

`OBSERVED_037`: the 2560x1440 original/replacement image comparison shows that
the original chat stays anchored to the physical left edge and that TextDraw X
coordinates span the complete client width (`screenWidth / 640`). The former
centred 1920x1440 4:3 sub-viewport compressed the six Font-5 cards horizontally
around the screen centre by 25 percent. The replacement now uses the full
client rectangle for TextDraw geometry and hit-testing while retaining the
independent `screenHeight / 448` Y scale. Its class-selection fallback mirrors
the observed compact bottom-centre `<<`, `>>`, `Spawn` control group. Chat and
scoreboard panels intentionally reuse that fallback's translucent black,
single-pixel white-border visual style (`INFERRED`, user-requested replacement
UI rather than an original-R5 parity claim).

A dedicated original-R5 Font 5 probe is available behind
`SAMP_PROBE_FONT5_HOOKS=1` / `samp_probe_font5_hooks.flag`. It traces the
validated SA-MP preview dispatch boundary together with render-target,
depth-stencil, viewport, clear, texture-surface, and draw activity. This probe
is opt-in and guarded by the R5 PE identity proxy plus exact prologues; its
output is evidence gathering, not a replacement renderer. The supported DLL
hash is documented for the run, but is not recomputed by the runtime guard.

Sprite note:

`OPENMP_REF` + `TODO_VERIFY`: font `4` textdraw sprites use `library:texture`
names resolved from `SA Dir/models/txd/` and `SA Dir/SAMP/`. Full TXD sprite
loading remains open.

Crash note:

`PROBE_TRACE` + `TODO_VERIFY`: the 2026-07-06 libsamp replacement EndScene
CFont run crashed immediately after the first successful
`textdraw_gta_font: drawing enabled` line with
`exception_filter: code=0xc0000005 ip=0x00000000`. The later GameProcess CFont
run also reached the first TD00 draw log and then faulted at `ip=0`, which
points more strongly at stack cleanup/calling-convention drift than malformed
textdraw state. An `esp`-neutral wrapper alone did not change that fingerprint,
and the stack/register-neutral 2026-07-06 22:34 run still faulted at the same
point. The replacement now also passes a persistent slot buffer to `PrintString`
and keeps the GTA control-key converter opt-in while its 0.3.7 contract is
`TODO_VERIFY`. The path stays opt-in until its ABI and render phase are
verified against 0.3.7.

`OBSERVED_037` + `PROBE_TRACE`: with the GTA converter disabled, the screenshot
comparison `sa-mp-000.png` vs. `sa-mp-020.png` still requires local expansion
of `~k~~VEHICLE_ENTER_EXIT~` to `RETURN` before `CFont::PrintString`; otherwise
TD08 renders a placeholder glyph instead of the key name.

D3DX fallback note:

`PROBE_TRACE` + `INFERRED`: the 2026-07-06 screenshot comparison
`sa-mp-000.png` original vs. `sa-mp-016.png` replacement confirms the fallback
is not visually 1:1: glyphs are too thin/small, outline/shadow are too weak,
and GTA control-key tokens need local expansion. Runtime fallback adjustments
may improve comparison screenshots, but they are not proof of original-DLL
behavior and must not replace the GTA `CFont` render path.

Experimental replacement CFont path:

`STATIC_037` + `PROBE_TRACE` + `TODO_VERIFY`: prefer
`SAMP_TEXTDRAW_GTA_FONT_GAMEPROCESS=1`. The replacement draws from the
GameProcess hook phase with a register-preserving generated stub. The GTA
`CFont` calls in this mode are stack- and register-neutral on i386 so a
callee/caller cleanup mismatch or unexpected caller-register clobber cannot
drift the C callback while the original ABI is still `TODO_VERIFY`.

Preferred local configuration is `sampdll.ini` beside `samp.dll`:

```ini
[sampdll]
SAMPDLL_GRAPHICS_HOOK_POST_CALLBACK=0
SAMP_TEXTDRAW_GTA_FONT_GAMEPROCESS=1
SAMP_TEXTDRAW_GTA_FONT_CONVERT=0
SAMP_TEXTDRAW_GTA_FONT_GRAPHICS=0
SAMP_TEXTDRAW_GTA_FONT=0
```

Avoid using `SAMPDLL_GRAPHICS_HOOK_POST_CALLBACK=1` with
`SAMP_TEXTDRAW_GTA_FONT_GRAPHICS=1` for normal testing. That route calls CFont
from the graphics callback after the original target returns and crashed in the
2026-07-06 libsamp run at `ip=0`.

Stable fallback configuration while the CFont render phase is still
`TODO_VERIFY`:

```ini
[sampdll]
SAMP_TEXTDRAW_GTA_FONT_GAMEPROCESS=0
SAMP_TEXTDRAW_GTA_FONT_CONVERT=0
SAMPDLL_GRAPHICS_HOOK_POST_CALLBACK=0
SAMP_TEXTDRAW_GTA_FONT_GRAPHICS=0
SAMP_TEXTDRAW_GTA_FONT=0
```

Flag lookup order is `sampdll.ini`, process environment, Wine prefix registry,
then the compiled default. This makes per-prefix test configuration auditable
even when Wine turns registry environment values into process variables.

Open verification tasks:

- capture a focused original-DLL trace for textdraw background color alpha
- verify the GameProcess-hook CFont path against the 0.3.7 original screenshots
- verify `CFont` setter order for centered/right-aligned textdraws
- validate preview-model textdraw object/material ownership and replace the D3D
  proxy with the verified GTA/RenderWare render path
- implement full font `4` TXD sprite loading from GTA/SA-MP texture libraries
- verify click/select hit-test bounds against original 0.3.7 behavior
