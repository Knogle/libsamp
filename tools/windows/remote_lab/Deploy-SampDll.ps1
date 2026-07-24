param(
    [string]$Candidate = "C:\samp-test\incoming\samp.dll",
    [string]$Root = "C:\samp-test",
    [string]$Label = "candidate",
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

function Get-PeMachine {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $reader = New-Object IO.BinaryReader $stream
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Missing MZ header: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Missing PE signature: $Path" }
        return $reader.ReadUInt16()
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $Candidate)) {
    throw "Candidate DLL not found: $Candidate"
}
$candidateResolved = (Resolve-Path -LiteralPath $Candidate).Path
$candidateItem = Get-Item -LiteralPath $candidateResolved
$machine = Get-PeMachine -Path $candidateResolved
if ($machine -ne 0x014C) {
    throw ("Candidate is not an x86 PE image (machine=0x{0:X4}): {1}" -f $machine, $candidateResolved)
}

$config = Get-SampLabConfig -Root $Root
$target = Join-Path ([string]$config.game_dir) "samp.dll"
$candidateHash = Get-SampLabHash -Path $candidateResolved
$targetHashBefore = Get-SampLabHash -Path $target

$validation = [ordered]@{
    candidate = $candidateResolved
    candidate_sha256 = $candidateHash
    candidate_size = $candidateItem.Length
    pe_machine = "0x014C"
    target = $target
    target_sha256_before = $targetHashBefore
    validated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
}

if ($ValidateOnly) {
    [pscustomobject]$validation | ConvertTo-Json -Depth 4
    exit 0
}

$running = @(Get-SampLabProcesses)
if ($running.Count -gt 0) {
    throw "Refusing deployment while GTA/SA-MP processes are running: $($running.name -join ', ')"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$safeLabel = ConvertTo-SampLabName -Value $Label
$backup = $null
if (Test-Path -LiteralPath $target) {
    $backupName = "samp_{0}_{1}.dll" -f $stamp, $targetHashBefore.Substring(0, 12)
    $backup = Join-Path $Root ("backups\" + $backupName)
    Copy-Item -LiteralPath $target -Destination $backup -ErrorAction Stop
}

$temporary = "$target.codex-next"
Copy-Item -LiteralPath $candidateResolved -Destination $temporary -Force
if ((Get-SampLabHash -Path $temporary) -ne $candidateHash) {
    Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
    throw "Candidate hash changed while staging the DLL."
}
Move-Item -LiteralPath $temporary -Destination $target -Force

$targetHashAfter = Get-SampLabHash -Path $target
if ($targetHashAfter -ne $candidateHash) {
    throw "Installed DLL hash does not match the candidate hash. Backup: $backup"
}

$manifest = [ordered]@{
    schema = 1
    label = $safeLabel
    deployed_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    deployed_by = [Environment]::UserDomainName + "\" + [Environment]::UserName
    candidate = $candidateResolved
    candidate_sha256 = $candidateHash
    target = $target
    target_sha256_before = $targetHashBefore
    target_sha256_after = $targetHashAfter
    backup = $backup
}
$manifestPath = Join-Path $Root ("deployments\{0}_{1}.json" -f $stamp, $safeLabel)
Write-SampLabJson -Value $manifest -Path $manifestPath
[pscustomobject]$manifest | ConvertTo-Json -Depth 4
