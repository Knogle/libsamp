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
- treat preview-model textdraws as placeholders until their render path is
  implemented separately

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
- validate preview-model textdraw object/material ownership
- verify click/select hit-test bounds against original 0.3.7 behavior
