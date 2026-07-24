param(
    [string]$Root = "C:\samp-test",
    [ValidateSet(
        "passive",
        "no-hooks",
        "asset-paths",
        "custom-object-heavy",
        "textdraw",
        "textdraw-verbose",
        "textdraw-render",
        "font5",
        "actor",
        "actor-heavy",
        "rpc-gap",
        "dialog-menu"
    )]
    [string]$Profile = "passive"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$running = @(Get-SampLabProcesses)
if ($running.Count -gt 0) {
    throw "Refusing to change probe flags while GTA/SA-MP is running: $($running.name -join ', ')"
}

$config = Get-SampLabConfig -Root $Root
$gameDir = [string]$config.game_dir
$probePath = Join-Path $gameDir "samp_probe.asi"
if (-not (Test-Path -LiteralPath $probePath)) {
    throw "Probe ASI is not installed: $probePath"
}

$knownFlags = @(
    "samp_probe_no_hooks.flag",
    "samp_probe_asset_paths.flag",
    "samp_probe_file_hooks.flag",
    "samp_probe_samp_code_hooks.flag",
    "samp_probe_gta_asset_hooks.flag",
    "samp_probe_object_info.flag",
    "samp_probe_custom_object_heavy.flag",
    "samp_probe_textdraw_hooks.flag",
    "samp_probe_textdraw_verbose.flag",
    "samp_probe_textdraw_render.flag",
    "samp_probe_font5_hooks.flag",
    "samp_probe_actor_hooks.flag",
    "samp_probe_actor_heavy.flag",
    "samp_probe_rpc_gap_hooks.flag",
    "samp_probe_dialog_menu_rpc_hooks.flag"
)

$profileFlags = @{
    "passive" = @()
    "no-hooks" = @("samp_probe_no_hooks.flag")
    "asset-paths" = @("samp_probe_asset_paths.flag")
    "custom-object-heavy" = @("samp_probe_custom_object_heavy.flag")
    "textdraw" = @("samp_probe_textdraw_hooks.flag")
    "textdraw-verbose" = @(
        "samp_probe_textdraw_hooks.flag",
        "samp_probe_textdraw_verbose.flag"
    )
    "textdraw-render" = @("samp_probe_textdraw_render.flag")
    "font5" = @("samp_probe_font5_hooks.flag")
    "actor" = @("samp_probe_actor_hooks.flag")
    "actor-heavy" = @("samp_probe_actor_heavy.flag")
    "rpc-gap" = @("samp_probe_rpc_gap_hooks.flag")
    "dialog-menu" = @("samp_probe_dialog_menu_rpc_hooks.flag")
}

$removed = @()
foreach ($name in $knownFlags) {
    $path = Join-Path $gameDir $name
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
        $removed += $name
    }
}

$enabled = @()
foreach ($name in @($profileFlags[$Profile])) {
    $path = Join-Path $gameDir $name
    [IO.File]::WriteAllText(
        $path,
        "managed profile=$Profile utc=$((Get-Date).ToUniversalTime().ToString('o'))`r`n",
        [Text.UTF8Encoding]::new($false)
    )
    $enabled += $name
}

[pscustomobject]@{
    profile = $Profile
    probe = $probePath
    probe_sha256 = Get-SampLabHash -Path $probePath
    removed = $removed
    enabled = $enabled
    changed_at_utc = (Get-Date).ToUniversalTime().ToString("o")
} | ConvertTo-Json -Depth 4
