# Windows SA-MP Remote Test Lab

This kit installs a small, auditable automation layer under `C:\samp-test`.
It is intended for a dedicated Windows test machine with an already installed
GTA San Andreas/SA-MP tree. It does not copy proprietary game assets into the
repository.

## Security model

- `sshadmin` controls scripts, configuration, incoming DLLs, and deployments.
- SSH password login and forwarding are disabled. The Windows firewall and the
  authorized key both restrict access to source addresses in `192.168.0.0/16`.
- The logged-on desktop user can write command results, run artifacts, dumps,
  screenshots, and agent state, but cannot modify installed agent scripts.
- The interactive queue accepts only `ping`, `screenshot`, constrained named
  keys/clicks inside the active GTA window, `start`, `collect`, and `stop`. It
  cannot execute arbitrary command lines or arbitrary key strings.
- Deployment requires an x86 PE image, refuses to run while GTA/SA-MP is
  active, backs up the installed DLL, and verifies SHA-256 after installation.
- Windows Error Reporting writes full dumps for `gta_sa.exe` and `samp.exe`.

## Installation

From an elevated 64-bit PowerShell:

```powershell
.\Install-SampTestLab.ps1 `
  -Root 'C:\samp-test' `
  -GameDir 'C:\Program Files (x86)\Rockstar Games\GTA San Andreas' `
  -InteractiveUser 'DESKTOP-7PGINHC\defaultusr'
```

The installer registers the `SampTestInteractiveAgent` scheduled task with an
interactive-token logon. The task runs in the visible user's desktop session,
which is required for DirectX startup and screenshots.

## Common operations

From the Linux workspace, the passwordless wrapper uses
`~/.ssh/id_default` and `sshadmin@192.168.3.180` by default:

```bash
tools/windows/remote_lab/samp_lab.sh ping
tools/windows/remote_lab/samp_lab.sh key ENTER language_english
tools/windows/remote_lab/samp_lab.sh key ACTORS actor_cycle
tools/windows/remote_lab/samp_lab.sh key RPC176RAW actor_position_raw
tools/windows/remote_lab/samp_lab.sh click 1176 674 spawn
tools/windows/remote_lab/samp_lab.sh validate build-win32/samp.dll
tools/windows/remote_lab/samp_lab.sh deploy build-win32/samp.dll local-build
tools/windows/remote_lab/samp_lab.sh start connect_spawn samp 192.168.3.181 7778 WinDebug
tools/windows/remote_lab/samp_lab.sh collect
tools/windows/remote_lab/samp_lab.sh stop
tools/windows/remote_lab/samp_lab.sh probe-profile actor-heavy
tools/windows/remote_lab/samp_lab.sh overlay-profile shadow
tools/windows/remote_lab/samp_lab.sh overlay-kill on
tools/windows/remote_lab/samp_lab.sh fetch-run RUN_ID /tmp/samp-runs
```

Override `SAMP_LAB_HOST` or `SAMP_LAB_KEY` when the machine address or key
changes. The wrapper never stores or accepts a password.

From Windows directly, use the commands below.

Validate a candidate without modifying the game directory:

```powershell
C:\samp-test\scripts\Deploy-SampDll.ps1 -ValidateOnly
```

Deploy `C:\samp-test\incoming\samp.dll`:

```powershell
C:\samp-test\scripts\Deploy-SampDll.ps1 -Label local-build
```

Verify the interactive agent:

```powershell
C:\samp-test\scripts\Submit-SampTestCommand.ps1 -Action ping
C:\samp-test\scripts\Submit-SampTestCommand.ps1 -Action screenshot -Label desktop
```

Start, collect, and stop a test:

```powershell
C:\samp-test\scripts\Submit-SampTestCommand.ps1 `
  -Action start `
  -Scenario connect_spawn `
  -Target samp `
  -ServerHost 192.168.3.181 `
  -ServerPort 7778 `
  -Nickname WinDebug
C:\samp-test\scripts\Submit-SampTestCommand.ps1 -Action collect
C:\samp-test\scripts\Submit-SampTestCommand.ps1 -Action stop
```

Each run records the installed `samp.dll` hash and the pre-run byte length of
every known log. Collection keeps both complete logs and `latest_log_bytes`, so
older `process_attach` blocks are not mistaken for the current run. Original
probe output is collected from the game root as `samp_probe.root.log`, because
`samp_probe.asi` writes next to itself rather than to `SAMPDLL_LOG_DIR`.

Module inventories use Toolhelp with both `TH32CS_SNAPMODULE` and
`TH32CS_SNAPMODULE32`; this lets the 64-bit PowerShell agent enumerate the
32-bit GTA process, including `samp.dll` and loaded ASIs.

`probe-profile` removes only the probe's fixed, known flag-file allowlist and
then enables the selected named profile. It refuses to change flags while a
GTA/SA-MP process is running; arbitrary flag names and arbitrary commands are
not accepted.

`overlay-profile` selects only `bypass`, `shadow`, or `replace` for
`samp_re.asi` and refuses changes while GTA/SA-MP is running. The interactive
starter validates the managed profile again, sets the fixed
`SAMP_RE_FUNCTIONS=rpc175_set_actor_facing_angle,rpc178_set_actor_health`
tokens, and records the effective
mode, function token, overlay hash, and both kill-switch states in each run
manifest. If no managed profile exists, the starter explicitly uses `bypass`;
malformed profile state aborts the launch. `bypass` can be selected even when
the overlay is absent; active profiles require an installed `samp_re.asi`.

`overlay-kill on` creates only the fixed `samp_re_disabled.flag`. During an
active run this changes the installed overlay to permanent original-trampoline
passthrough for the rest of that process. `overlay-kill off` removes only that
managed flag and refuses to do so while GTA/SA-MP is running.

If the game directory is protected and its existing trace files were created
by an administrator, grant the configured interactive test user access only to
the fixed trace-file allowlist (`samp_runtime.log`, `samp_net_trace.log`,
`samp_hook_trace.log`, `samp_probe.log`, `reloop_control.log`, and
`samp_re.log`) before starting a traced run:

```powershell
C:\samp-test\scripts\Enable-SampTestLogging.ps1 -Root C:\samp-test
```

After changing those ACLs, `samp_lab.sh logcheck` appends one fixed diagnostic
marker to `samp_re.log` through the limited interactive agent. It does not
accept a path or arbitrary content.

`deploy-probe` and `deploy-overlay` accept only the fixed ASI names
`samp_probe.asi` and `samp_re.asi`. Deployment refuses while GTA/SA-MP is
running, validates x86 PE identity, backs up an existing target, stages via a
temporary file, verifies SHA-256 after install, and writes a deployment
manifest.

## Result layout

```text
C:\samp-test
|-- backups
|-- commands
|   |-- pending
|   |-- processing
|   |-- done
|   `-- failed
|-- deployments
|-- dumps
|-- incoming
|-- runs
|-- screenshots
|-- scripts
|-- state
`-- tools\sysinternals
```

WinDbg may remain an AppX application rather than a command on `PATH`.
`Install-Sysinternals.ps1` installs signed ProcDump binaries and adds their
directory to the machine `PATH`; all downloaded executables are checked with
Authenticode before use.
