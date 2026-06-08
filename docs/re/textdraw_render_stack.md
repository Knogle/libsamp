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

`STATIC_037` + `GTA_REVERSED_REF` + `TODO_VERIFY`: original-DLL RE and
gta-reversed both point to GTA `CFont` as the correct text path for normal
textdraw text and background boxes. The replacement keeps a D3DX fallback for
preview-model placeholders and unverified edge cases.

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

Current call sequence:

1. set scale from script-space letter size and HUD scale
2. set letter color
3. set justification/orientation from left/center/right flags
4. set wrap/centre size from line width/height
5. set background box and box color
6. set proportional flag
7. set drop color, outline or shadow
8. set font style
9. print at script-space converted to screen-space
10. reset outline/shadow

## String Conversion Risk

`GTA_REVERSED_REF`: `0x69E160` is
`CMessages::InsertPlayerControlKeysInString`, not a generic underscore
converter. `STATIC_037` + `TODO_VERIFY`: the original-DLL text conversion thunk
near `0x69DE90` still needs a focused call-convention trace.

`INFERRED`: directly calling either helper from our overlay path can corrupt the
stack or jump through an invalid output buffer. Until `OBSERVED_037` proves the
exact call convention and buffer contract, the replacement normalizes underscores
locally and leaves GTA conversion helpers disabled.

## Implementation Plan

Stable default path:

- use GTA `CFont` for normal textdraw text and background boxes when enabled
- keep D3DX fallback for preview-model placeholders and hit-tests
- use local SAMP tag stripping and `_` to space conversion
- treat preview-model textdraws as placeholders until their render path is
  implemented separately

Open verification tasks:

- capture a focused original-DLL trace for textdraw background color alpha
- verify `CFont` setter order for centered/right-aligned textdraws
- validate preview-model textdraw object/material ownership
- verify click/select hit-test bounds against original 0.3.7 behavior
