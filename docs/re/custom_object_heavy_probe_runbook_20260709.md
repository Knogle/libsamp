# SA-MP 0.3.7 Custom Object Heavy Probe Runbook

Date: 2026-07-09

Evidence target: `OBSERVED_037` + `PROBE_TRACE` + `TODO_VERIFY`

## Goal

Capture how the original SA-MP 0.3.7 DLL registers and binds custom object
models, especially the low SAMP custom range that currently crashes the
replacement client around model `11683`.

This probe is intentionally heavier than the normal asset trace, but still
avoids full `ReadFile` tracing by default.

## Prefix

Original/reference prefix:

```text
/home/chairman/Games/san-andreas-multiplayer-legacy-legacy
```

Original/reference GTA root:

```text
/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas
```

## Probe Setup

Build the ASI probe from the repo root:

```sh
tools/asi_probe/build_win32.sh
```

Install the probe into the original/reference GTA root:

```sh
cp build-asi-probe/samp_probe.asi "/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas/samp_probe.asi"
```

Enable only the heavy custom-object flag:

```sh
cp /tmp/samp_probe_custom_object_heavy.flag "/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas/samp_probe_custom_object_heavy.flag"
```

Do not enable `samp_probe_file_hooks.flag` for this baseline unless a very
short file-read trace is explicitly needed.

## Scenario

Use the same local test server/gamemode that drives the replacement-client
object scan. The server does not need extra validation for this probe.

Recommended first run:

```text
/sampobjscan 11682 5000 3000
```

Then repeat for the next known failing model if the first run is clean:

```text
/sampobjscan 11683 5000 3000
```

The long hold/gap keeps the original-DLL trace sparse enough to inspect while
still forcing object create/destroy and `CPhysical::Add` observations.

## Expected Log Lines

Inspect `samp_probe.log` in the original/reference GTA root. The important
lines are:

```text
custom_object_heavy: store_addresses ...
custom_object_heavy: add_image ...
custom_object_heavy: load_cd_directory_begin ...
custom_object_heavy: load_cd_directory_end ...
custom_object_heavy: model_add ...
custom_object_heavy: col_add_slot ...
custom_object_heavy: col_load_begin ...
custom_object_heavy: col_load_end ...
custom_object_heavy: physical_add ...
gta_object_info: phase=...
```

`stack_samp='...'` is the key addition over the older GTA hooks. `AddAtomicModel`
is often called by GTA loader code directly, so the immediate return address can
be inside GTA. The stack scan records nearby return addresses that point back
into `samp.dll`, which should identify the original SA-MP registration path.

## Diff Targets

Compare the original-DLL heavy trace with replacement runs for:

- image registration order for `CUSTOM.IMG`, `SAMP.IMG`, `GTA3.IMG`,
  `GTA_INT.IMG`, and `SAMPCOL.IMG`
- `CStreaming::LoadCdDirectory` begin/end store counter deltas
- `CModelInfo::AddAtomicModel` count and model IDs around store count
  `13990..15417`
- model-info slot address, `slot_before`, `slot_after`, and `result` for
  `11682`, `11683`, `11684`, and selected high custom IDs
- `gta_object_info` raw bytes, `txd_index`, `draw_distance`, and `col_model`
- collision slot names and `CColStore::LoadCol` slot/size/order
- `CPhysical::Add` model, `model_info_ptr`, RW object, and status for the
  spawned custom object
- `stack_samp` RVAs that identify original `samp.dll` call paths

## Interpretation Rules

- Treat raw logs from the original DLL as `OBSERVED_037` + `PROBE_TRACE`.
- Treat field names and GTA struct offsets as `GTA_REVERSED_REF` +
  `TODO_VERIFY`; raw bytes are the durable evidence.
- Do not infer pointer-patch behavior from a single `model_add` line. Confirm
  with `slot_before/slot_after`, store counters, `gta_object_info`, and later
  `physical_add`.
- If the original DLL registers model IDs beyond the vanilla array, record the
  exact slot addresses and store counters before changing replacement behavior.
