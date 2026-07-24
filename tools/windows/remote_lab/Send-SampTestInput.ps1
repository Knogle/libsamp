param(
    [string]$Root = "C:\samp-test",
    [ValidateSet("key", "click")][string]$Mode = "key",
    [ValidateSet("ENTER", "ESCAPE", "SPACE", "UP", "DOWN", "LEFT", "RIGHT", "MODE", "CLASS", "KILL", "QUIT", "SFA", "LVA", "AA", "ACTORS", "ACTORSOFF", "RPC175EDGE", "RPC175EDGEOFF", "RPC175RAW", "RPC176RAW", "RPC178EDGE", "RPC178EDGEOFF")][string]$Key = "ENTER",
    [int]$X = 0,
    [int]$Y = 0,
    [string]$Label = "input"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$gta = @(Get-Process -Name "gta_sa" -ErrorAction SilentlyContinue)
if ($gta.Count -ne 1 -or $gta[0].MainWindowHandle -eq 0) {
    throw "Exactly one interactive gta_sa.exe window is required."
}
$process = $gta[0]
if (-not ("SampTestWindowInput" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class SampTestWindowInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
}
"@
}
$windowHandle = [SampTestWindowInput]::GetForegroundWindow()
[uint32]$foregroundProcessId = 0
[SampTestWindowInput]::GetWindowThreadProcessId($windowHandle, [ref]$foregroundProcessId) | Out-Null
if ($foregroundProcessId -ne $process.Id) {
    $shell = New-Object -ComObject WScript.Shell
    if (-not $shell.AppActivate($process.Id)) {
        throw "Could not activate the GTA test window."
    }
    Start-Sleep -Milliseconds 200
    $windowHandle = [SampTestWindowInput]::GetForegroundWindow()
    $foregroundProcessId = 0
    [SampTestWindowInput]::GetWindowThreadProcessId($windowHandle, [ref]$foregroundProcessId) | Out-Null
    if ($foregroundProcessId -ne $process.Id) {
        throw "GTA did not become the foreground window."
    }
}

if ($Mode -eq "key") {
    $sendKey = switch ($Key) {
        "ENTER" { "{ENTER}" }
        "ESCAPE" { "{ESC}" }
        "SPACE" { " " }
        "UP" { "{UP}" }
        "DOWN" { "{DOWN}" }
        "LEFT" { "{LEFT}" }
        "RIGHT" { "{RIGHT}" }
        # Fixed UFW test action; intentionally not a general text-input mode.
        "MODE" { "{F6}/modo{ENTER}" }
        # Split F4 and /kill into separate observable actions.  A combined
        # SendKeys burst made it impossible to tell whether the legacy class
        # latch, chat activation, or the spawn request caused a sync failure.
        "CLASS" { "{F4}" }
        "KILL" { "{F6}/kill{ENTER}" }
        "QUIT" { "{F6}/q{ENTER}" }
        # Fixed teleport actions used to stress the large object/model RPC
        # bursts on the SuperFreeroam compatibility route.
        "SFA" { "{F6}/sfa{ENTER}" }
        "LVA" { "{F6}/lva{ENTER}" }
        "AA" { "{F6}/aa{ENTER}" }
        # Fixed local bare-server Actor parity actions. Keep arbitrary command
        # injection out of the remote queue while allowing a deterministic
        # original/replacement Actor lifecycle run.
        "ACTORS" { "{F6}/rpcactors{ENTER}" }
        "ACTORSOFF" { "{F6}/rpcactorsoff{ENTER}" }
        "RPC175EDGE" { "{F6}/rpc175edge{ENTER}" }
        "RPC175EDGEOFF" { "{F6}/rpc175edgeoff{ENTER}" }
        "RPC175RAW" { "{F6}/rpc175raw{ENTER}" }
        "RPC176RAW" { "{F6}/rpc176raw{ENTER}" }
        "RPC178EDGE" { "{F6}/rpc178edge{ENTER}" }
        "RPC178EDGEOFF" { "{F6}/rpc178edgeoff{ENTER}" }
    }
    $shell = New-Object -ComObject WScript.Shell
    $shell.SendKeys($sendKey)
    if ($Key -eq "MODE") {
        # UFW answers /modo with a list dialog.  Confirm its preselected
        # FreeRoam row promptly so the test reaches the TextDraw mode selector
        # before the server's dialog/sync timeout closes this connection.
        Start-Sleep -Milliseconds 1000
        $shell.SendKeys("{ENTER}")
    }
} else {
    $rect = New-Object SampTestWindowInput+RECT
    if (-not [SampTestWindowInput]::GetWindowRect($windowHandle, [ref]$rect)) {
        throw "Could not read the GTA window bounds."
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($X -lt 0 -or $Y -lt 0 -or $X -ge $width -or $Y -ge $height) {
        throw "Relative click coordinate is outside the GTA window: $X,$Y in $($width)x$height."
    }
    [SampTestWindowInput]::SetCursorPos($rect.Left + $X, $rect.Top + $Y) | Out-Null
    [SampTestWindowInput]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    # GTA/SA-MP polls the DirectInput mouse state once per frame.  Back-to-back
    # mouse_event calls can collapse into a transition that is visible as hover
    # movement but never sampled as a held button on native Windows.
    Start-Sleep -Milliseconds 100
    [SampTestWindowInput]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}

Start-Sleep -Milliseconds 750
$screenshot = & (Join-Path $PSScriptRoot "Capture-Screenshot.ps1") -Root $Root -Label $Label
[pscustomobject]@{
    action = "input"
    mode = $Mode
    key = if ($Mode -eq "key") { $Key } else { $null }
    relative_x = if ($Mode -eq "click") { $X } else { $null }
    relative_y = if ($Mode -eq "click") { $Y } else { $null }
    process_id = $process.Id
    screenshot = $screenshot
}
