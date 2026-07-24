param([string]$Root = "C:\samp-test")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$toolDir = Join-Path $Root "tools\sysinternals"
$zipPath = Join-Path $Root "state\Procdump.zip"
New-SampLabDirectory -Path $toolDir

Invoke-WebRequest -UseBasicParsing -Uri "https://download.sysinternals.com/files/Procdump.zip" -OutFile $zipPath
Expand-Archive -LiteralPath $zipPath -DestinationPath $toolDir -Force

$verified = foreach ($name in @("procdump.exe", "procdump64.exe", "procdump64a.exe")) {
    $path = Join-Path $toolDir $name
    if (-not (Test-Path -LiteralPath $path)) { continue }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    if ($signature.Status -ne "Valid" -or $signature.SignerCertificate.Subject -notmatch "Microsoft") {
        throw "Authenticode verification failed for $path (status=$($signature.Status))."
    }
    [pscustomobject]@{
        path = $path
        sha256 = Get-SampLabHash -Path $path
        signer = $signature.SignerCertificate.Subject
        status = $signature.Status.ToString()
    }
}

$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if (($machinePath -split ';') -notcontains $toolDir) {
    [Environment]::SetEnvironmentVariable("Path", ($machinePath.TrimEnd(';') + ";" + $toolDir), "Machine")
}

$verified | ConvertTo-Json -Depth 4
