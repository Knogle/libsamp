param(
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath,

    [Parameter(Mandatory = $true)]
    [string]$RockstarRoot,

    [Parameter(Mandatory = $true)]
    [string]$GameDirectoryName,

    [Parameter(Mandatory = $true)]
    [string]$BackupDirectoryName
)

$ErrorActionPreference = 'Stop'
$activeGameProcesses = @(Get-Process -Name gta_sa, samp -ErrorAction SilentlyContinue)
if ($activeGameProcesses.Count -ne 0) {
    throw 'Refusing to replace the GTA tree while gta_sa.exe or samp.exe is running.'
}

$resolvedArchive = (Resolve-Path -LiteralPath $ArchivePath).Path
$resolvedRockstarRoot = (Resolve-Path -LiteralPath $RockstarRoot).Path
$gameRoot = Join-Path $resolvedRockstarRoot $GameDirectoryName
$backupRoot = Join-Path $resolvedRockstarRoot $BackupDirectoryName
$failedRoot = "$gameRoot.failed-extract"

if (-not (Test-Path -LiteralPath $gameRoot -PathType Container)) {
    throw "Game directory does not exist: $gameRoot"
}
if (Test-Path -LiteralPath $backupRoot) {
    throw "Backup destination already exists: $backupRoot"
}
if (Test-Path -LiteralPath $failedRoot) {
    throw "Failed-extraction destination already exists: $failedRoot"
}

Move-Item -LiteralPath $gameRoot -Destination $backupRoot

& tar.exe -xf $resolvedArchive -C $resolvedRockstarRoot
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gameRoot -PathType Container)) {
    if (Test-Path -LiteralPath $gameRoot) {
        Move-Item -LiteralPath $gameRoot -Destination $failedRoot
    }
    Move-Item -LiteralPath $backupRoot -Destination $gameRoot
    throw "Archive extraction failed with exit code $LASTEXITCODE; the original directory was restored."
}

foreach ($requiredFile in @('gta_sa.exe', 'samp.exe', 'samp.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $gameRoot $requiredFile) -PathType Leaf)) {
        throw "Extracted game directory is missing required file: $requiredFile"
    }
}

[pscustomobject]@{
    GameRoot = $gameRoot
    BackupRoot = $backupRoot
    ArchivePath = $resolvedArchive
    GameFiles = @(Get-ChildItem -LiteralPath $gameRoot -Recurse -File).Count
    BackupFiles = @(Get-ChildItem -LiteralPath $backupRoot -Recurse -File).Count
} | ConvertTo-Json -Compress
