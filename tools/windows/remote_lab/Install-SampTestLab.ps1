param(
    [string]$Root = "C:\samp-test",
    [string]$GameDir = "C:\Program Files (x86)\Rockstar Games\GTA San Andreas",
    [string]$InteractiveUser = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Install-SampTestLab.ps1 must run as an administrator."
}

if (-not (Test-Path -LiteralPath (Join-Path $GameDir "gta_sa.exe"))) {
    throw "GTA San Andreas executable not found under: $GameDir"
}

if (-not $InteractiveUser) {
    $InteractiveUser = (Get-CimInstance Win32_ComputerSystem).UserName
}
if (-not $InteractiveUser) {
    throw "No interactive Windows user is logged on. Pass -InteractiveUser explicitly."
}

$directories = @(
    $Root,
    (Join-Path $Root "backups"),
    (Join-Path $Root "commands\pending"),
    (Join-Path $Root "commands\processing"),
    (Join-Path $Root "commands\done"),
    (Join-Path $Root "commands\failed"),
    (Join-Path $Root "deployments"),
    (Join-Path $Root "dumps"),
    (Join-Path $Root "incoming"),
    (Join-Path $Root "runs"),
    (Join-Path $Root "screenshots"),
    (Join-Path $Root "scripts"),
    (Join-Path $Root "state"),
    (Join-Path $Root "tools\sysinternals")
)
foreach ($directory in $directories) {
    New-SampLabDirectory -Path $directory
}

$scriptDestination = Join-Path $Root "scripts"
Get-ChildItem -LiteralPath $PSScriptRoot -File |
    Where-Object { $_.Extension -in @(".ps1", ".md") } |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $scriptDestination $_.Name) -Force
        Unblock-File -LiteralPath (Join-Path $scriptDestination $_.Name) -ErrorAction SilentlyContinue
    }

$config = [ordered]@{
    schema = 1
    root = $Root
    game_dir = (Resolve-Path -LiteralPath $GameDir).Path
    launcher_exe = "samp.exe"
    game_exe = "gta_sa.exe"
    process_names = @("gta_sa", "samp")
    interactive_user = $InteractiveUser
    installed_at_utc = (Get-Date).ToUniversalTime().ToString("o")
}
Write-SampLabJson -Value $config -Path (Join-Path $Root "config.json")

# Keep scripts and configuration administrator-controlled. The desktop user can
# write only command results, run artifacts, screenshots, dumps, and state.
& icacls.exe $Root /inheritance:r | Out-Null
& icacls.exe $Root /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Failed to secure $Root" }

$writable = @("commands", "dumps", "runs", "screenshots", "state")
foreach ($name in $writable) {
    $path = Join-Path $Root $name
    & icacls.exe $path /grant "${InteractiveUser}:(OI)(CI)M" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Failed to grant desktop write access to $path" }
}
& icacls.exe $scriptDestination /grant "${InteractiveUser}:(OI)(CI)RX" | Out-Null
& icacls.exe (Join-Path $Root "config.json") /grant "${InteractiveUser}:R" | Out-Null

$dumpFolder = Join-Path $Root "dumps"
foreach ($exeName in @("gta_sa.exe", "samp.exe")) {
    $key = "HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\$exeName"
    New-Item -Path $key -Force | Out-Null
    New-ItemProperty -Path $key -Name DumpFolder -PropertyType ExpandString -Value $dumpFolder -Force | Out-Null
    New-ItemProperty -Path $key -Name DumpType -PropertyType DWord -Value 2 -Force | Out-Null
    New-ItemProperty -Path $key -Name DumpCount -PropertyType DWord -Value 20 -Force | Out-Null
}

$taskName = "SampTestInteractiveAgent"
$taskCommand = "powershell.exe"
$taskArguments = "-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$scriptDestination\SampTestAgent.ps1`" -Root `"$Root`""
$action = New-ScheduledTaskAction -Execute $taskCommand -Argument $taskArguments
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $InteractiveUser
$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew `
    -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1)
$principalConfig = New-ScheduledTaskPrincipal -UserId $InteractiveUser -LogonType Interactive -RunLevel Limited
Register-ScheduledTask `
    -TaskName $taskName `
    -Action $action `
    -Trigger $trigger `
    -Settings $settings `
    -Principal $principalConfig `
    -Description "Interactive screenshot and SA-MP test agent for the samp.dll compatibility lab." `
    -Force | Out-Null

Start-ScheduledTask -TaskName $taskName

[pscustomobject]@{
    root = $Root
    game_dir = $config.game_dir
    interactive_user = $InteractiveUser
    task_name = $taskName
    dump_folder = $dumpFolder
    scripts = @(Get-ChildItem -LiteralPath $scriptDestination -File | Select-Object -ExpandProperty Name)
} | ConvertTo-Json -Depth 4
