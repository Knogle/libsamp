param([string]$Root = "C:\samp-test")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

function Copy-NewLogBytes {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][long]$Offset,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) { return }
    $input = [IO.File]::Open($Source, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        if ($Offset -lt 0 -or $Offset -gt $input.Length) { $Offset = 0 }
        $input.Position = $Offset
        $output = [IO.File]::Open($Destination, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read)
        try { $input.CopyTo($output) } finally { $output.Dispose() }
    } finally {
        $input.Dispose()
    }
}

$current = Get-SampLabCurrentRun -Root $Root
if (-not $current) {
    throw "No current test run is registered."
}
$runDir = [string]$current.run_dir
$manifestPath = Join-Path $runDir "manifest.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$gameDir = [string]$manifest.game_dir
$logsDir = Join-Path $runDir "logs"
$latestDir = Join-Path $runDir "latest_log_bytes"
New-SampLabDirectory -Path $logsDir
New-SampLabDirectory -Path $latestDir

foreach ($before in @($manifest.logs_before)) {
    $source = [string]$before.path
    $collectName = if ($before.PSObject.Properties.Name -contains "collect_name" -and
        [string]$before.collect_name) {
        Split-Path -Leaf ([string]$before.collect_name)
    } else {
        Split-Path -Leaf $source
    }
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $logsDir $collectName) -Force
        Copy-NewLogBytes `
            -Source $source `
            -Offset ([long]$before.length) `
            -Destination (Join-Path $latestDir $collectName)
    }
}

$processes = @(Get-SampLabProcesses)
Write-SampLabJson -Value $processes -Path (Join-Path $runDir "processes.json")
foreach ($process in $processes) {
    try {
        $modules = Get-SampLabModules -ProcessId $process.id
        $modules | Export-Csv -LiteralPath (Join-Path $runDir ("modules_{0}_{1}.tsv" -f $process.name, $process.id)) -NoTypeInformation -Delimiter "`t"
    } catch {
        $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir ("modules_{0}_{1}.error.txt" -f $process.name, $process.id))
    }
}

$screenshot = $null
try {
    $screenshot = & (Join-Path $PSScriptRoot "Capture-Screenshot.ps1") -Root $Root -Label ("collect_" + [string]$current.run_id)
    Copy-Item -LiteralPath $screenshot -Destination (Join-Path $runDir ("collect_{0}.png" -f (Get-Date -Format "yyyyMMdd_HHmmss"))) -Force
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "collect_screenshot.error.txt")
}

$runStart = [datetime]::Parse([string]$current.started_at_utc).ToUniversalTime()
$dumpDir = Join-Path $Root "dumps"
$runDumpDir = Join-Path $runDir "dumps"
New-SampLabDirectory -Path $runDumpDir
Get-ChildItem -LiteralPath $dumpDir -Filter "*.dmp" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTimeUtc -ge $runStart } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $runDumpDir $_.Name) -Force }

try {
    $events = @(
        Get-WinEvent -FilterHashtable @{ LogName = "Application"; StartTime = $runStart.ToLocalTime() } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -match "Application Error|Windows Error Reporting" -or $_.Message -match "gta_sa.exe|samp.exe" } |
        Select-Object TimeCreated, Id, LevelDisplayName, ProviderName, Message
    )
    $eventPath = Join-Path $runDir "application_events.tsv"
    if ($events.Count -gt 0) {
        $events | Export-Csv -LiteralPath $eventPath -NoTypeInformation -Delimiter "`t"
    } else {
        Set-Content -LiteralPath $eventPath -Value ""
    }
    Remove-Item -LiteralPath (Join-Path $runDir "application_events.error.txt") -Force -ErrorAction SilentlyContinue
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $runDir "application_events.error.txt")
}

$result = [ordered]@{
    collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    run_id = [string]$current.run_id
    run_dir = $runDir
    processes = $processes
    screenshot = $screenshot
    dumps = @(Get-ChildItem -LiteralPath $runDumpDir -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
}
Write-SampLabJson -Value $result -Path (Join-Path $runDir "last_collection.json")
[pscustomobject]$result
