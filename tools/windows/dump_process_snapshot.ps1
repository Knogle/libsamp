param(
    [int]$ProcessId = 0,
    [string]$ProcessName = "gta_sa",
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [string]$Tag = "snapshot",
    [switch]$CaptureDump
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-Directory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Export-Table {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Rows,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ($Rows.Count -eq 0) {
        Set-Content -LiteralPath $Path -Value ""
        return
    }

    $Rows | Export-Csv -LiteralPath $Path -NoTypeInformation -Delimiter "`t"
}

function Find-OptionalTool {
    param([string[]]$Names)

    foreach ($name in $Names) {
        $cmd = Get-Command -Name $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }

    return $null
}

function Safe-Value {
    param([scriptblock]$Script)
    try {
        return & $Script
    } catch {
        return $null
    }
}

if ($ProcessId -gt 0) {
    $proc = Get-Process -Id $ProcessId -ErrorAction Stop
} else {
    $proc = Get-Process -Name $ProcessName -ErrorAction Stop |
        Sort-Object -Property StartTime -Descending |
        Select-Object -First 1
}

$snapshotDir = Join-Path $OutputDir $Tag
New-Directory -Path $snapshotDir

if (-not ("SampWindowEnum" -as [type])) {
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class SampWindowEnum {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern int GetWindowTextLengthW(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    public static string GetWindowText(IntPtr hWnd) {
        int len = GetWindowTextLengthW(hWnd);
        StringBuilder sb = new StringBuilder(len + 1);
        GetWindowTextW(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }

    public static string GetClassName(IntPtr hWnd) {
        StringBuilder sb = new StringBuilder(256);
        GetClassNameW(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }
}
"@
}

$processRow = [pscustomobject]@{
    collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    process_id       = $proc.Id
    process_name     = $proc.ProcessName
    path             = $proc.Path
    start_time       = Safe-Value { $proc.StartTime.ToUniversalTime().ToString("o") }
    main_window      = Safe-Value { $proc.MainWindowTitle }
    handle_count     = Safe-Value { $proc.HandleCount }
    thread_count     = Safe-Value { $proc.Threads.Count }
    working_set_mb   = [math]::Round($proc.WorkingSet64 / 1MB, 2)
    private_memory_mb = [math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
    virtual_memory_mb = [math]::Round($proc.VirtualMemorySize64 / 1MB, 2)
}
$processRow | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $snapshotDir "process.json")

Export-Table -Rows (
    $proc.Threads | ForEach-Object {
        [pscustomobject]@{
            id               = $_.Id
            base_priority    = Safe-Value { $_.BasePriority }
            current_priority = Safe-Value { $_.CurrentPriority }
            thread_state     = Safe-Value { $_.ThreadState }
            wait_reason      = Safe-Value { $_.WaitReason }
            start_time       = Safe-Value { $_.StartTime.ToUniversalTime().ToString("o") }
        }
    }
) -Path (Join-Path $snapshotDir "threads.tsv")

try {
    $modules = Get-Process -Id $proc.Id -Module -ErrorAction Stop | ForEach-Object {
        [pscustomobject]@{
            module_name        = $_.ModuleName
            file_name          = $_.FileName
            base_address       = ("0x{0:X8}" -f $_.BaseAddress.ToInt64())
            module_memory_size = $_.ModuleMemorySize
            file_version       = Safe-Value { $_.FileVersionInfo.FileVersion }
            product_version    = Safe-Value { $_.FileVersionInfo.ProductVersion }
        }
    }
    Export-Table -Rows $modules -Path (Join-Path $snapshotDir "modules.tsv")
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "modules.error.txt")
}

$windows = New-Object System.Collections.Generic.List[object]
[SampWindowEnum]::EnumWindows({
    param($hWnd, $lParam)

    $windowProcessId = 0
    $windowThreadId = [SampWindowEnum]::GetWindowThreadProcessId($hWnd, [ref]$windowProcessId)
    if ($windowProcessId -eq $proc.Id) {
        $windows.Add([pscustomobject]@{
            handle_hex = ("0x{0:X8}" -f $hWnd.ToInt64())
            thread_id  = $windowThreadId
            process_id = $windowProcessId
            visible    = [SampWindowEnum]::IsWindowVisible($hWnd)
            class_name = [SampWindowEnum]::GetClassName($hWnd)
            title      = [SampWindowEnum]::GetWindowText($hWnd)
        }) | Out-Null
    }

    return $true
}, [IntPtr]::Zero) | Out-Null
Export-Table -Rows $windows.ToArray() -Path (Join-Path $snapshotDir "windows.tsv")

try {
    tasklist /v /fi "PID eq $($proc.Id)" | Out-File -LiteralPath (Join-Path $snapshotDir "tasklist_v.txt") -Encoding ascii
    tasklist /m /fi "PID eq $($proc.Id)" | Out-File -LiteralPath (Join-Path $snapshotDir "tasklist_m.txt") -Encoding ascii
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "tasklist.error.txt")
}

try {
    netstat -ano | Select-String -Pattern ("\s" + [regex]::Escape($proc.Id.ToString()) + "$") |
        Set-Content -LiteralPath (Join-Path $snapshotDir "netstat_pid.txt")
} catch {
    $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "netstat.error.txt")
}

$listDllsExe = Find-OptionalTool -Names @("listdlls64.exe", "listdlls.exe")
if ($listDllsExe) {
    try {
        & $listDllsExe -accepteula $proc.Id *> (Join-Path $snapshotDir "listdlls.txt")
    } catch {
        $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "listdlls.error.txt")
    }
}

$handleExe = Find-OptionalTool -Names @("handle64.exe", "handle.exe")
if ($handleExe) {
    try {
        & $handleExe -accepteula -p $proc.Id *> (Join-Path $snapshotDir "handles.txt")
    } catch {
        $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "handles.error.txt")
    }
}

if ($CaptureDump) {
    $procdumpExe = Find-OptionalTool -Names @("procdump64.exe", "procdump.exe")
    if ($procdumpExe) {
        try {
            & $procdumpExe -accepteula -ma $proc.Id (Join-Path $snapshotDir "gta_sa_full.dmp") *> (Join-Path $snapshotDir "procdump.txt")
        } catch {
            $_.Exception.ToString() | Set-Content -LiteralPath (Join-Path $snapshotDir "procdump.error.txt")
        }
    } else {
        "procdump.exe not found on PATH" | Set-Content -LiteralPath (Join-Path $snapshotDir "procdump.note.txt")
    }
}
