param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')

Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
    ForEach-Object {
        [pscustomobject]@{
            Path = $_.FullName.Substring($resolvedRoot.Length + 1)
            Length = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } |
    Sort-Object Path |
    Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8

Get-Item -LiteralPath $OutputPath |
    Select-Object FullName, Length, LastWriteTimeUtc |
    ConvertTo-Json -Compress
