param(
    [int]$Port = 18737
)

$ErrorActionPreference = "Stop"
$script:ReloopClient = [System.Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
$script:ReloopStream = $script:ReloopClient.GetStream()
$script:ReloopReader = [System.IO.StreamReader]::new(
    $script:ReloopStream,
    [System.Text.Encoding]::UTF8,
    $false,
    4096,
    $true
)
$script:ReloopWriter = [System.IO.StreamWriter]::new(
    $script:ReloopStream,
    [System.Text.UTF8Encoding]::new($false),
    4096,
    $true
)
$script:ReloopWriter.AutoFlush = $true

function Send-ReloopControl {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [hashtable]$Fields = @{}
    )

    $payload = [ordered]@{
        token = "reloop-local-v1"
        cmd = $Command
    }
    foreach ($entry in $Fields.GetEnumerator()) {
        $payload[$entry.Key] = $entry.Value
    }

    $script:ReloopWriter.WriteLine(($payload | ConvertTo-Json -Compress))
    $line = $script:ReloopReader.ReadLine()
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "reloop_control closed the connection"
    }
    $response = $line | ConvertFrom-Json
    if (-not $response.ok) {
        throw "reloop_control rejected '$Command': $line"
    }
    $response
}

function Set-ReloopFocus {
    Send-ReloopControl -Command "focus"
}

function Send-ReloopEnter {
    Send-ReloopControl -Command "window_key" -Fields @{ vk = 13 }
}

function Send-ReloopClick {
    param(
        [Parameter(Mandatory = $true)]
        [int]$X,
        [Parameter(Mandatory = $true)]
        [int]$Y
    )

    Send-ReloopControl -Command "mouse" -Fields @{
        action = "click"
        x = $X
        y = $Y
    }
}

function Send-ReloopChatCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text
    )

    Set-ReloopFocus | Out-Null
    Send-ReloopControl -Command "char" -Fields @{ code = [int][char]'t' } | Out-Null
    Start-Sleep -Milliseconds 150
    foreach ($character in $Text.ToCharArray()) {
        Send-ReloopControl -Command "char" -Fields @{ code = [int]$character } | Out-Null
    }
    Send-ReloopEnter
}

function Get-ReloopState {
    Send-ReloopControl -Command "state"
}

function Close-ReloopControl {
    if ($null -ne $script:ReloopWriter) {
        $script:ReloopWriter.Dispose()
        $script:ReloopWriter = $null
    }
    if ($null -ne $script:ReloopReader) {
        $script:ReloopReader.Dispose()
        $script:ReloopReader = $null
    }
    if ($null -ne $script:ReloopStream) {
        $script:ReloopStream.Dispose()
        $script:ReloopStream = $null
    }
    if ($null -ne $script:ReloopClient) {
        $script:ReloopClient.Dispose()
        $script:ReloopClient = $null
    }
}

Send-ReloopControl -Command "ping"
Write-Host "RELOOP_READY functions=Set-ReloopFocus,Send-ReloopEnter,Send-ReloopClick,Send-ReloopChatCommand,Get-ReloopState,Close-ReloopControl"
