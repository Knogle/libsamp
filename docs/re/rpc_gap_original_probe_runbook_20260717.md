# Original R5 RPC 43/112/113/116/117 probe

## Scope

This focused run captures the exact original SA-MP 0.3.7-R5 path for:

- `RemoveBuildingForPlayer` (RPC 43);
- `PlayCrimeReport` (RPC 112);
- `SetPlayerAttachedObject` create/remove (RPC 113);
- `EditAttachedObject` begin/response (RPC 116);
- `EditObject` begin/response (RPC 117).

It is designed for the local bare open.mp fixture and the original/reference
prefix. Do not combine it with unrelated deep probe modes.

Reference identities:

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
samp_probe.asi SHA256=f9b1656246de405670e4927568f6a0249dfa005ba929aa1bfc9571a1dddc8b36
```

The ASI hash is for the 2026-07-17 build of this runbook. Recompute and record
it when the probe source changes.

## Evidence basis

`STATIC_037`: the local Ghidra export and disassembly for the exact DLL above
establish these `RegisterAsRemoteProcedureCall` mappings and downstream calls:

| Surface | Original R5 RVA |
| --- | --- |
| RPC 43 handler | `samp.dll+0x1d530` |
| RPC 112 handler | `samp.dll+0x19050` |
| RPC 113 handler | `samp.dll+0x18f00` |
| RPC 116 handler | `samp.dll+0x117e0` |
| RPC 117 handler | `samp.dll+0x118a0` |
| Building/dummy and object removal | `samp.dll+0x9d020`, `+0x9cff0` |
| Police-scanner helper | `samp.dll+0xa1790` |
| Player attached-object slot helper | `samp.dll+0xb0b10` |
| Object/attached editor begin | `samp.dll+0x72420`, `+0x724e0` |

The ASI requires the matching PE identity, validates complete-instruction
prologues for all eleven functions, and installs nothing unless all eleven
preflight checks succeed. A successful original run upgrades actual calls and
state changes to `PROBE_TRACE`. Audible output and visible GUI/gizmo behavior
must be recorded separately as `OBSERVED_037`.

## Setup

Build:

```bash
toolbox run -c devbuild bash -lc \
  'cd /home/chairman/Projects/sa-mp.dll-rebuild && tools/asi_probe/build_win32.sh'
```

Original GTA root:

```text
/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas
```

Place `samp_probe.asi` there and enable only:

```text
samp_probe_rpc_gap_hooks.flag
```

Disable unrelated deep flags, especially:

```text
samp_probe_samp_code_hooks.flag
samp_probe_font5_hooks.flag
samp_probe_actor_hooks.flag
samp_probe_actor_heavy.flag
samp_probe_gta_asset_hooks.flag
samp_probe_textdraw_render.flag
```

Keep this mode process-bound: start a fresh GTA process, perform the short
route below, then exit the process cleanly. Do not hot-unload the ASI.

## Deterministic route

Connect the original client to the bare server at `127.0.0.1:7778` and spawn.
Use this exact order:

1. At Area 51, observe which vanilla buildings are absent. Move far enough to
   force a stream-out/stream-in and return once. The gamemode sends six RPC 43
   rules on connect for models `16203`, `16590`, `16323`, `16619`, `1697`, and
   `16094`.
2. Run `/rpcpattach`. Record the helmet model's visible bone, offset, rotation,
   scale, and whether it follows the ped animation.
3. Run `/rpcpattachedit`. Record the complete original GUI: buttons/labels,
   3D gizmo, selected mode/axis, mouse capture, and camera behavior. Change one
   translation, one rotation, and one scale value, then save/confirm.
4. Run `/rpcpattachedit` again, make one visible change, then press Escape to
   exercise cancel.
5. Run `/rpcpattachoff` and confirm the object disappears.
6. Run `/rpcedit`. Move and rotate the test object, save/confirm, then repeat
   `/rpcedit` and cancel with Escape.
7. On foot, run `/rpccrime` once and record the exact audible police-scanner
   phrase, delay, and whether other game audio continues.
8. Enter any available vehicle, run `/rpccrime` once more, and record whether
   the scanner adds a vehicle type/colour description.
9. Exit the client cleanly.

Do not repeat commands rapidly. Wait for the visible/audio effect and the
server callback before starting the next step.

## Expected log evidence

Startup must contain:

```text
rpc_gap_hook: set_preflight_ok hooks=11 rpc_handlers=5 downstream=6
rpc_gap_hook: summary installed=11 requested=11
```

Primary records:

- `rpc_gap`: bounded plaintext RPC payload and matching begin/end sequence;
- `rpc_gap_remove`: model, removed flag, simple Z, and matrix Z before/after;
- `rpc_gap_crime`: decoded scanner-helper arguments and GTA patch bytes
  before/after the original call;
- `rpc_gap_attached`: complete 52-byte input/stored slot, active flag, and
  original `0x119c` object-wrapper pointer;
- `rpc_gap_editor`: original editor mode, active flag, target, position, UI
  pointers, and bounded raw state before/after GUI setup.

The bare server log should also contain `OnPlayerEditObject` and
`OnPlayerEditAttachedObject` callbacks for update/final/cancel responses.

## Acceptance and rejection

Accept the trace only if:

- all eleven hooks pass preflight and install;
- every focused RPC has matching `phase=begin` and `phase=end`;
- RPC 43 produces at least one matching `rpc_gap_remove` mutation when a
  targeted entity is resident;
- RPC 113 create reaches `rpc_gap_attached` with `active=1` and a non-null
  object pointer, while remove makes the attachment visibly disappear;
- RPC 116/117 reach their matching `rpc_gap_editor_begin` records and server
  callbacks distinguish save/final from Escape/cancel;
- both crime runs reach `rpc_gap_crime`, and the manual observation states
  explicitly whether scanner audio was audible;
- the log has no `exception:` and the process exits cleanly.

Reject and rerun if the log contains `set_preflight_failed`,
`incomplete_install`, an unsupported identity, an exception, or mixed records
from an older process block.

## Comparison target

After the clean original run, repeat the same command order with the
replacement/libsamp prefix. Compare:

1. remove-entity model/flag/Z transitions and stream-in persistence;
2. crime ID/position/vehicle arguments and audible output;
3. all 52 attachment bytes, active/object lifecycle, bone-follow rendering,
   non-uniform scale, and both material colours;
4. editor mode transitions, visible controls, mouse/camera behavior, and exact
   update/final/cancel server callbacks.

Pointer values, timestamps, and sequence counters must be normalized before a
text diff. Visible and audible notes remain separate observations rather than
being inferred from the trace.
