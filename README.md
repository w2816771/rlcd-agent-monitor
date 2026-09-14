# RLCD Agent Monitor

A desk monitor built on the Waveshare ESP32-S3-RLCD-4.2, a 400 by 300
reflective monochrome display board. The board shows a computer's system load,
network, Codex and Claude usage, top processes, news, weather, room conditions,
and sound level. A small Rust program on each computer, the host, sends that
data over Bluetooth Low Energy.

## Features

- Seven pages: overview, host system, agent usage, top processes, news, sound,
  and devices.
- Up to three computers connected at once, such as a Mac and a PC, with the
  shown one switchable from the board and remembered across reboots.
- Two-way Bluetooth. Board buttons can run commands on the shown computer.
- Onboard temperature and humidity, a calibrated sound level meter with
  history, and battery level.
- Power save mode on battery with automatic light sleep, and full function mode
  while plugged into a computer's USB port.

## Buttons

| Button | Press | Hold |
|---|---|---|
| KEY | next page | switch source; on the news page, next headlines |
| BOOT | previous page | back to the overview |
| PWR | power on | power off |

## Repository layout

| Path | Contents |
|---|---|
| `main/` | firmware application: pages, Bluetooth, sensors, sound meter |
| `components/rlcd/` | display driver, derived from Waveshare's Apache-2.0 example |
| `host/` | Rust host for macOS and Windows, its config, and startup scripts |
| `docs/host-api.md` | full host contract: Bluetooth protocol, snapshot schema, requirements |
| `CLAUDE.md` | architecture notes and engineering rules for this codebase |

## Firmware

Requires ESP-IDF 5.5.1. The wrapper script uses `~/esp/esp-idf-v5.5.1`, or the
checkout named by `IDF_PATH`. The first build downloads LVGL through the ESP-IDF
Component Manager.

```bash
./idf.sh build
```

The board uses its native USB port. Flash with the USB reset method, because the
default reset method cannot put this chip into download mode while the firmware
is running:

```bash
cd build && esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX --before usb_reset --after hard_reset write_flash @flash_args
```

If the port does not appear at all, hold BOOT, click PWR, keep holding BOOT for a
second, then flash.

Crash output goes only to the unconnected UART0, so the firmware saves a core
dump to flash instead. Read it back with the ELF that matches the flashed build:

```bash
./idf.sh -p /dev/cu.usbmodemXXXX coredump-info
```

## Host

On macOS, from `host/`:

```bash
cargo build --release
```

```bash
./target/release/rlcd-host
```

To start it at login, run `./service.sh install`, then use `./service.sh logs`
to follow it. macOS asks for Bluetooth permission on first run.

On Windows, follow `docs/host-api.md`, section 11, and install the logon task
with `service.ps1`.

Every computer must set a different `device_name` in `host/config.yaml`, such as
`MAC` and `PC`. The config is re-read continuously, so edits take effect without
a restart. It also sets weather location, news topic, update intervals, button
commands, and the microphone calibration `noise_offset_db`.

## Power behavior

- Plugged into a computer, the board runs in full function mode, polls often,
  and keeps USB alive for flashing. The battery corner reads `USB`, because the
  charger holds the cell voltage near full.
- On battery, it enters power save mode with automatic light sleep and slower
  polling.
- A computer that is not being shown sends every five minutes instead of every
  minute, and is woken immediately when you switch to it.

## Status and known limitations

- The Windows host compiles, but has not yet been run on Windows.
- Switching between two real computers has not been tested with two machines.
- The Bluetooth characteristics require no pairing.
- The card slot, speaker, and real-time clock chip on the board are not used yet.
