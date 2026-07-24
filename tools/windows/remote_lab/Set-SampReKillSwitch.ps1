param(
    [string]$Root = "C:\samp-test",
    [ValidateSet("on", "off")]
    [string]$State
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$config = Get-SampLabConfig -Root $Root
$gameDir = [string]$config.game_dir
$overlayPath = Join-Path $gameDir "samp_re.asi"
$flagPath = Join-Path $gameDir "samp_re_disabled.flag"
$running = @(Get-SampLabProcesses)

if ($State -eq "on") {
    if (-not (Test-Path -LiteralPath $overlayPath)) {
        throw "samp_re overlay is not installed: $overlayPath"
    }
    [IO.File]::WriteAllText(
        $flagPath,
        "managed kill_switch=on utc=$((Get-Date).ToUniversalTime().ToString('o'))`r`n",
        [Text.Encoding]::ASCII
    )
} else {
    if ($running.Count -gt 0) {
        throw "Refusing to clear samp_re kill switch while GTA/SA-MP is running: $($running.name -join ', ')"
    }
    if (Test-Path -LiteralPath $flagPath) {
        Remove-Item -LiteralPath $flagPath -Force
    }
}

[pscustomobject]@{
    state = $State
    flag_path = $flagPath
    flag_present = Test-Path -LiteralPath $flagPath
    overlay_sha256 = Get-SampLabHash -Path $overlayPath
    processes = @($running)
    changed_at_utc = (Get-Date).ToUniversalTime().ToString("o")
} | ConvertTo-Json -Depth 5
