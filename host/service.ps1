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

switch ($Action) {
    "install" {
        if (-not (Test-Path $exe)) { throw "Missing $exe. Run cargo build --release first." }
        # The collector reads config.yaml from its working directory.
        # Run through cmd so the output lands in a file. A logon task has no
        # console, so without this there is no telling a working collector
        # from a crashed one, which the macOS service already avoids.
        $run = New-ScheduledTaskAction -Execute "cmd.exe" `
            -Argument "/c `"`"$exe`" >> `"$log`" 2>&1`"" `
            -WorkingDirectory $PSScriptRoot
        $logon = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
            -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero)
        Register-ScheduledTask -TaskName $task -Action $run -Trigger $logon -Settings $settings -Force | Out-Null
        Start-ScheduledTask -TaskName $task
        "RLCD host task installed and started."
    }
    "uninstall" {
        Stop-ScheduledTask -TaskName $task -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $task -Confirm:$false
        "RLCD host task removed."
    }
    "restart" {
        Stop-ScheduledTask -TaskName $task -ErrorAction SilentlyContinue
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
