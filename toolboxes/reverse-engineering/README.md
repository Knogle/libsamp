# Reverse-Engineering Toolbox

This toolbox is the isolated working environment for high-friction reverse-engineering and binary compatibility work around `samp.dll`.

## Goal

Keep host pollution low while making Wine, debuggers, MinGW, and PE-analysis tools reproducibly available.

## What It Installs

Core tools:

1. Wine and `winedbg`
2. `gdb`
3. `rizin`
4. MinGW 32-bit toolchain
5. `cmake`, `ninja`, `jq`, `ripgrep`, `7z`, `cabextract`
6. Python packages for PE work (`pefile`, `capstone`, `lief`)

## Files

1. `create.sh`: creates and provisions the toolbox.
2. `run.sh`: runs a command inside the toolbox.
3. `provision.sh`: package install logic executed inside the toolbox.
4. `packages.fedora.txt`: package baseline for Fedora-based toolboxes.
5. `requirements.txt`: Python packages used for PE/reverse work.

## Usage

Create or refresh the toolbox:

```bash
toolboxes/reverse-engineering/create.sh
```

Run a strict PE diff inside the toolbox:

```bash
toolboxes/reverse-engineering/run.sh \
  bash -lc 'cd /path/to/libsamp && tools/check_pe_abi_strict.sh samp.dll build-win32/samp.dll'
```

Run the existing Win32 build inside the toolbox:

```bash
toolboxes/reverse-engineering/run.sh \
  bash -lc 'cd /path/to/libsamp && reimpl/scripts/build_win32.sh'
```

## Recommended Workflow

1. Build candidate DLL.
2. Run `tools/check_pe_abi_strict.sh`.
3. Capture or refresh Wine traces.
4. Run `tools/compare_runtime_traces.sh`.
5. Promote findings into specs, not directly into `reimpl/`.

## Notes

1. This toolbox is for Team A style work and validation tooling. It does not relax clean-room rules for Team B.
2. If Fedora package names drift, adjust `packages.fedora.txt` and rerun `create.sh`.
