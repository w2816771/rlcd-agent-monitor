# Reads fan speeds and temperatures from the Lenovo GameZone WMI interface and
# leaves them in sensors.json beside this script, every few seconds, forever.
#
# This runs as its own scheduled task with the highest run level, because the
# GameZone class answers "Access denied" to a normal user. It is kept apart from
# the collector on purpose: the collector runs shell commands from config.yaml,
# which any process of this user can edit, so elevating the collector would turn
# that file into a path to administrator. This script reads WMI and writes one
# file, and executes nothing.
#
# Only the Lenovo interface is used. Windows' generic Win32_Fan reports no speed
# on this machine and MSAcpi_ThermalZoneTemperature is unsupported. Boards that
# have neither the class nor the methods get nulls, which the board shows as N/A
# rather than as a stopped fan.
param(
    [int]$IntervalSeconds = 5,
    [switch]$Once
)
$ErrorActionPreference = "Continue"
$target = Join-Path $PSScriptRoot "sensors.json"
$temp = "$target.tmp"

function Read-Method($instance, [string]$name) {
    try {
        $result = Invoke-CimMethod -InputObject $instance -MethodName $name -ErrorAction Stop
        if ($null -ne $result.Data) { return [int]$result.Data }
    } catch { }
    return $null
}

# A reading the firmware uses as "no value" in place of a real one. Lenovo
# reports 0 RPM for a stopped fan and some firmware reports 0 or 255 degrees
# for a sensor it does not have; the temperature guards catch the latter.
function Plausible-Temperature($value) {
    if ($null -eq $value) { return $null }
    if ($value -le 0 -or $value -ge 120) { return $null }
    return $value
}

do {
    $instance = $null
    try { $instance = Get-CimInstance -Namespace root\WMI -ClassName LENOVO_GAMEZONE_DATA -ErrorAction Stop | Select-Object -First 1 } catch { }
    if ($instance) {
        $count = Read-Method $instance "GetFanCount"
        if ($null -eq $count -or $count -lt 1) { $count = 2 }
        $fans = @()
        for ($i = 1; $i -le [Math]::Min($count, 4); $i++) { $fans += ,(Read-Method $instance "GetFan$($i)Speed") }
        $record = [ordered]@{
            written_at = [int][double]::Parse((Get-Date -UFormat %s))
            source     = "lenovo-gamezone"
            fan_rpm    = $fans
            cpu_temp_c = Plausible-Temperature (Read-Method $instance "GetCPUTemp")
            gpu_temp_c = Plausible-Temperature (Read-Method $instance "GetGPUTemp")
            thermal_mode = Read-Method $instance "GetThermalMode"
        }
    } else {
        $record = [ordered]@{
            written_at = [int][double]::Parse((Get-Date -UFormat %s))
            source     = "none"
            fan_rpm    = @()
            cpu_temp_c = $null
            gpu_temp_c = $null
            thermal_mode = $null
        }
    }
    # Temporary name then rename, so the collector never reads half a file.
    try {
        ($record | ConvertTo-Json -Compress) | Set-Content -Path $temp -Encoding utf8 -NoNewline
        Move-Item -Path $temp -Destination $target -Force
    } catch { }
    if ($Once) { break }
    Start-Sleep -Seconds $IntervalSeconds
} while ($true)
