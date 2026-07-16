# TextDraw 0.3.7 Probe Runbook - 2026-07-06

## Scope

Goal: capture enough original-client evidence to replace the current textdraw
path with a stack that matches SA-MP 0.3.7 behavior instead of only matching
open.mp semantics.

Evidence status:

- `PROBE_TRACE`: existing replacement logs cover RPC ingress/show/hide/edit and
  select-mode state, but not original-client CFont order.
- `OPENMP_REF`: scripting API behavior used to build the probe gamemode.
- `GTA_REVERSED_REF`: CFont address family and GTA-side font state path.
- `INFERRED`: legacy client/server textdraw sources describe a compatible
  older stack, but are not proof of 0.3.7 `samp.dll` behavior.
- `TODO_VERIFY`: render setter order, box alpha, selectable hit bounds, preview
  model handling, and string conversion helper contracts.

## References Checked

- `docs/re/textdraw_render_stack.md`
- `samp/client/game/textdraw.cpp`
- `samp/client/game/textdraw.h`
- `samp/client/net/textdrawpool.cpp`
- `samp/server/textdrawpool.cpp`
- `local_reference_sources/saco/game/textdraw.cpp`
- `local_reference_sources/saco/net/textdrawpool.cpp`
- open.mp docs: `TextDrawCreate`, `TextDrawTextSize`,
  `TextDrawSetSelectable`, `SelectTextDraw`, `OnPlayerClickTextDraw`,
  `PlayerTextDrawCreate`, `OnPlayerClickPlayerTextDraw`

## Probe Pieces

- `omp-server-bare/gamemodes/textdraw_probe.pwn`: bare gamemode that creates
  global and player textdraws covering fonts 0-5, alignments, boxes, alpha
  colors, shadow/outline, proportional toggles, multiline/color-tag text,
  selectable global/player textdraws, sprite previews, model previews,
  `TextDrawSetString`, hide/show, and select cancellation.
- `tools/asi_probe/src/samp_probe_asi.c`: opt-in GTA CFont hooks for focused
  original-client traces. Enable with `SAMP_PROBE_TEXTDRAW_HOOKS=1` or a
  `samp_probe_textdraw_hooks.flag` file next to `samp_probe.asi`. For complete
  setter-order traces, also enable `SAMP_PROBE_TEXTDRAW_VERBOSE=1` or create
  `samp_probe_textdraw_verbose.flag`.
- `tools/analyze_textdraw_trace.sh`: post-processes probe/runtime/server logs
  into RPC events, CFont call sequence, caller counts, print strings, and probe
  callback events.
- `tools/trim_wine_trace.sh`: now also emits `textdraw_focus.log` when trimming
  mixed Wine/probe logs.

## Build

Pawn gamemode:

```bash
cd omp-server-bare/gamemodes
../qawno/pawncc textdraw_probe.pwn -D. -d3 -Z -i../qawno/include -otextdraw_probe.amx
```

Probe ASI:

```bash
toolbox run -c reverse-engineering tools/asi_probe/build_win32.sh
```

## Original-Client Capture Recipe

1. Temporarily set `omp-server-bare/config.json` `pawn.main_scripts` to
   `["textdraw_probe 1"]`.
2. Copy `build-asi-probe/samp_probe.asi` next to the original client's
   `gta_sa.exe`.
3. Create `samp_probe_textdraw_hooks.flag` next to `samp_probe.asi`, or set
   `SAMP_PROBE_TEXTDRAW_HOOKS=1`. For the final order comparison, also create
   `samp_probe_textdraw_verbose.flag`.
4. Start the bare server, then join with the original SA-MP 0.3.7 client.
5. Run this scenario in order:
   `/tdshow`, `/tdstrings`, `/tdpreview`, `/tdselect`, click selectable global
   textdraws, click selectable player textdraws, press ESC, `/tdhide`,
   `/tdshow`, `/tdrebuild`.
6. Analyze logs:

```bash
tools/analyze_textdraw_trace.sh \
  -s omp-server-bare/log.txt \
  -o analysis/generated/textdraw_probe_037 \
  '/path/to/GTA San Andreas/samp_probe.log'
```

## What To Compare

- `font_order.log`: CFont setter order around each `CFont.PrintString`.
- `font_caller_counts.tsv`: original-DLL caller RVAs that drive textdraw draw
  state.
- `textdraw_rpc_events.log`: show/hide/edit/select/click network path.
- `server_events.log`: open.mp callback and command events from the probe
  script.
- `print_strings.log`: final GTA-visible strings after SA-MP conversion.

The replacement should not be changed to claim 0.3.7 parity until these files
are captured from the original client and the relevant behavior is promoted
from `TODO_VERIFY` to `OBSERVED_037`/`PROBE_TRACE`.
