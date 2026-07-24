param(
    [string]$Root = "C:\samp-test",
    [string]$Label = "manual",
    [string]$OutputPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

if (-not $OutputPath) {
    $safeLabel = ConvertTo-SampLabName -Value $Label
    $OutputPath = Join-Path $Root ("screenshots\{0}_{1}.png" -f (Get-Date -Format "yyyyMMdd_HHmmss_fff"), $safeLabel)
}

$bounds = [Windows.Forms.SystemInformation]::VirtualScreen
if ($bounds.Width -le 0 -or $bounds.Height -le 0) {
    throw "The interactive session has no capturable virtual screen."
}

$bitmap = New-Object Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen($bounds.Location, [Drawing.Point]::Empty, $bounds.Size)
    $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}

Write-Output $OutputPath
