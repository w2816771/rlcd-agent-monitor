# Waits for the board's USB port to appear, then flashes build/ onto it.
#
#   powershell -ExecutionPolicy Bypass -File .\flash-when-ready.ps1
#
# Put the board in download mode while this runs: unplug USB, hold BOOT,
# plug USB into the computer, keep BOOT held five seconds, release. The ROM
# bootloader enumerates as a new COM port; the script flashes it as soon as
# it shows up. Ports present at start are ignored, so the Bluetooth virtual
# ports do not confuse it.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root "build"
if (-not (Test-Path (Join-Path $build "flash_args"))) {
    Write-Host "No build\flash_args. Build first (idf.py build)." -ForegroundColor Red
    exit 1
}

$before = [System.IO.Ports.SerialPort]::GetPortNames()
Write-Host "Ignoring existing ports: $($before -join ' ')"
Write-Host "Waiting for the board. Unplug USB, hold BOOT, plug in, hold 5 s, release." -ForegroundColor Cyan

$port = $null
while (-not $port) {
    Start-Sleep -Seconds 1
    $now = [System.IO.Ports.SerialPort]::GetPortNames()
    $new = $now | Where-Object { $before -notcontains $_ }
    if ($new) { $port = $new | Select-Object -First 1 }
}
Write-Host "Board on $port, flashing in 2 s" -ForegroundColor Green
Start-Sleep -Seconds 2

# The chip is already in download mode, so no reset dance: toggling DTR/RTS
# on this board can wedge it. Hard reset afterwards boots the new firmware.
$cmd = "set ""IDF_TOOLS_PATH=D:\esp\tools"" && call D:\esp\esp-idf-v5.5.1\export.bat >nul 2>&1 && cd /d ""$build"" && python -m esptool --chip esp32s3 --port $port --before no_reset --after hard_reset write_flash @flash_args"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "Flash failed (exit $LASTEXITCODE). Re-enter download mode and run again." -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "Done. The board reboots into the new firmware." -ForegroundColor Green
