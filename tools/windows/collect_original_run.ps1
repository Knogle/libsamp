param(
    [Parameter(Mandatory = $true)]
    [string]$GameDir,

    [string]$OutputRoot = ".\artifacts\windows-original-run",
    [string]$ProcessName = "gta_sa",
    [string]$LauncherProcessName = "samp",
    [string]$LaunchExe = "",
    [string]$GameExe = "",
    [int]$SnapshotCount = 10,
    [int]$SnapshotIntervalSeconds = 2,
    [int]$LauncherWaitSeconds = 20,
    [switch]$LaunchGame,
    [switch]$CollectDxDiag,
    [switch]$CaptureDump
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-Directory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Export-Table {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Rows,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ($Rows.Count -eq 0) {
        Set-Content -LiteralPath $Path -Value ""
        return
    }

    $Rows | Export-Csv -LiteralPath $Path -NoTypeInformation -Delimiter "`t"
}

function Find-OptionalTool {
    param([string[]]$Names)

    foreach ($name in $Names) {
        $cmd = Get-Command -Name $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }

    return $null
}

function Save-RegistryQuery {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Key,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    try {
        reg query $Key /s *> $Path
    } catch {
        Set-Content -LiteralPath $Path -Value $_.Exception.ToString()
    }
}

if (-not (Test-Path -LiteralPath $GameDir)) {
    throw "GameDir not found: $GameDir"
}

$gameDirResolved = (Resolve-Path -LiteralPath $GameDir).Path
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$runDir = Join-Path $OutputRoot ("original_run_" + $timestamp)
New-Directory -Path $runDir

$envInfo = [pscustomobject]@{
    collected_at_utc        = (Get-Date).ToUniversalTime().ToString("o")
    game_dir                = $gameDirResolved
    process_name            = $ProcessName
    launcher_process_name   = $LauncherProcessName
    launch_exe              = $LaunchExe
    computer_name           = $env:COMPUTERNAME
    username                = $env:USERNAME
    powershell_version      = $PSVersionTable.PSVersion.ToString()
    host_architecture       = $env:PROCESSOR_ARCHITECTURE
    host_is_64bit_process   = [Environment]::Is64BitProcess
    host_is_64bit_os        = [Environment]::Is64BitOperatingSystem
}
$envInfo | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDir "runtime_env.json")

try {
    $os = Get-CimInstance -ClassName Win32_OperatingSystem
    $cs = Get-CimInstance -ClassName Win32_ComputerSystem
    $cpu = Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1
    [pscustomobject]@{
        caption          = $os.Caption
        version          = $os.Version
        build_number     = $os.BuildNumber
        install_date     = $os.InstallDate
        last_boot_time   = $os.LastBootUpTime
        total_memory_mb  = [math]::Round($cs.TotalPhysicalMemory / 1MB, 0)
        processor_name   = $cpu.Name
        processor_cores  = $cpu.NumberOfCores
        processor_threads = $cpu.NumberOfLogicalProcessors
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDir "os_summary.json")
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "os_summary.error.txt")
}

try {
    systeminfo | Out-File -LiteralPath (Join-Path $runDir "systeminfo.txt") -Encoding ascii
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "systeminfo.error.txt")
}

if ($CollectDxDiag) {
    try {
        Start-Process -FilePath "dxdiag.exe" -ArgumentList "/dontskip", "/t", (Join-Path $runDir "dxdiag.txt") -Wait -WindowStyle Hidden
    } catch {
        $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "dxdiag.error.txt")
    }
}

Save-RegistryQuery -Key "HKCU\Software\SAMP" -Path (Join-Path $runDir "reg_hkcu_software_samp.txt")
Save-RegistryQuery -Key "HKCU\Software\SA-MP" -Path (Join-Path $runDir "reg_hkcu_software_sa-mp.txt")
Save-RegistryQuery -Key "HKLM\Software\SAMP" -Path (Join-Path $runDir "reg_hklm_software_samp.txt")
Save-RegistryQuery -Key "HKLM\Software\WOW6432Node\SAMP" -Path (Join-Path $runDir "reg_hklm_wow6432node_samp.txt")

$trackedFiles = Get-ChildItem -LiteralPath $gameDirResolved -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -match '^\.(exe|dll|asi|ini|cfg)$' } |
    Sort-Object -Property FullName -Unique

Export-Table -Rows (
    $trackedFiles | ForEach-Object {
        [pscustomobject]@{
            full_name = $_.FullName
            size      = $_.Length
            modified  = $_.LastWriteTimeUtc.ToString("o")
        }
    }
) -Path (Join-Path $runDir "tracked_files.tsv")

Export-Table -Rows (
    $trackedFiles | ForEach-Object {
        $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
        [pscustomobject]@{
            full_name = $_.FullName
            sha256    = $hash.Hash
        }
    }
) -Path (Join-Path $runDir "tracked_hashes.tsv")

Export-Table -Rows (
    $trackedFiles | ForEach-Object {
        $version = (Get-Item -LiteralPath $_.FullName).VersionInfo
        [pscustomobject]@{
            full_name       = $_.FullName
            file_version    = $version.FileVersion
            product_version = $version.ProductVersion
            company_name    = $version.CompanyName
            product_name    = $version.ProductName
            original_name   = $version.OriginalFilename
        }
    }
) -Path (Join-Path $runDir "tracked_versions.tsv")

$sigcheckExe = Find-OptionalTool -Names @("sigcheck64.exe", "sigcheck.exe")
if ($sigcheckExe) {
    try {
        & $sigcheckExe -accepteula -nobanner -h -q ("$gameDirResolved\*") *> (Join-Path $runDir "sigcheck_top_level.txt")
    } catch {
        $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "sigcheck.error.txt")
    }
}

try {
    tasklist /v | Out-File -LiteralPath (Join-Path $runDir "tasklist_before_launch.txt") -Encoding ascii
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "tasklist_before_launch.error.txt")
}

$snapshotScript = Join-Path $PSScriptRoot "dump_process_snapshot.ps1"
if (-not (Test-Path -LiteralPath $snapshotScript)) {
    throw "Missing snapshot helper: $snapshotScript"
}

if ($LaunchGame) {
    if (-not $LaunchExe) {
        $sampExe = Join-Path $gameDirResolved "samp.exe"
        $gtaExe = Join-Path $gameDirResolved "gta_sa.exe"

        if (Test-Path -LiteralPath $sampExe) {
            $LaunchExe = $sampExe
        } elseif ($GameExe) {
            $LaunchExe = $GameExe
        } elseif (Test-Path -LiteralPath $gtaExe) {
            $LaunchExe = $gtaExe
        }
    }

    if (-not $LaunchExe) {
        throw "No launch target found. Set -LaunchExe explicitly."
    }

    if (-not (Test-Path -LiteralPath $LaunchExe)) {
        throw "LaunchExe not found: $LaunchExe"
    }

    Start-Process -FilePath $LaunchExe -WorkingDirectory (Split-Path -Parent $LaunchExe)
}

if ($LauncherProcessName) {
    $launcherProcess = $null
    for ($attempt = 0; $attempt -lt $LauncherWaitSeconds; $attempt++) {
        $launcherProcess = Get-Process -Name $LauncherProcessName -ErrorAction SilentlyContinue |
            Sort-Object -Property StartTime -Descending |
            Select-Object -First 1

        if ($launcherProcess) {
            break
        }

        Start-Sleep -Seconds 1
    }

    if ($launcherProcess) {
        & $snapshotScript -ProcessId $launcherProcess.Id -OutputDir $runDir -Tag "launcher_snapshot"
    }
}

$targetProcess = $null
for ($attempt = 0; $attempt -lt 120; $attempt++) {
    $targetProcess = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
        Sort-Object -Property StartTime -Descending |
        Select-Object -First 1

    if ($targetProcess) {
        break
    }

    Start-Sleep -Seconds 1
}

if (-not $targetProcess) {
    "Process '$ProcessName' was not found. Metadata collection finished without runtime snapshots." |
        Set-Content -LiteralPath (Join-Path $runDir "runtime_note.txt")
    Write-Host "Run directory: $runDir"
    exit 0
}

for ($index = 1; $index -le $SnapshotCount; $index++) {
    $tag = "snapshot_{0:D2}" -f $index
    $doDump = $CaptureDump -and ($index -eq 1)
    & $snapshotScript -ProcessId $targetProcess.Id -OutputDir $runDir -Tag $tag -CaptureDump:$doDump
    Start-Sleep -Seconds $SnapshotIntervalSeconds
}

try {
    tasklist /v /fi "PID eq $($targetProcess.Id)" | Out-File -LiteralPath (Join-Path $runDir "tasklist_after_snapshots.txt") -Encoding ascii
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "tasklist_after_snapshots.error.txt")
}

Write-Host "Run directory: $runDir"
