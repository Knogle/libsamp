# Textdraw Legacy Stack

## Scope

This note captures the textdraw path used as implementation reference for the
replacement client. Evidence tags: `OLD_02X_REF`, `GTA_REVERSED_REF`,
`OPENMP_REF`, `INFERRED`, `TODO_VERIFY`.

## Network Ingress

`OLD_02X_REF`: `scriptrpc.cpp` receives textdraw RPCs and updates
`CTextDrawPool`.

- Show: `WORD id`, packed 40-byte `TEXT_DRAW_TRANSMIT`, then up to 256 bytes of
  text.
- Hide: `WORD id`.
- Edit: `WORD id`, then replacement text.

`OPENMP_REF`: open.mp/0.3.7 payloads extend the legacy fields with selectable
state and preview-model metadata. Our current decoder keeps the legacy 40-byte
core and stores the additional fields in `samp_raknet_textdraw_transmit`.

## Pool Lifecycle

`OLD_02X_REF`: `CTextDrawPool` owns fixed slots. Show constructs a
`CTextDraw`, hide deletes it, edit replaces only the text. Draw iterates active
slots every frame and skips the pool while TAB is held.

Current replacement equivalent:

- decode in `reimpl/src/net/raknet_client_adapter.cpp`
- queue ring event in `samp_raknet_textdraw_event`
- apply to `g_runtime.textdraw_slots`
- render from the D3D overlay hook

## Render Order

`OLD_02X_REF`: `GameProcessHook` calls `CTextDrawPool::Draw()` when the game is
running and the GTA menu is not active. Each `CTextDraw::Draw()` configures GTA
`CFont`, queues `CFont::PrintString`, then resets outline/shadow.

`GTA_REVERSED_REF`: the important GTA addresses are:

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
- `0x719600`: `CFont::SetJustify`
- `0x719610`: `CFont::SetOrientation`
- `0x71A700`: `CFont::PrintString`

The legacy call sequence is:

1. set scale from script-space letter size and HUD scale
2. set letter color
3. disable justify mode
4. set orientation from left/center/right flags
5. set wrap/centre size from line width/height
6. set background box and box color
7. set proportional flag
8. set drop color, outline or shadow
9. set font style
10. print at script-space converted to screen-space
11. reset outline/shadow

## String Conversion Risk

`GTA_REVERSED_REF`: `0x69E160` is
`CMessages::InsertPlayerControlKeysInString`, not a generic underscore
converter. `OLD_02X_REF`: the old `Font_UnkConv` wrapper does not call
`0x69DE90` like a normal function; it returns that address in `EAX`.

`INFERRED`: directly calling `0x69E160` from our overlay path can corrupt the
stack or jump through an invalid output buffer. Until `OBSERVED_037` proves the
exact 0.3.7 call convention and buffer contract, the replacement normalizes
underscores locally and leaves GTA conversion helpers disabled.

## Implementation Plan

Stable default path:

- keep current D3DX renderer for normal textdraw text, boxes and selection
  hit-tests
- use local SAMP tag stripping and `_` to space conversion
- keep preview-model textdraws as placeholders until their model-render path is
  implemented separately

Experimental legacy path:

- `SAMP_TEXTDRAW_GTA_FONT=1` enables the GTA CFont path
- it follows the legacy call order but does not call GTA message conversion
  helpers
- this path is `TODO_VERIFY` against original 0.3.7 traces before making it the
  default

