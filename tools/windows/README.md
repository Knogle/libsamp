# Windows Original-Run Collection Kit

Goal: extract high-value evidence from a native Windows run of the original `samp.dll` so we can reproduce ABI and runtime behavior against the same baseline later.

This kit is intentionally compatibility-first. It prioritizes:

1. exact file provenance,
2. loaded-module and thread snapshots,
3. file/registry/image-load traces,
4. dynamic `LoadLibrary` / `GetProcAddress` behavior,
5. crash and loader-state evidence.

## Recommended Layout

Create one output root such as:

```text
C:\samp-collect
```

Copy this directory there or point commands at the scripts inside the repository checkout.

Run everything from **64-bit PowerShell** on Windows.

If Windows prompts with repeated "Run only scripts that you trust" warnings, remove the downloaded-file mark first:

```powershell
Unblock-File .\collect_original_run.ps1
Unblock-File .\dump_process_snapshot.ps1
```

## Pass 1: Baseline Metadata And Runtime Snapshots

This captures hashes, versions, registry probes, system metadata, and repeated snapshots of the live `gta_sa.exe` process.

The script is launcher-aware:

1. if `samp.exe` exists in the game directory, `-LaunchGame` will prefer launching that first,
2. it then tries to snapshot the launcher process,
3. after that it waits for and snapshots the resident `gta_sa.exe` process.

Example:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
cd C:\path\to\sa-mp.dll-rebuild\tools\windows
.\collect_original_run.ps1 `
  -GameDir 'C:\Games\GTA San Andreas' `
  -OutputRoot 'C:\samp-collect' `
  -ProcessName 'gta_sa' `
  -LauncherProcessName 'samp' `
  -LaunchGame `
  -CollectDxDiag `
  -SnapshotCount 12 `
  -SnapshotIntervalSeconds 2
```

If the game is already running, omit `-LaunchGame`.

If you want to force the exact launcher binary explicitly:

```powershell
.\collect_original_run.ps1 `
  -GameDir 'C:\Games\GTA San Andreas' `
  -OutputRoot 'C:\samp-collect' `
  -ProcessName 'gta_sa' `
  -LauncherProcessName 'samp' `
  -LaunchGame `
  -LaunchExe 'C:\Games\GTA San Andreas\samp.exe'
```

What this produces:

1. `tracked_hashes.tsv`: SHA256 for relevant `.exe/.dll/.asi/.ini/.cfg`
2. `tracked_versions.tsv`: file-version metadata
3. `systeminfo.txt`, `os_summary.json`, optional `dxdiag.txt`
4. `reg_*.txt`: likely SA-MP registry keys
5. `snapshot_XX/`: process JSON, modules, threads, windows, `tasklist`, `netstat`
6. optional `listdlls`, `handle`, and full dump output if Sysinternals tools are on `PATH`

If you want a full process dump at the first snapshot:

```powershell
.\collect_original_run.ps1 `
  -GameDir 'C:\Games\GTA San Andreas' `
  -OutputRoot 'C:\samp-collect' `
  -ProcessName 'gta_sa' `
  -LaunchGame `
  -CaptureDump
```

`procdump.exe` must be available on `PATH` for that flag to do anything.

## Pass 2: Procmon Capture

Procmon is the best source for:

1. `Load Image`
2. `CreateFile`
3. `RegOpenKey` / `RegQueryValue`
4. missing-file probing
5. unexpected DLL search paths

### First-Time GUI Filter Setup

Open Procmon and create these filters:

1. `Process Name` `is` `samp.exe` `Include`
2. `Process Name` `is` `gta_sa.exe` `Include`
3. `Operation` `is` `Load Image` `Include`
4. `Operation` `is` `CreateFile` `Include`
5. `Operation` `is` `RegOpenKey` `Include`
6. `Operation` `is` `RegQueryValue` `Include`
7. `Operation` `is` `Process Create` `Include`
8. `Operation` `is` `Thread Create` `Include`
9. `Path` `ends with` `\samp.dll` `Include`
10. `Path` `ends with` `\BASS.dll` `Include`
11. `Path` `ends with` `\d3dx9_25.dll` `Include`
12. `Path` `contains` `GTA San Andreas` `Include`

Important: keep `NAME NOT FOUND` results visible. Those misses are often more useful than the successful opens.

Save the filter configuration to something like:

```text
C:\samp-collect\procmon_samp.pmc
```

### CLI Capture

Start capture:

```cmd
cd C:\path\to\sa-mp.dll-rebuild\tools\windows
start_procmon_capture.cmd C:\samp-collect\procmon_original.pml Procmon64.exe C:\samp-collect\procmon_samp.pmc
```

Run the original client through startup, main menu, and one connect attempt.

Stop capture:

```cmd
cd C:\path\to\sa-mp.dll-rebuild\tools\windows
stop_procmon_capture.cmd Procmon64.exe
```

Then export from Procmon:

1. open `procmon_original.pml`
2. `File -> Save`
3. save both `Native Process Monitor Format (PML)` and `Events displayed using current filter` as `CSV`

Keep both the raw `PML` and filtered `CSV`.

## Pass 3: WinDbg Loader Trace

This gives us dynamic loader behavior the static import table cannot show.

### Setup

1. Install WinDbg Preview or classic WinDbg.
2. Create `C:\samp-collect`.
3. Edit the first line of [`windbg_original_run.txt`](windbg_original_run.txt) so `.logopen` points to a real path on your machine.

### Launch Command

Use a command file so the session is reproducible:

Launch against the real SA-MP start path and let WinDbg follow child processes:

```cmd
windbg.exe -o -G -c "$$< C:\path\to\sa-mp.dll-rebuild\tools\windows\windbg_original_run.txt" "C:\Games\GTA San Andreas\samp.exe"
```

If you are bypassing the launcher for a special test, target `gta_sa.exe` directly instead.

What this captures:

1. module loads for `samp.dll`, `BASS.dll`, `d3dx9_25.dll`
2. `LoadLibraryA/W`, `GetProcAddress`, `FreeLibrary`
3. `CreateThread`, `ExitProcess`
4. key window, registry, and mutex APIs
5. `WSAStartup`, `socket`, `connect`
6. `Direct3DCreate9`, `BASS_Init`
7. access violations with registers, stack, and loaded modules

Artifacts to keep:

1. `windbg_original.log`
2. any manual `.dump /ma` dump you decide to take
3. screenshots only if a breakpoint shows an address or module state not present in the log

## Pass 4: Optional Deep Capture

These are useful if you already have the basics above.

### Sysinternals

If these are on `PATH`, the scripts will use them automatically when possible:

1. `sigcheck.exe`
2. `listdlls.exe`
3. `handle.exe`
4. `procdump.exe`

### Manual Full Dump

Inside WinDbg, once `samp.dll` is loaded and the UI is stable:

```text
.dump /ma C:\samp-collect\gta_sa_after_samp_load.dmp
```

### Repeat At Key Milestones

Run the same collection at:

1. immediately after process start,
2. once `samp.dll` is loaded,
3. once the SA-MP UI is visible,
4. during a server connect attempt,
5. immediately before crash or disconnect if one happens.

## What To Send Back

Highest-value files:

1. the whole timestamped directory from `collect_original_run.ps1`
2. `procmon_original.pml`
3. `procmon_original.csv`
4. `windbg_original.log`
5. any `.dmp`
6. exact GTA/SA-MP game directory tree or at least the hash/version TSVs

## Notes

1. Always run the original `samp.dll` first.
2. Later, repeat the identical procedure against our rebuilt candidate.
3. Differential evidence from the same machine is more valuable than isolated traces from different environments.
