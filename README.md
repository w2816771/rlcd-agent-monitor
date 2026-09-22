# RLCD Agent Monitor

A desk monitor built on the Waveshare ESP32-S3-RLCD-4.2, a 400 by 300 reflective
monochrome display. The board shows a computer's load, network, Codex and Claude
token usage, top processes, news, weather, room conditions and sound level. A
small Rust program on each computer, the host, sends that data over Bluetooth Low
Energy.

Reflective means no backlight: it is readable in daylight, holds its image
without power, and is meant to sit on a desk being glanced at rather than
watched.

## What you need

| | |
|---|---|
| Board | **Waveshare ESP32-S3-RLCD-4.2** (ESP32-S3 with a 400x300 ST7305 reflective LCD, SHTC3 sensor, ES7210 microphone, ES8311 codec, battery holder). No other board will work without porting the display driver and the pin map in `main/main.cpp`. |
| Optional | A small speaker on the MX1.25 header, for the reminder sound. Without one, reminders are silent but still show on screen. |
| A computer | macOS or Windows 10 1703+/11, with Bluetooth Low Energy. Up to three can feed one board at once. |
| To build the firmware | ESP-IDF 5.5.1 |
| To build the host | Rust 1.95 or newer. On Windows also the Visual Studio Build Tools C++ workload. |
| Optional | Node.js, for Claude's token counts, which come from `ccusage`. Without it they read zero; the limit percentages do not need it |

Everything else — weather and news — uses keyless public endpoints, so there is
nothing to sign up for.

## Pages

Nine, cycled with the buttons: overview (clock, weather, room temperature,
humidity, sound history), CPU (a bar per logical processor, the total, CPU and
GPU temperatures, GPU load, memory and power, fan speeds), trends (RAM, network,
GPU and temperature curves over the last 60 snapshots), top processes (ordered
by CPU, RAM or a disk-weighted impact score; KEY held cycles), agent usage
(Codex and Claude limits and tokens), news, todo (from the config file, with
time left on anything that has a deadline), devices, and a debug log showing
the board log lines.

Reminders come from the same config file: a time of day, a repeating interval,
or a countdown. When one is due the board draws a box over whatever page is up
and plays a short eight bit jingle through the speaker header, repeating until
any button is pressed. That press only silences it; it never turns a page.

| Button | Press | Hold |
|---|---|---|
| KEY | next page | switch source; on processes reorders the rows, on news and debug pages through them |
| BOOT | previous page | back to the overview |
| PWR | power on | power off |

## Quick start

**1. Flash the firmware.** With ESP-IDF 5.5.1 installed:

```bash
./idf.sh build
```

The board uses its native USB port, and needs the USB reset method because the
default one cannot enter download mode while the firmware is running:

```bash
cd build && esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX --before usb_reset --after hard_reset write_flash @flash_args
```

On Windows the port looks like `COM5`. If it does not appear at all, hold BOOT,
click PWR, keep holding BOOT for a second, then flash.

**2. Configure the host.** `config.yaml` is not tracked by git, so copy the
template and edit your own copy:

```bash
cp host/config.example.yaml host/config.yaml
```

Set `weather.latitude`, `weather.longitude` and `weather.region` for where you
are, and give every computer a different `device_name`, such as `MAC` and `PC`.
Two hosts sharing a name overwrite each other on the board. The file is re-read
every loop, so edits take effect without a restart.

**3. Build and run the host.**

```bash
cd host && cargo build --release
```

```bash
./target/release/rlcd-host
```

To start it at login: `./service.sh install` on macOS, then `./service.sh logs`
to follow it. On Windows, `service.ps1 install` registers a logon task; it also
takes `status`, `logs`, `restart` and `uninstall`. macOS asks for Bluetooth
permission on first run.

On Windows the host also shows a notification area icon. Windows puts new icons
in the hidden overflow, so drag it onto the taskbar if you want it visible. Its
tooltip shows the last successful send, and its menu opens the log, the config
and the folder, or stops the collector.

The Windows tray also has **打开设置** for the embedded Chinese settings page.
It edits refresh intervals, weather/news, reminders, todos and button commands.
The host serves it on `127.0.0.1:17832` (an available port is chosen if busy);
there is no Node service or separate HTML file to install. Open it from the tray
or the generated `host/settings.url` shortcut, which carries a per-run access
token. After restarting the host, reopen the page through either entry point.
Saving validates the configuration, rejects stale edits, backs up the previous
file as `config.yaml.bak` and atomically replaces it. Existing unknown keys are
retained; YAML comments/formatting are kept in the backup. The collector applies
changes on its next collection cycle. The release executable runs without a
console, writes its own log and is launched directly by the scheduled task.

### Fan speeds and CPU temperature on Windows

GPU figures come from `nvidia-smi` and need nothing. Fan speeds and the CPU
temperature come from the Lenovo GameZone WMI interface, which refuses a normal
user, so a small read-only script runs as its own elevated scheduled task and
leaves the readings in `host\sensors.json`. The collector stays unelevated on
purpose: it runs the button commands from `config.yaml`, which any process of
your user can edit. From an **administrator** PowerShell, in `host\`:

```powershell
powershell -ExecutionPolicy Bypass -File .\service.ps1 install-sensors
```

`status` reports the sensor file and its age; `uninstall-sensors` removes the
task. Without it the board shows FAN N/A and TEMP N/A, never 0. Only Lenovo
laptops with the GameZone interface are covered; on anything else the script
writes nulls and the board shows N/A.

### Claude limit figures

The board shows Claude's 5 hour and 7 day limits as percentages, the same ones
the `/usage` screen shows. Claude Code hands them to its status line command,
so the status line script has to leave them where the host can find them. Add
this near the top of your script, after the line that reads stdin into `input`:

```sh
usage_file="$HOME/.claude/rlcd-usage.json"
if command -v jq >/dev/null 2>&1; then
  printf '%s' "$input" | jq -c '{rate_limits: (.rate_limits // null), context_window: (.context_window // null)}' > "$usage_file.tmp" 2>/dev/null
else
  printf '%s' "$input" > "$usage_file.tmp" 2>/dev/null
fi
[ -s "$usage_file.tmp" ] && mv -f "$usage_file.tmp" "$usage_file" 2>/dev/null
```

If you have no status line yet, a script holding `input=$(cat)` and those lines
is enough; point `statusLine` in `~/.claude/settings.json` at it. The figures
exist for Pro and Max subscriptions and refresh while Claude Code is running.
Without the file the host falls back to token counts, shown as `5H 1.4M`.

### If the host stops

The board keeps showing the last snapshot with its age in the header, so a
stopped host looks like a stale clock.

On Windows the task carries a trigger that fires every minute and starts the
collector if it is not already running, so it normally comes back by itself
within about a minute. The commands below are for when you do not want to wait, or when it
is not coming back at all.

Run them from the `host` folder of **this** checkout. If a second copy of the
project is lying around, `install` there will fail with `Missing ...
rlcd-host.exe`, which means you are in the wrong folder rather than that
anything is broken. On Windows, in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\service.ps1 status
```

```powershell
powershell -ExecutionPolicy Bypass -File .\service.ps1 restart
```

On macOS, from `host/`:

```bash
./service.sh status
```

```bash
./service.sh restart
```

`restart` stops any running collector before starting a new one, so it is safe
to run even when the host is already up. If it still will not stay running,
read the end of the log (`service.ps1 logs`, or `host\rlcd-host.log`; on macOS
`./service.sh logs`) for the reason. A log that ends in `^C` means the collector
was sent a console close signal rather than crashing, which the Windows task's
minute trigger recovers from on its own; `restart` just skips the wait.

If you used **Quit collector** from the tray icon, the task will not start again
by itself until the next logon. Use `restart` to bring it back sooner.

## Repository layout

| Path | Contents |
|---|---|
| `main/` | firmware: pages, Bluetooth, sensors, sound meter, debug log |
| `components/rlcd/` | display driver, derived from Waveshare's example |
| `host/` | Rust host, its config template and startup scripts |
| `docs/host-api.md` | the full host contract: BLE protocol, snapshot schema, how every field is derived |
| `CLAUDE.md` | architecture notes and the engineering rules for this codebase |

`docs/host-api.md` is the file to read before changing either side of the
protocol, or before writing a host in another language. It is a specification,
not a tutorial.

## Power behaviour

Plugged into a computer, the board runs in full function mode and polls often.
On battery it enters power save with automatic light sleep and slower polling.
A computer the board is not currently showing sends every five minutes instead
of every minute, and is woken the moment you switch to it.

## Status and known limitations

- Runs on macOS and Windows. The Windows path has been exercised: build, flash,
  collect, reconnect after a board reboot.
- Switching between two real computers has been implemented and tested from the
  board side, but not yet with two machines connected at once.
- The Bluetooth characteristics require no pairing, so any nearby BLE device can
  write a snapshot to the board.
- `gpu` and `temp` are not measured on any platform, and `battery` and `power`
  only on macOS laptops. On Windows `swap` reads zero, because `sysinfo` reports
  no used swap there.
- Per process power is not available on either platform without elevated access,
  so the IMPACT ordering is a composite of CPU and disk traffic, not watts.
- Two display optimisations were tried and reverted: writing only the changed
  window made the panel glitch, and idling it at its low refresh rate washed the
  picture out to grey. Both remain in the driver, uncalled. See `CLAUDE.md`.
- The card slot and real time clock chip on the board are unused.

## Licence

Apache License 2.0, see `LICENSE` and `NOTICE`.

The display driver derives from Waveshare's Apache-2.0 example. The bitmap fonts
are generated from Montserrat, under the SIL Open Font License 1.1; its text is
in `licenses/Montserrat-OFL.txt`. If you redistribute this project or the font
tables, both notices have to travel with it.
