param([string]$Root = "C:\samp-test")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$config = Get-SampLabConfig -Root $Root
$gameDir = [string]$config.game_dir
$interactiveUser = [string]$config.interactive_user
$rule = New-Object Security.AccessControl.FileSystemAccessRule(
    $interactiveUser,
    [Security.AccessControl.FileSystemRights]::Modify,
    [Security.AccessControl.AccessControlType]::Allow
)
$logNames = @(
    "samp_runtime.log",
    "samp_net_trace.log",
    "samp_hook_trace.log",
    "samp_probe.log",
    "reloop_control.log",
    "samp_re.log"
)
$results = foreach ($name in $logNames) {
    $path = Join-Path $gameDir $name
    if (-not (Test-Path -LiteralPath $path)) {
        New-Item -ItemType File -Path $path -Force | Out-Null
    }
    $acl = Get-Acl -LiteralPath $path
    $acl.SetAccessRule($rule)
    Set-Acl -LiteralPath $path -AclObject $acl
    [pscustomobject]@{
        path = $path
        user = $interactiveUser
        rights = "Modify"
        length = (Get-Item -LiteralPath $path).Length
    }
}

[pscustomobject]@{
    configured_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    logs = @($results)
}
