param([string]$Root = "C:\samp-test")

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Common.ps1")

$pendingDir = Join-Path $Root "commands\pending"
$processingDir = Join-Path $Root "commands\processing"
$doneDir = Join-Path $Root "commands\done"
$failedDir = Join-Path $Root "commands\failed"
$stateDir = Join-Path $Root "state"
$agentLog = Join-Path $stateDir "agent.log"
$heartbeatPath = Join-Path $stateDir "heartbeat.json"
$mutex = [Threading.Mutex]::new($false, "Local\SampTestInteractiveAgent")

if (-not $mutex.WaitOne(0)) {
    exit 0
}

function Write-AgentLog {
    param([string]$Message)
    $line = "{0} {1}" -f (Get-Date).ToUniversalTime().ToString("o"), $Message
    Add-Content -LiteralPath $agentLog -Value $line -Encoding UTF8
}

function Invoke-AgentCommand {
    param([Parameter(Mandatory = $true)][object]$Command)

    $action = ([string]$Command.action).ToLowerInvariant()
    switch ($action) {
        "ping" {
            return [pscustomobject]@{
                action = $action
                user = [Environment]::UserDomainName + "\" + [Environment]::UserName
                session_id = [Diagnostics.Process]::GetCurrentProcess().SessionId
                processes = @(Get-SampLabProcesses)
            }
        }
        "screenshot" {
            $label = if ($Command.PSObject.Properties.Name -contains "label") { [string]$Command.label } else { "manual" }
            $path = & (Join-Path $PSScriptRoot "Capture-Screenshot.ps1") -Root $Root -Label $label
            return [pscustomobject]@{ action = $action; screenshot = $path }
        }
        "input" {
            $mode = if ($Command.PSObject.Properties.Name -contains "input_mode") { [string]$Command.input_mode } else { "key" }
            $key = if ($Command.PSObject.Properties.Name -contains "input_key") { [string]$Command.input_key } else { "ENTER" }
            $x = if ($Command.PSObject.Properties.Name -contains "input_x") { [int]$Command.input_x } else { 0 }
            $y = if ($Command.PSObject.Properties.Name -contains "input_y") { [int]$Command.input_y } else { 0 }
            $label = if ($Command.PSObject.Properties.Name -contains "label") { [string]$Command.label } else { "input" }
            return & (Join-Path $PSScriptRoot "Send-SampTestInput.ps1") `
                -Root $Root -Mode $mode -Key $key -X $x -Y $y -Label $label
        }
        "start" {
            $scenario = if ($Command.PSObject.Properties.Name -contains "scenario") { [string]$Command.scenario } else { "manual" }
            $target = if ($Command.PSObject.Properties.Name -contains "target") { [string]$Command.target } else { "samp" }
            $serverHost = if ($Command.PSObject.Properties.Name -contains "server_host") { [string]$Command.server_host } else { "" }
            $serverPort = if ($Command.PSObject.Properties.Name -contains "server_port") { [int]$Command.server_port } else { 0 }
            $nickname = if ($Command.PSObject.Properties.Name -contains "nickname") { [string]$Command.nickname } else { "WinDebug" }
            $favoriteIndex = if ($Command.PSObject.Properties.Name -contains "favorite_index") { [int]$Command.favorite_index } else { -1 }
            return & (Join-Path $PSScriptRoot "Start-SampTest.ps1") `
                -Root $Root `
                -Scenario $scenario `
                -Target $target `
                -ServerHost $serverHost `
                -ServerPort $serverPort `
                -Nickname $nickname `
                -FavoriteIndex $favoriteIndex
        }
        "collect" {
            return & (Join-Path $PSScriptRoot "Collect-SampRun.ps1") -Root $Root
        }
        "stop" {
            return & (Join-Path $PSScriptRoot "Stop-SampTest.ps1") -Root $Root
        }
        "logcheck" {
            $config = Get-SampLabConfig -Root $Root
            $path = Join-Path ([string]$config.game_dir) "samp_re.log"
            $marker = "{0} lab_log_check user={1}\r\n" -f (
                (Get-Date).ToUniversalTime().ToString("o")
            ), ([Environment]::UserDomainName + "\" + [Environment]::UserName)
            [IO.File]::AppendAllText($path, $marker, [Text.Encoding]::ASCII)
            $item = Get-Item -LiteralPath $path
            return [pscustomobject]@{
                action = $action
                path = $path
                length = $item.Length
                modified_utc = $item.LastWriteTimeUtc.ToString("o")
            }
        }
        default {
            throw "Unsupported agent action: $action"
        }
    }
}

try {
    foreach ($path in @($pendingDir, $processingDir, $doneDir, $failedDir, $stateDir)) {
        New-SampLabDirectory -Path $path
    }
    Write-AgentLog -Message "agent_started pid=$PID session=$([Diagnostics.Process]::GetCurrentProcess().SessionId)"

    while ($true) {
        Write-SampLabJson -Value ([ordered]@{
            updated_utc = (Get-Date).ToUniversalTime().ToString("o")
            pid = $PID
            user = [Environment]::UserDomainName + "\" + [Environment]::UserName
            session_id = [Diagnostics.Process]::GetCurrentProcess().SessionId
        }) -Path $heartbeatPath

        $pending = Get-ChildItem -LiteralPath $pendingDir -Filter "*.json" -File |
            Sort-Object CreationTimeUtc, Name |
            Select-Object -First 1

        if ($pending) {
            $processingPath = Join-Path $processingDir $pending.Name
            try {
                Move-Item -LiteralPath $pending.FullName -Destination $processingPath -ErrorAction Stop
                $command = Get-Content -LiteralPath $processingPath -Raw | ConvertFrom-Json
                $started = (Get-Date).ToUniversalTime()
                $payload = Invoke-AgentCommand -Command $command
                $result = [ordered]@{
                    id = [string]$command.id
                    action = [string]$command.action
                    status = "ok"
                    started_utc = $started.ToString("o")
                    finished_utc = (Get-Date).ToUniversalTime().ToString("o")
                    result = $payload
                }
                Write-SampLabJson -Value $result -Path (Join-Path $doneDir $pending.Name)
                Remove-Item -LiteralPath $processingPath -Force
                Write-AgentLog -Message "command_ok id=$($command.id) action=$($command.action)"
            } catch {
                $failure = [ordered]@{
                    status = "failed"
                    finished_utc = (Get-Date).ToUniversalTime().ToString("o")
                    error = $_.Exception.ToString()
                }
                Write-SampLabJson -Value $failure -Path (Join-Path $failedDir $pending.Name)
                Remove-Item -LiteralPath $processingPath -Force -ErrorAction SilentlyContinue
                Write-AgentLog -Message "command_failed file=$($pending.Name) error=$($_.Exception.Message)"
            }
        }

        Start-Sleep -Seconds 2
    }
} finally {
    Write-AgentLog -Message "agent_stopped pid=$PID"
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
