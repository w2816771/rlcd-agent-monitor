# Starts the RLCD collector at logon on Windows, the counterpart of service.sh.
# Build first with: cargo build --release
param(
    [ValidateSet("install", "uninstall", "restart", "status", "logs", "install-sensors", "uninstall-sensors")]
    [string]$Action = "status"
)
$ErrorActionPreference = "Stop"
$task = "RLCD Monitor Host"
$exe = Join-Path $PSScriptRoot "target\release\rlcd-host.exe"
$log = Join-Path $PSScriptRoot "rlcd-host.log"
$config = Join-Path $PSScriptRoot "config.yaml"
$template = Join-Path $PSScriptRoot "config.example.yaml"
# Fan speeds and temperatures come from a separate task that runs elevated,
# because the Lenovo WMI class refuses a normal user. See sensors.ps1 for why
# the collector itself is never elevated.
$sensorTask = "RLCD Sensors"
$sensorScript = Join-Path $PSScriptRoot "sensors.ps1"
$sensorFile = Join-Path $PSScriptRoot "sensors.json"
$elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

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
        if (Get-ScheduledTask -TaskName $sensorTask -ErrorAction SilentlyContinue) {
            if (Test-Path $sensorFile) {
                $age = [int]((Get-Date) - (Get-Item $sensorFile).LastWriteTime).TotalSeconds
                "Sensor task installed; sensors.json written $age s ago: " + (Get-Content $sensorFile -Raw)
            } else { "Sensor task installed but sensors.json has not been written yet." }
        } else { "Sensor task not installed; fan speeds and CPU temperature read N/A. Run install-sensors from an administrator PowerShell." }
    }
    "install-sensors" {
        # Registering a task with the highest run level needs an elevated shell;
        # a normal one gets Access denied from the scheduler, not from us.
        if (-not $elevated) { throw "install-sensors must run from an administrator PowerShell (right click, Run as administrator)." }
        $run = New-ScheduledTaskAction -Execute "powershell.exe" `
            -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$sensorScript`"" `
            -WorkingDirectory $PSScriptRoot
        $logon = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
        $heartbeat = New-ScheduledTaskTrigger -Once -At (Get-Date) -RepetitionInterval (New-TimeSpan -Minutes 1)
        $principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -RunLevel Highest -LogonType Interactive
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
            -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
        Register-ScheduledTask -TaskName $sensorTask -Action $run -Trigger $logon, $heartbeat -Principal $principal -Settings $settings -Force | Out-Null
        Start-ScheduledTask -TaskName $sensorTask
        Start-Sleep -Seconds 4
        if (Test-Path $sensorFile) { "Sensor task installed. First reading: " + (Get-Content $sensorFile -Raw) }
        else { "Sensor task installed and started; sensors.json should appear within a few seconds." }
    }
    "uninstall-sensors" {
        if (-not $elevated) { throw "uninstall-sensors must run from an administrator PowerShell." }
        Stop-ScheduledTask -TaskName $sensorTask -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $sensorTask -Confirm:$false
        Remove-Item $sensorFile -ErrorAction SilentlyContinue
        "Sensor task removed."
    }
    "logs" {
        if (Test-Path $log) { Get-Content $log -Tail 40 -Wait }
        else { "No log yet at $log. Install and start the task first." }
    }
}
