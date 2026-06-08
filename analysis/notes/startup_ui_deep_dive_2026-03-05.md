# Original `samp.dll` Startup/UI Deep Dive

Date: 2026-03-05

This note records the concrete reverse-analysis evidence used to build `analysis/metadata/STARTUP_UI_PATH_SPEC.md`.

## 1. Bootstrap Slice

### `0x100cbc90` entry

High-confidence behavior:

- DLL reason dispatch
- forwards into:
  - `0x100cbb0f`
  - `0x100c5270`

This matches a CRT wrapper plus SA:MP bootstrap handoff.

### `0x100cbb0f`

Observed process-attach work:

- `GetVersionExA`
- stores OS/version globals:
  - `data.1026ec5c`
  - `data.1026ec68`
  - `data.1026ec6c`
  - `data.1026ec60`
  - `data.1026ec64`
- `GetCommandLineA`
- multiple runtime-init helpers

Conclusion:

- runtime/attach scaffold
- not the connect UI initializer

### `0x100c5270 -> 0x100c50c0`

`0x100c5270` is just a jump into `0x100c50c0`.

Inside `0x100c50c0`, the first clear SA:MP bootstrap appears:

- save HMODULE to `data.1026eb38`
- `SetUnhandledExceptionFilter(data.10060970)`
- derive `samp.saa` from module path
- allocate/load archive object, store in `data.1026eb44`
- `AddFontResourceA("gtaweap3.ttf")`
- `AddFontResourceA("sampaux3.ttf")`
- call `0x10062ca0`
- call `0x10062970`
- allocate `0x142` bytes
- construct via `0x1009ff80`
- store resulting object in `data.1026ebac`

Working interpretation:

- `data.1026eb44` is archive/fs-facing
- `data.1026ebac` is a long-lived UI/scene/control manager

## 2. Support Functions Called Directly by `0x100c50c0`

### `0x10062ca0`

This resolves runtime API entrypoints by module+name strings and stores them in globals.

Resolved targets:

- `kernel32.dll!CreateFileA`
- `kernel32.dll!ReadFile`
- `kernel32.dll!GetFileSize`
- `kernel32.dll!SetFilePointer`
- `kernel32.dll!CloseHandle`
- `kernel32.dll!GetFileType`

Interpretation:

- filesystem shim/bootstrap support
- likely used by archive-backed loading

### `0x10062970`

This resolves:

- `user32.dll!ShowCursor`

Interpretation:

- startup prepares cursor control explicitly
- another hint that UI bring-up is part of early init

### `0x1009ff80`

Constructor-like routine used for the object stored in `data.1026ebac`.

Observed details:

- allocates a 5-byte block and an 8-byte block
- zeros multiple object fields
- zeros a large global region at `data.10150340`
- fills default flags and scalar state

Interpretation:

- clearly not just a tiny helper
- likely manager/state object reused by later UI or scene code

## 3. String Xrefs

### `Connecting to %s:%d...`

String:

- `0x100e599c`

Xref:

- `0x1000896d`

Function:

- `0x10008940`

What it does:

- throttles on `GetTickCount()` with a 3000 ms window
- reads host from `[esi+0x30]`
- reads port from `[esi+0x235]`
- emits formatted text through `0x100680f0`
- calls a vtable method on the current object
- stores:
  - `[esi+0x3d1] = tick`
  - `[esi+0x3cd] = 2`

Important extra detail:

- adjacent function `0x100089b0` references `data.1026ebac`
- this reinforces that connect-wait behavior already depends on the manager object created during bootstrap

### `Connected to {B9C9BF}%.64s`

String:

- `0x100e6060`

Xref:

- `0x10010a87`

Function:

- `0x10010a40`

What it does:

- `0x100a1560`
- conditional `0x100a71c0`
- reads `data.1026eb94`
- reads `data.1026ebac`
- calls `0x100a0b40`
- emits formatted text through `0x100680f0`
- sets `[edx+0x3cd] = 5`
- calls `0x10004080`
- calls `0x1001f8d0`

Interpretation:

- post-connect success transition
- not just logging, but visible state/UI promotion

## 4. First Hard UI Asset Loader

### `0x100990f0`

Primary anchors:

- string `UI\\DXUTShared.fx`
- string `UI\\arrow.x`

Observed sequence:

- `0x10097830` builds full paths
- `D3DXCreateEffectFromFileA("UI\\DXUTShared.fx")`
- `D3DXLoadMeshFromXA("UI\\arrow.x")`
- globals written near:
  - `0x10143c08`
  - `0x10143c0c`
  - `0x10143c10`
- mesh-related allocation via `0x100c627a`
- COM/vtable calls on the mesh/resource

Interpretation:

- concrete DXUT/D3DX resource bring-up
- strongest static proof that the original binary has a real in-game SA:MP UI asset path

Open point:

- rizin currently sees no direct code callers into `0x100990f0`
- likely explanation is indirect dispatch, callback registration, or incomplete function ownership

## 5. Render/UI Callgraph Slices

### `0x1007fcf0`

Shape:

- `2719` bytes
- `111` basic blocks
- `18` incoming references

Known incoming call sites:

- `0x100800e5`
- `0x100806cc`
- `0x1008083f`
- `0x1008096d`
- `0x10080ab6`
- `0x10080b57`
- `0x10080c23`
- `0x10080c6c`
- `0x10080e04`
- `0x10080e78`
- `0x100821be`
- `0x1008232f`
- `0x1008242e`

Representative behavior:

- clipboard handling
- accelerator translation
- message dispatch
- window sizing/visibility management
- nested helper calls and self-recursion

Conclusion:

- this is the main dialog/window/message dispatcher layer

### `0x10073550`

Incoming call site:

- `0x100618c4`

Representative behavior:

- `GetCursorPos`
- `ScreenToClient`
- `SetCapture`
- `ClipCursor`
- `ReleaseCapture`
- `SetCursorPos`

Conclusion:

- low-level cursor capture/warp controller beneath the UI dispatcher

### `0x10093ee0`

Incoming call site:

- `0x10094edc`

Representative behavior:

- `PtInRect`
- `GetCursorPos`
- `SetCapture`
- `ReleaseCapture`
- per-control state toggles at offsets `+0x94..+0x97`

Conclusion:

- widget-local interaction handler

### `0x10095d10`

Incoming call sites:

- `0x10097556`
- `0x100975a1`

Representative behavior:

- `D3DXMatrixInverse`
- `D3DXMatrixRotationQuaternion`
- `D3DXMatrixMultiply`
- `D3DXVec3Normalize`
- `D3DXVec3TransformNormal`

Conclusion:

- render math helper only
- relevant to a UI/render cluster, but not a startup root

## 6. Runtime Diff Notes

### Confirmed from runtime artifacts

- original olddll loads `d3dx9_25.dll` early
- original olddll loads `BASS.dll` early
- connect banner text enters the text-render path:
  - `MultiByteToWideChar`
  - `gdi32.ScriptItemize`

Meaning:

- the original connect-wait state is already attached to a live UI/text rendering pipeline

### Important false lead: `CreateWindowEx*`

The old runtime logs contain a very large `CreateWindowExA/W` burst, but those events start before the `LoadLibraryA("samp.dll")` event.

That means:

- those windows are likely from the launcher/browser process
- they should not be treated as proof for the in-game connect screen inside the DLL

### `VirtualProtect`

No `VirtualProtect` hits were found in the available old/new runtime trace sets.

Meaning:

- current runtime diff does not tell us when the original applies patch/protect sequences
- static bootstrap evidence is stronger than runtime patch evidence for this question

## 7. Original-DLL Runtime Crosswalk

Useful structural anchors from current 0.3.7-R5 RE and traces:

- `entry0` / `0x100c50c0`: process attach and archive/bootstrap setup
- `0x100990f0`: startup UI/game-state transition cluster
- `0x1007fcf0`, `0x10073550`, `0x10093ee0`: USER32/DXUT-heavy UI control clusters
- original-DLL strings:
  - `Connecting to %s:%d...`
  - `Connected to %.64s`
  - `Lost connection to the server. Reconnecting..`

Best current mapping:

- attach archive/bootstrap ~= `0x100c50c0`
- startup UI / render hook preparation ~= `0x100990f0` + `0x1007fcf0` + `0x10073550` + `0x10093ee0`
- old connect-wait / connected messages ~= `0x10008940` and `0x10010a40`

## 8. Implementation Consequence

The rebuild has been trying to force entry-state/network transitions before the binary-equivalent UI manager and DXUT resource path exist.

The original binary evidence points the other way:

1. bootstrap long-lived managers first
2. bring up DXUT/D3DX resources
3. bring up dialog/control dispatch
4. then drive connect-wait and connected-state UI

That is the main strategic correction.
