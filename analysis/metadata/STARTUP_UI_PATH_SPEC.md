# Startup/UI Path Spec for Original `samp.dll`

Date: 2026-03-05

Purpose: pin down the original startup path that gets SA:MP from DLL attach to the in-game connect UI, so the rebuild can stop guessing at `StartGame` timing and instead reimplement the same stages in the same order.

## Executive Summary

The original binary is not doing "connect first, UI later". The evidence supports a three-stage startup:

1. Early bootstrap and resource setup in `0x100c50c0`
2. DXUT/D3DX-backed UI asset and message/control setup around `0x100990f0`, `0x1007fcf0`, `0x10073550`, `0x10093ee0`
3. Connect-state UI and network-state promotion around `0x10008940` and `0x10010a40`

That means the rebuild should not try to jump directly from entry gating into `CNetGame`-like behavior. The missing piece is the UI/control stack that sits between bootstrap and connect.

## Confirmed Bootstrap Chain

### 1) Entry and CRT/runtime dispatch

- `entry0 @ 0x100cbc90`
  - DLL reason dispatcher
  - calls `0x100cbb0f`
  - calls `0x100c5270`
- `0x100cbb0f`
  - OS/version capture via `GetVersionExA`
  - command-line capture via `GetCommandLineA`
  - process/thread attach bookkeeping
  - this is runtime scaffolding, not the connect UI
- `0x100c5270`
  - direct jump to `0x100c50c0`

### 2) First real SA:MP bootstrap in `0x100c50c0`

This is the first high-confidence SA:MP-specific startup stage:

- stores module handle in `data.1026eb38`
- installs `SetUnhandledExceptionFilter(data.10060970)`
- rewrites module path to `samp.saa`
- allocates and constructs archive/fs object via `0x10065350`
- stores archive object in `data.1026eb44`
- loads `samp.saa`
- installs fonts:
  - `gtaweap3.ttf`
  - `sampaux3.ttf`
- calls `0x10062ca0`
- calls `0x10062970`
- allocates `0x142` bytes, constructs object via `0x1009ff80`
- stores that object in `data.1026ebac`

Interpretation:

- `0x100c50c0` is the original equivalent of the old `DllMain` resource/bootstrap work.
- It is already creating long-lived SA:MP objects used later by connect-state code.

## Early Support Init Directly Under Bootstrap

### `0x10062ca0`

This is not generic fluff. It resolves and stores file API entrypoints at runtime:

- `CreateFileA`
- `ReadFile`
- `GetFileSize`
- `SetFilePointer`
- `CloseHandle`
- `GetFileType`

Interpretation:

- early filesystem shim / hook bootstrap
- consistent with archive-backed asset loading

### `0x10062970`

This resolves `user32!ShowCursor` dynamically and stores the pointer.

Interpretation:

- cursor/UI support is part of the bootstrap, not bolted on later

### `0x1009ff80`

Constructor-like routine called from `0x100c50c0`, stored into `data.1026ebac`.

Observed behavior:

- allocates small sub-objects
- zeroes large internal state regions
- initializes default values and flags

Interpretation:

- this is likely a UI/scene/control manager, not the network core
- important because the same `data.1026ebac` object is reused later by connect-state functions

## String Xrefs and Their Meaning

### `Connecting to %s:%d...`

- string: `0x100e599c`
- xref: `0x1000896d`
- function: `0x10008940`

Observed behavior in `0x10008940`:

- throttled by `GetTickCount()` with a 3000 ms interval
- formats host string from `[esi+0x30]`
- formats port from `[esi+0x235]`
- emits banner through `0x100680f0`
- updates state field `[esi+0x3cd] = 2`

Interpretation:

- this is the connect-wait banner ticker
- not bootstrap
- it runs after the UI/control stack already exists

### `Connected to {B9C9BF}%.64s`

- string: `0x100e6060`
- xref: `0x10010a87`
- function: `0x10010a40`

Observed behavior in `0x10010a40`:

- calls `0x100a1560`
- conditionally calls `0x100a71c0`
- uses `data.1026eb94` and `data.1026ebac`
- pushes banner into `0x100680f0`
- sets `[edx+0x3cd] = 5`
- calls `0x10004080`
- calls `0x1001f8d0`

Interpretation:

- post-connect transition function
- not just chat text; it coordinates UI/state progression after acceptance

## First Confirmed DXUT/D3DX Asset Init

### `UI\\DXUTShared.fx`

- string: `0x100ebc04`
- code use: `0x100990fb`
- function: `0x100990f0`

Observed behavior in `0x100990f0`:

- path-build helper `0x10097830`
- `D3DXCreateEffectFromFileA("UI\\DXUTShared.fx")`
- `D3DXLoadMeshFromXA("UI\\arrow.x")`
- stores resources in globals near `0x10143c08..0x10143c10`
- allocates mesh-related memory via `0x100c627a`
- performs COM/vtable calls on loaded resources

Interpretation:

- this is real SA:MP UI asset initialization
- not a dead helper
- it is the clearest static proof that the original binary builds a DXUT/D3DX UI resource path before normal connect-state rendering

Important caveat:

- rizin currently reports `indegree=0` for `0x100990f0`
- that likely means indirect dispatch, vtable linkage, or incomplete analysis
- it does not weaken the function's role as an asset loader

## Render/UI Callgraph Slices

### `0x1007fcf0`

Profile:

- size `2719`
- `111` basic blocks
- `18` incoming references
- heavy USER32 usage

Known callers:

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

Observed imports and behavior:

- `OpenClipboard`
- `EmptyClipboard`
- `TranslateAcceleratorA`
- `DispatchMessageA`
- window sizing, visibility, message dispatch, nested UI helpers

Interpretation:

- high-level dialog/window/message dispatcher
- closest current-binary analog to old `SetupGameUI` plus DXUT event plumbing

### `0x10073550`

Known caller:

- `0x100618c4`

Observed behavior:

- mouse messages around `0x200`, `0x201`, `0x202`
- `GetCursorPos`
- `ScreenToClient`
- `SetCapture`
- `ClipCursor`
- `ReleaseCapture`
- `SetCursorPos`

Interpretation:

- low-level cursor/capture controller under the UI stack

### `0x10093ee0`

Known caller:

- `0x10094edc`

Observed behavior:

- widget-local message handling
- `PtInRect`
- `GetCursorPos`
- `SetCapture`
- `ReleaseCapture`
- toggles control state bytes and bitfields

Interpretation:

- per-control or per-widget interaction handler
- part of the connect/dialog control layer

### `0x10095d10`

Known callers:

- `0x10097556`
- `0x100975a1`

Observed behavior:

- `D3DXMatrixInverse`
- `D3DXMatrixRotationQuaternion`
- `D3DXMatrixMultiply`
- `D3DXVec3Normalize`
- `D3DXVec3TransformNormal`

Interpretation:

- render/math helper only
- important subordinate cluster, but not a startup root

## Runtime Diff Implications

What the captured runtime evidence does support:

- original olddll loads `d3dx9_25.dll` very early
- original olddll loads `BASS.dll` very early
- original connect banner text passes through:
  - `MultiByteToWideChar`
  - `gdi32.ScriptItemize`

Interpretation:

- the connect banner is rendered through an active text/UI pipeline, not just a raw debug print
- D3DX/UI dependencies are already in place before or during connect-wait rendering

What the captured runtime evidence does not currently support:

- no `VirtualProtect` calls were captured in the available old/new runtime traces

Interpretation:

- we cannot currently use runtime logs to prove or diff patch-sequence timing via `VirtualProtect`
- the stronger evidence is static bootstrap/API-resolver analysis plus the connect-text render path

Important caution:

- the large `CreateWindowEx*` burst in `runtime_olddll_bigone/call_focus*.log` begins before `LoadLibraryA("samp.dll")`
- that window activity is likely from the launcher/browser process, not from the in-game DLL UI path
- do not use those lines as proof for the in-game connect screen

## Old 0.2x Crosswalk

The old source still matches the staged structure well:

1. `DllMain` + archive bootstrap
2. `LaunchMonitor`
3. `DoInitStuff`
4. `SetupGameUI`
5. `IDirect3DDevice9Hook` / render hooks
6. `CNetGame`

Relevant old-source anchors:

- `saco/main.cpp`
  - `LaunchMonitor`
  - `SetupGameUI`
  - `DoInitStuff`
  - `TheGraphicsLoop`
- `saco/net/netgame.cpp`
  - `GAMESTATE_WAIT_CONNECT`
  - `Connecting to %s:%d...`
  - `ID_CONNECTION_REQUEST_ACCEPTED`
- `saco/net/netrpc.cpp`
  - `Connected to %.64s`

## Rebuild Order Implied by the Original Binary

Recommended implementation order:

1. Recreate `0x100c50c0` behavior
   - archive load
   - font install
   - early fs/cursor shims
   - creation of the long-lived manager object analogous to `data.1026ebac`
2. Recreate the DXUT/UI asset stage
   - `DXUTShared.fx`
   - `arrow.x`
   - resource object storage and initialization
3. Recreate the UI dispatcher/control layer
   - `0x1007fcf0`
   - `0x10073550`
   - `0x10093ee0`
4. Only then recreate connect-state flow
   - connect-wait banner
   - connected banner
   - promotion from wait to connected

## Bottom Line

The missing behavior in the rebuild is not just "one more hook" or "a better entry gate". The original SA:MP startup is UI-first enough that the connect flow depends on bootstrap objects and a working DXUT/control stack. Reimplementing network state without that stack is structurally backwards.
