param(
    [string]$Root = "C:\samp-test",
    [ValidateSet("bypass", "shadow", "replace")]
    [string]$Profile = "bypass"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$running = @(Get-SampLabProcesses)
if ($running.Count -gt 0) {
    throw "Refusing to change samp_re profile while GTA/SA-MP is running: $($running.name -join ', ')"
}

$config = Get-SampLabConfig -Root $Root
$gameDir = [string]$config.game_dir
$overlayPath = Join-Path $gameDir "samp_re.asi"
if ($Profile -ne "bypass" -and -not (Test-Path -LiteralPath $overlayPath)) {
    throw "samp_re overlay is not installed: $overlayPath"
}

$normalizedProfile = $Profile.ToLowerInvariant()
$changedAt = (Get-Date).ToUniversalTime()
$profilePath = Join-Path $Root "state\samp_re_profile.json"
$functions = @(
    "rpc175_set_actor_facing_angle",
    "rpc178_set_actor_health"
)
$profileState = [ordered]@{
    schema = 1
    profile = $normalizedProfile
    functions = $functions
    changed_at_utc = $changedAt.ToString("o")
    changed_by = [Environment]::UserDomainName + "\" + [Environment]::UserName
}
Write-SampLabJson -Value $profileState -Path $profilePath

[pscustomobject]@{
    profile = $normalizedProfile
    functions = $functions
    profile_path = $profilePath
    overlay = $overlayPath
    overlay_present = Test-Path -LiteralPath $overlayPath
    overlay_sha256 = Get-SampLabHash -Path $overlayPath
    disabled_flag_present = Test-Path -LiteralPath (Join-Path $gameDir "samp_re_disabled.flag")
    changed_at_utc = $changedAt.ToString("o")
} | ConvertTo-Json -Depth 4
