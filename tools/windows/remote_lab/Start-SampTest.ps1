param(
    [string]$Root = "C:\samp-test",
    [string]$Scenario = "manual",
    [ValidateSet("samp", "gta")][string]$Target = "samp",
    [string]$ServerHost = "",
    [int]$ServerPort = 0,
    [string]$Nickname = "WinDebug",
    [int]$FavoriteIndex = -1
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$running = @(Get-SampLabProcesses)
if ($running.Count -gt 0) {
    throw "A GTA/SA-MP process is already running: $($running.name -join ', ')"
}

$config = Get-SampLabConfig -Root $Root
$safeScenario = ConvertTo-SampLabName -Value $Scenario
$runId = "{0}_{1}_{2}" -f (Get-Date -Format "yyyyMMdd_HHmmss"), $safeScenario, ([guid]::NewGuid().ToString("N").Substring(0, 8))
$runDir = Join-Path $Root ("runs\" + $runId)
New-SampLabDirectory -Path $runDir

$gameDir = [string]$config.game_dir
$documentsDir = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
$logDir = Join-Path (Join-Path $documentsDir "SA-MP Logs") $runId
New-SampLabDirectory -Path $logDir

# The overlay configuration is deliberately a named profile, not an arbitrary
# environment block. Missing configuration is fail-safe bypass; malformed
# managed state aborts the run rather than launching with ambiguous behavior.
$sampReProfilePath = Join-Path $Root "state\samp_re_profile.json"
$sampReProfileSource = "default_missing"
$sampReProfileChangedAt = $null
$sampReMode = "bypass"
$sampReFunctionList = @(
    "rpc175_set_actor_facing_angle",
    "rpc178_set_actor_health"
)
$sampReFunctions = $sampReFunctionList -join ","
$sampReOverlayPath = Join-Path $gameDir "samp_re.asi"
if (Test-Path -LiteralPath $sampReProfilePath) {
    $sampReProfile = Get-Content -LiteralPath $sampReProfilePath -Raw | ConvertFrom-Json
    if (
        -not ($sampReProfile.PSObject.Properties.Name -contains "schema") -or
        ([int]$sampReProfile.schema) -ne 1 -or
        -not ($sampReProfile.PSObject.Properties.Name -contains "profile") -or
        -not ($sampReProfile.PSObject.Properties.Name -contains "functions")
    ) {
        throw "Invalid samp_re profile schema: $sampReProfilePath"
    }

    $requestedSampReProfile = ([string]$sampReProfile.profile).ToLowerInvariant()
    switch ($requestedSampReProfile) {
        "bypass" { $sampReMode = "bypass" }
        "shadow" { $sampReMode = "shadow" }
        "replace" { $sampReMode = "replace" }
        default { throw "Unsupported samp_re profile in ${sampReProfilePath}: $requestedSampReProfile" }
    }

    $managedFunctions = @($sampReProfile.functions)
    if (
        $managedFunctions.Count -ne $sampReFunctionList.Count
    ) {
        throw "Invalid samp_re function set in $sampReProfilePath"
    }
    for ($functionIndex = 0; $functionIndex -lt $sampReFunctionList.Count; $functionIndex++) {
        if (([string]$managedFunctions[$functionIndex]) -cne $sampReFunctionList[$functionIndex]) {
            throw "Invalid samp_re function set in $sampReProfilePath"
        }
    }

    $sampReProfileSource = "managed_state"
    if ($sampReProfile.PSObject.Properties.Name -contains "changed_at_utc") {
        $sampReProfileChangedAt = [string]$sampReProfile.changed_at_utc
    }
}
if ($sampReMode -ne "bypass" -and -not (Test-Path -LiteralPath $sampReOverlayPath)) {
    throw "Active samp_re profile requires an installed overlay: $sampReOverlayPath"
}
$sampReDisableEnvironmentValue = [Environment]::GetEnvironmentVariable("SAMP_RE_DISABLE")
$sampReDisableEnvironmentEnabled = @("1", "true", "yes", "on", "enabled") -contains $sampReDisableEnvironmentValue

$logStates = @(
    foreach ($name in (Get-SampLabLogNames)) {
        Get-SampLabFileState -Path (Join-Path $logDir $name) -CollectName $name
    }
    # samp_probe.asi intentionally resolves its log next to the ASI. Snapshot
    # that append-only file separately so original-DLL runs get an isolated
    # byte range even though the replacement uses SAMPDLL_LOG_DIR.
    Get-SampLabFileState `
        -Path (Join-Path $gameDir "samp_probe.log") `
        -CollectName "samp_probe.root.log"
    Get-SampLabFileState `
        -Path (Join-Path $gameDir "reloop_control.log") `
        -CollectName "reloop_control.root.log"
    Get-SampLabFileState `
        -Path (Join-Path $gameDir "samp_re.log") `
        -CollectName "samp_re.root.log"
)
$launchName = if ($Target -eq "gta") { [string]$config.game_exe } else { [string]$config.launcher_exe }
$launchPath = Join-Path $gameDir $launchName
if (-not (Test-Path -LiteralPath $launchPath)) {
    throw "Launch target not found: $launchPath"
}

$launchArguments = @()
if ($ServerHost) {
    if ($ServerHost.Length -gt 253 -or $ServerHost -notmatch '^[A-Za-z0-9.-]+$') {
        throw "ServerHost contains unsupported characters: $ServerHost"
    }
    if ($ServerPort -lt 1 -or $ServerPort -gt 65535) {
        throw "ServerPort must be between 1 and 65535."
    }
    if ($Nickname -notmatch '^[A-Za-z0-9_]{3,24}$') {
        throw "Nickname must contain 3-24 ASCII letters, digits, or underscores."
    }
    if ($Target -eq "samp") {
        if ($FavoriteIndex -lt 0 -or $FavoriteIndex -gt 100) {
            throw "FavoriteIndex must be between 0 and 100 for an automated SA-MP browser connection."
        }

        # This script is executed by the interactive desktop agent, so HKCU is
        # the same profile from which samp.exe reads PlayerName.  Keep the
        # browser launch path (the direct gta_sa.exe path can fall through to
        # single-player), but make the requested test nickname authoritative.
        $sampRegistryPath = "HKCU:\Software\SAMP"
        if (-not (Test-Path -LiteralPath $sampRegistryPath)) {
            New-Item -Path $sampRegistryPath -Force | Out-Null
        }
        Set-ItemProperty -LiteralPath $sampRegistryPath -Name "PlayerName" -Value $Nickname
    } else {
        $launchArguments = @("-c", "-h", $ServerHost, "-p", $ServerPort.ToString(), "-n", $Nickname)
    }
}

$startedAt = (Get-Date).ToUniversalTime()
$manifest = [ordered]@{
    schema = 1
    run_id = $runId
    scenario = $safeScenario
    started_at_utc = $startedAt.ToString("o")
    user = [Environment]::UserDomainName + "\" + [Environment]::UserName
    session_id = [Diagnostics.Process]::GetCurrentProcess().SessionId
    game_dir = $gameDir
    log_dir = $logDir
    launch_target = $launchPath
    launch_arguments = @($launchArguments)
    requested_server = if ($ServerHost) { "$($ServerHost):$ServerPort" } else { $null }
    requested_nickname = if ($ServerHost) { $Nickname } else { $null }
    favorite_index = if ($FavoriteIndex -ge 0) { $FavoriteIndex } else { $null }
    gta_sha256 = Get-SampLabHash -Path (Join-Path $gameDir "gta_sa.exe")
    samp_sha256 = Get-SampLabHash -Path (Join-Path $gameDir "samp.dll")
    samp_re = [ordered]@{
        profile = $sampReMode
        mode = $sampReMode
        functions = $sampReFunctionList
        profile_source = $sampReProfileSource
        profile_path = $sampReProfilePath
        profile_changed_at_utc = $sampReProfileChangedAt
        overlay_sha256 = Get-SampLabHash -Path $sampReOverlayPath
        disabled_flag_present = Test-Path -LiteralPath (Join-Path $gameDir "samp_re_disabled.flag")
        disable_environment_enabled = $sampReDisableEnvironmentEnabled
    }
    logs_before = @($logStates)
}
Write-SampLabJson -Value $manifest -Path (Join-Path $runDir "manifest.json")

$startProcessParameters = @{
    FilePath = $launchPath
    WorkingDirectory = $gameDir
    PassThru = $true
}
$env:SAMPDLL_TRACE = "1"
$env:SAMPDLL_LOG_DIR = $logDir
$env:SAMP_RE_MODE = $sampReMode
$env:SAMP_RE_FUNCTIONS = $sampReFunctions
if ($launchArguments.Count -gt 0) {
    $startProcessParameters.ArgumentList = $launchArguments
}
$process = Start-Process @startProcessParameters

if ($Target -eq "samp" -and $ServerHost) {
    $deadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 200
        $process.Refresh()
    } while ($process.MainWindowHandle -eq 0 -and -not $process.HasExited -and (Get-Date) -lt $deadline)
    if ($process.HasExited -or $process.MainWindowHandle -eq 0) {
        throw "SA-MP browser did not expose an interactive window."
    }

    $shell = New-Object -ComObject WScript.Shell
    if (-not $shell.AppActivate($process.Id)) {
        throw "Could not activate the SA-MP browser window."
    }
    Start-Sleep -Milliseconds 300
    $shell.SendKeys("{HOME}")
    for ($index = 0; $index -lt $FavoriteIndex; $index++) {
        $shell.SendKeys("{DOWN}")
    }
    Start-Sleep -Milliseconds 500

    if (-not ("SampBrowserMouse" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class SampBrowserMouse {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
}
"@
    }
    $windowHandle = [SampBrowserMouse]::GetForegroundWindow()
    [uint32]$foregroundProcessId = 0
    [SampBrowserMouse]::GetWindowThreadProcessId($windowHandle, [ref]$foregroundProcessId) | Out-Null
    if ($windowHandle -eq [IntPtr]::Zero -or $foregroundProcessId -ne $process.Id) {
        throw "The SA-MP browser is not the foreground window after favorite selection."
    }
    $windowRect = New-Object SampBrowserMouse+RECT
    if (-not [SampBrowserMouse]::GetWindowRect($windowHandle, [ref]$windowRect)) {
        throw "Could not read the SA-MP browser window bounds."
    }
    $windowWidth = $windowRect.Right - $windowRect.Left
    $windowHeight = $windowRect.Bottom - $windowRect.Top
    if ($windowWidth -lt 600 -or $windowHeight -lt 400) {
        throw "Unexpected SA-MP browser window size: $($windowWidth)x$windowHeight"
    }
    [SampBrowserMouse]::SetCursorPos($windowRect.Left + 11, $windowRect.Top + 64) | Out-Null
    [SampBrowserMouse]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [SampBrowserMouse]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)

    $gta = $null
    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $gta = Get-Process -Name "gta_sa" -ErrorAction SilentlyContinue | Select-Object -First 1
    } while (-not $gta -and (Get-Date) -lt $deadline)
    if (-not $gta) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "The selected SA-MP favorite did not start gta_sa.exe."
    }

    $commandLine = (Get-CimInstance Win32_Process -Filter "ProcessId=$($gta.Id)").CommandLine
    $expectedHost = "-h $ServerHost"
    $expectedPort = "-p $ServerPort"
    if ($commandLine -notlike "*$expectedHost*" -or $commandLine -notlike "*$expectedPort*") {
        Stop-Process -Id $gta.Id -Force -ErrorAction SilentlyContinue
        throw "Selected favorite launched an unexpected endpoint. Expected $ServerHost`:$ServerPort, got: $commandLine"
    }
}
$state = [ordered]@{
    run_id = $runId
    run_dir = $runDir
    scenario = $safeScenario
    started_at_utc = $startedAt.ToString("o")
    launcher_pid = $process.Id
}
Write-SampLabJson -Value $state -Path (Join-Path $Root "state\current_run.json")

Start-Sleep -Seconds 3
$screenshot = $null
try {
    $screenshot = & (Join-Path $PSScriptRoot "Capture-Screenshot.ps1") -Root $Root -Label ("run_" + $runId)
    Copy-Item -LiteralPath $screenshot -Destination (Join-Path $runDir "start.png") -Force
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "start_screenshot.error.txt")
}

[pscustomobject]@{
    run_id = $runId
    run_dir = $runDir
    launcher_pid = $process.Id
    screenshot = $screenshot
    processes = @(Get-SampLabProcesses)
}
