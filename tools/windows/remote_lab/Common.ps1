Set-StrictMode -Version Latest

function New-SampLabDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Get-SampLabConfig {
    param([string]$Root = "C:\samp-test")

    $path = Join-Path $Root "config.json"
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Remote-lab configuration not found: $path"
    }

    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Write-SampLabJson {
    param(
        [Parameter(Mandatory = $true)][object]$Value,
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$Depth = 8
    )

    $parent = Split-Path -Parent $Path
    New-SampLabDirectory -Path $parent
    $temporary = "$Path.tmp.$PID"
    $json = ConvertTo-Json -InputObject $Value -Depth $Depth
    [IO.File]::WriteAllText($temporary, $json, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function ConvertTo-SampLabName {
    param([Parameter(Mandatory = $true)][string]$Value)

    $clean = $Value -replace '[^A-Za-z0-9_.-]', '_'
    $clean = $clean.Trim(' ', '.', '_')
    if (-not $clean) {
        return "unnamed"
    }
    if ($clean.Length -gt 64) {
        return $clean.Substring(0, 64)
    }
    return $clean
}

function Get-SampLabHash {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-SampLabCurrentRun {
    param([string]$Root = "C:\samp-test")

    $path = Join-Path $Root "state\current_run.json"
    if (-not (Test-Path -LiteralPath $path)) {
        return $null
    }
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Get-SampLabFileState {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$CollectName = ""
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return [pscustomobject]@{
            path = $Path
            collect_name = if ($CollectName) { $CollectName } else { Split-Path -Leaf $Path }
            exists = $false
            length = 0
            modified_utc = $null
            sha256 = $null
        }
    }

    $item = Get-Item -LiteralPath $Path
    return [pscustomobject]@{
        path = $item.FullName
        collect_name = if ($CollectName) { $CollectName } else { $item.Name }
        exists = $true
        length = $item.Length
        modified_utc = $item.LastWriteTimeUtc.ToString("o")
        sha256 = Get-SampLabHash -Path $item.FullName
    }
}

function Get-SampLabModules {
    param([Parameter(Mandatory = $true)][int]$ProcessId)

    if (-not ("SampLabToolhelp" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class SampLabToolhelp {
    private const uint TH32CS_SNAPMODULE = 0x00000008;
    private const uint TH32CS_SNAPMODULE32 = 0x00000010;
    private static readonly IntPtr InvalidHandleValue = new IntPtr(-1);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct MODULEENTRY32 {
        public uint dwSize;
        public uint th32ModuleID;
        public uint th32ProcessID;
        public uint GlblcntUsage;
        public uint ProccntUsage;
        public IntPtr modBaseAddr;
        public uint modBaseSize;
        public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szExePath;
    }

    public sealed class ModuleRow {
        public string Name;
        public string Path;
        public long BaseAddress;
        public uint Size;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint processId);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool Module32FirstW(IntPtr snapshot, ref MODULEENTRY32 entry);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool Module32NextW(IntPtr snapshot, ref MODULEENTRY32 entry);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    public static ModuleRow[] Enumerate(int processId) {
        IntPtr snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
            unchecked((uint)processId));
        if (snapshot == InvalidHandleValue) {
            throw new Win32Exception(Marshal.GetLastWin32Error());
        }

        try {
            var rows = new List<ModuleRow>();
            var entry = new MODULEENTRY32();
            entry.dwSize = unchecked((uint)Marshal.SizeOf(typeof(MODULEENTRY32)));
            if (!Module32FirstW(snapshot, ref entry)) {
                int error = Marshal.GetLastWin32Error();
                if (error == 18) {
                    return rows.ToArray();
                }
                throw new Win32Exception(error);
            }

            do {
                rows.Add(new ModuleRow {
                    Name = entry.szModule,
                    Path = entry.szExePath,
                    BaseAddress = entry.modBaseAddr.ToInt64(),
                    Size = entry.modBaseSize
                });
                entry.dwSize = unchecked((uint)Marshal.SizeOf(typeof(MODULEENTRY32)));
            } while (Module32NextW(snapshot, ref entry));

            int finalError = Marshal.GetLastWin32Error();
            if (finalError != 0 && finalError != 18) {
                throw new Win32Exception(finalError);
            }
            return rows.ToArray();
        } finally {
            CloseHandle(snapshot);
        }
    }
}
"@
    }

    return @([SampLabToolhelp]::Enumerate($ProcessId) | ForEach-Object {
        $version = $null
        if ($_.Path -and (Test-Path -LiteralPath $_.Path)) {
            try { $version = (Get-Item -LiteralPath $_.Path).VersionInfo.FileVersion } catch { }
        }
        [pscustomobject]@{
            name = $_.Name
            path = $_.Path
            base_address = "0x{0:X8}" -f $_.BaseAddress
            size = $_.Size
            version = $version
        }
    })
}

function Get-SampLabProcesses {
    param([string[]]$Names = @("gta_sa", "samp"))

    $rows = foreach ($name in $Names) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            $processPath = try { $_.Path } catch { $null }
            $startTime = try { $_.StartTime.ToUniversalTime().ToString("o") } catch { $null }
            $responding = try { $_.Responding } catch { $null }
            $windowTitle = try { $_.MainWindowTitle } catch { $null }
            [pscustomobject]@{
                id = $_.Id
                name = $_.ProcessName
                path = $processPath
                start_time_utc = $startTime
                responding = $responding
                main_window_title = $windowTitle
            }
        }
    }
    return @($rows)
}

function Get-SampLabLogNames {
    return @(
        "samp_runtime.log",
        "samp_net_trace.log",
        "samp_hook_trace.log",
        "samp_probe.log",
        "reloop_control.log",
        "samp_re.log",
        "samp.log",
        "crashinfo.txt"
    )
}
