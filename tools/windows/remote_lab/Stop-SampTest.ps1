param([string]$Root = "C:\samp-test")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$collectionBefore = $null
if (Get-SampLabCurrentRun -Root $Root) {
    try { $collectionBefore = & (Join-Path $PSScriptRoot "Collect-SampRun.ps1") -Root $Root } catch { }
}

$stopped = @()
foreach ($process in @(Get-SampLabProcesses)) {
    try {
        Stop-Process -Id $process.id -ErrorAction Stop
        $stopped += $process
    } catch {
        try {
            Stop-Process -Id $process.id -Force -ErrorAction Stop
            $stopped += $process
        } catch { }
    }
}

Start-Sleep -Seconds 2
$remaining = @(Get-SampLabProcesses)
$clearedRunState = $false
if ($remaining.Count -eq 0) {
    $currentRunPath = Join-Path $Root "state\current_run.json"
    if (Test-Path -LiteralPath $currentRunPath) {
        Remove-Item -LiteralPath $currentRunPath -Force
        $clearedRunState = $true
    }
}
[pscustomobject]@{
    stopped = $stopped
    remaining = $remaining
    pre_stop_collection = $collectionBefore
    cleared_run_state = $clearedRunState
}
