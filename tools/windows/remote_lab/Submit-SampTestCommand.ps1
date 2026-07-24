param(
    [ValidateSet("ping", "screenshot", "input", "start", "collect", "stop", "logcheck")]
    [string]$Action = "ping",
    [string]$Scenario = "manual",
    [ValidateSet("samp", "gta")]
    [string]$Target = "samp",
    [string]$ServerHost = "",
    [int]$ServerPort = 0,
    [string]$Nickname = "WinDebug",
    [int]$FavoriteIndex = -1,
    [ValidateSet("key", "click")][string]$InputMode = "key",
    [ValidateSet("ENTER", "ESCAPE", "SPACE", "UP", "DOWN", "LEFT", "RIGHT", "MODE", "CLASS", "KILL", "QUIT", "SFA", "LVA", "AA", "ACTORS", "ACTORSOFF", "RPC175EDGE", "RPC175EDGEOFF", "RPC175RAW", "RPC176RAW", "RPC178EDGE", "RPC178EDGEOFF")][string]$InputKey = "ENTER",
    [int]$InputX = 0,
    [int]$InputY = 0,
    [string]$Label = "manual",
    [string]$Root = "C:\samp-test",
    [int]$WaitSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$id = "{0}_{1}_{2}" -f (
    Get-Date -Format "yyyyMMdd_HHmmss"
), (ConvertTo-SampLabName -Value $Action), ([guid]::NewGuid().ToString("N").Substring(0, 8))
$name = "$id.json"
$pendingPath = Join-Path $Root "commands\pending\$name"
$donePath = Join-Path $Root "commands\done\$name"
$failedPath = Join-Path $Root "commands\failed\$name"

$command = [ordered]@{
    schema = 1
    id = $id
    action = $Action
    scenario = $Scenario
    target = $Target
    server_host = $ServerHost
    server_port = $ServerPort
    nickname = $Nickname
    favorite_index = $FavoriteIndex
    input_mode = $InputMode
    input_key = $InputKey
    input_x = $InputX
    input_y = $InputY
    label = $Label
    submitted_utc = (Get-Date).ToUniversalTime().ToString("o")
    submitted_by = [Environment]::UserDomainName + "\" + [Environment]::UserName
}
Write-SampLabJson -Value $command -Path $pendingPath

for ($elapsed = 0; $elapsed -le $WaitSeconds; $elapsed++) {
    if (Test-Path -LiteralPath $donePath) {
        Get-Content -LiteralPath $donePath -Raw
        exit 0
    }
    if (Test-Path -LiteralPath $failedPath) {
        Get-Content -LiteralPath $failedPath -Raw
        exit 1
    }
    if ($elapsed -lt $WaitSeconds) {
        Start-Sleep -Seconds 1
    }
}

[pscustomobject]@{
    id = $id
    status = "queued"
    pending_path = $pendingPath
} | ConvertTo-Json -Depth 3
