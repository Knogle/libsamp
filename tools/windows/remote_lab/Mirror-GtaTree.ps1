param(
    [Parameter(Mandatory = $true)]
    [string]$StagingRoot,

    [Parameter(Mandatory = $true)]
    [string]$GameRoot,

    [Parameter(Mandatory = $true)]
    [string]$BackupRoot
)

$ErrorActionPreference = 'Stop'

$activeGameProcesses = @(Get-Process -Name gta_sa, samp -ErrorAction SilentlyContinue)
if ($activeGameProcesses.Count -ne 0) {
    throw "Refusing to mirror while gta_sa.exe or samp.exe is running."
}

$resolvedStaging = (Resolve-Path -LiteralPath $StagingRoot).Path
$resolvedGame = (Resolve-Path -LiteralPath $GameRoot).Path
New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
$resolvedBackup = (Resolve-Path -LiteralPath $BackupRoot).Path

function Invoke-RobocopyMirror {
    param(
        [string]$Source,
        [string]$Destination
    )

    & robocopy.exe $Source $Destination /MIR /COPY:DAT /DCOPY:DAT /XJ /R:1 /W:1 /NFL /NDL /NP /NJH /NJS
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy failed with exit code $LASTEXITCODE while mirroring '$Source' to '$Destination'."
    }
}

Invoke-RobocopyMirror -Source $resolvedGame -Destination $resolvedBackup
Invoke-RobocopyMirror -Source $resolvedStaging -Destination $resolvedGame

[pscustomobject]@{
    GameRoot = $resolvedGame
    BackupRoot = $resolvedBackup
    StagingRoot = $resolvedStaging
    GameFiles = @(Get-ChildItem -LiteralPath $resolvedGame -Recurse -File).Count
    BackupFiles = @(Get-ChildItem -LiteralPath $resolvedBackup -Recurse -File).Count
} | ConvertTo-Json -Compress
