# Original 0.3.7-R5 Dialog/Menu RPC Probe

## Question

Determine whether the replacement dialog/menu desync comes from the outgoing
wire contract or from local client UI/state handling. The focused probe records
the original client's outgoing:

- RPC 50 `ServerCommand` as scenario correlation;
- RPC 62 `DialogResponse`;
- RPC 132 `MenuSelect`;
- RPC 140 `MenuQuit`.

## Evidence Boundary

- `STATIC_037` + `PROBE_TRACE`: local R5 `CNetGame` pointer at
  `samp.dll+0x26eb94`.
- `STATIC_037` + `PROBE_TRACE`: reconstructed legacy `CNetGame` places
  RakClient first; the runtime object resolves slot 26 (`+0x68`) to
  `samp.dll+0x345b0`. R5 disassembly shows that wrapper dereferences `RPCID*`,
  copies an 8-byte packed `NetworkID`, and returns with `ret 0x24`.
- `OPENMP_REF`: RPC 62 payload is `int16 dialogId`, `uint8 response`,
  `int16 listItem`, `dynstr8 input`.
- `STATIC_037`: reconstructed legacy `CMenuPool::Process` sends RPC 132 with a
  one-byte row and RPC 140 with an empty BitStream, both `HIGH_PRIORITY`,
  `RELIABLE`, channel 0.
- `TODO_VERIFY`: exact original RPC 62 transport parameters and all runtime
  callsite RVAs remain unproven until this probe produces a clean trace.

Target DLL identity:

```text
SA-MP 0.3.7-R5
SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
TimeDateStamp=0x6372c39e
EntryRVA=0x000cbc90
SizeOfImage=0x0027e000
```

## Build

```bash
toolbox run -c reverse-engineering /usr/bin/bash -lc \
  'cd /home/chairman/Projects/sa-mp.dll-rebuild && tools/asi_probe/build_win32.sh'
```

Output: `build-asi-probe/samp_probe.asi`.

## Original Prefix Setup

Use the original/reference GTA root:

```text
/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas
```

Copy `samp_probe.asi` there and create only:

```text
samp_probe_dialog_menu_rpc_hooks.flag
```

For this focused run, remove/disable other optional probe flag files,
especially RPC-gap, Actor, Font5, textdraw-render, generic SA-MP-code, and
heavy asset hooks. The normal passive socket/state tracing may remain enabled.

## Scenario

Use the same server, gamemode, spawn location, and command sequence for the
original and replacement clients.

1. Connect and spawn.
2. Open `/attachments`.
3. Select the first attachment list row and accept the next dialog.
4. Reopen `/attachments`, select a row, then cancel/back out.
5. If a legacy SA-MP menu fixture is available, select one row once and open it
   again to cancel once.
6. Exit `gta_sa.exe` cleanly; do not hot-unload the ASI.

The direct attachment command can be used as a control, but it does not test
the dialog chain and therefore cannot replace steps 2-4.

## Valid Trace Markers

A usable trace contains:

```text
dialog_menu_rpc_hook: installed
dialog_menu_rpc: before rpc=50 name=ServerCommand
dialog_menu_rpc: before rpc=62 name=DialogResponse
dialog_menu_rpc: after rpc=62
```

The RPC 50 record must contain `command='/attachments'`. For a menu fixture it
additionally contains RPC 132 and/or 140 records. Reject
the run if it contains `unsupported_identity`, `slot_target_outside_samp`,
`vtable_changed`, or `exception:`.

Compare these fields against the replacement trace:

- payload bits and bytes;
- dialog ID, response, list item, input length/data;
- priority, reliability, channel, shift-timestamp;
- original call result;
- follow-up server RPC/dialog and whether the active dialog clears.

## Decision Rule

- Any field mismatch is a protocol/send-path defect and should be corrected
  before changing UI state.
- Matching RPC 62 plus a missing server follow-up indicates server-side
  acceptance/callback or ordering needs inspection.
- Matching RPC and server follow-up but stuck/hidden UI indicates local
  dialog/menu lifecycle, input edge handling, or active-ID cleanup.
