# Starts the RLCD collector at logon on Windows, the counterpart of service.sh.
# Build first with: cargo build --release
param(
    [ValidateSet("install", "uninstall", "restart", "status", "logs")]
    [string]$Action = "status"
)
$ErrorActionPreference = "Stop"
$task = "RLCD Monitor Host"
$exe = Join-Path $PSScriptRoot "target\release\rlcd-host.exe"
$log = Join-Path $PSScriptRoot "rlcd-host.log"
$config = Join-Path $PSScriptRoot "config.yaml"
$template = Join-Path $PSScriptRoot "config.example.yaml"

switch ($Action) {
    "install" {
        if (-not (Test-Path $exe)) { throw "Missing $exe. Run cargo build --release first." }
        # A fresh checkout has no config.yaml, since it is not tracked. Seed it
        # from the template rather than silently running on built in defaults.
        if (-not (Test-Path $config)) {
            Copy-Item $template $config
            "Created $config from the template. Edit it for your location."
        }
        # The collector reads config.yaml from its working directory.
        # The Windows GUI executable writes its own log without a console.
        $run = New-ScheduledTaskAction -Execute $exe `
            -WorkingDirectory $PSScriptRoot
        $logon = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
        # A second trigger that fires every minute forever. The restart on
        # failure setting below only covers a failing exit, and the collector has
        # been seen ending on a console close signal instead, which leaves the
        # task simply finished and nothing to bring it back. With IgnoreNew this
        # trigger is a no-op while the collector is up and a revival when it is
        # not, so the worst case is a minute of a stale clock on the board.
        $heartbeat = New-ScheduledTaskTrigger -Once -At (Get-Date) `
            -RepetitionInterval (New-TimeSpan -Minutes 1)
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
            -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero) `
            -MultipleInstances IgnoreNew
        Register-ScheduledTask -TaskName $task -Action $run -Trigger $logon, $heartbeat -Settings $settings -Force | Out-Null
        Start-ScheduledTask -TaskName $task
        "RLCD host task installed and started."
    }
    "uninstall" {
        Stop-ScheduledTask -TaskName $task -ErrorAction SilentlyContinue
        Get-Process rlcd-host -ErrorAction SilentlyContinue | Stop-Process -Force
        Unregister-ScheduledTask -TaskName $task -Confirm:$false
        "RLCD host task removed."
    }
    "restart" {
        Stop-ScheduledTask -TaskName $task -ErrorAction SilentlyContinue
        Get-Process rlcd-host -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-ScheduledTask -TaskName $task
    }
    "status" {
        Get-ScheduledTask -TaskName $task | Get-ScheduledTaskInfo
        $running = Get-Process rlcd-host -ErrorAction SilentlyContinue
        if ($running) { "Collector running, pid $($running.Id)." }
        else { "Collector is not running." }
    }
    "logs" {
        if (Test-Path $log) { Get-Content $log -Tail 40 -Wait }
        else { "No log yet at $log. Install and start the task first." }
    }
}
