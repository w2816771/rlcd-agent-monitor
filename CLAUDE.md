# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Two halves of one product for the Waveshare ESP32-S3-RLCD-4.2 (400x300 reflective
monochrome LCD):

- `main/` + `components/rlcd/` — ESP-IDF 5.5.1 firmware (C++17) that renders nine
  LVGL pages and advertises a NimBLE GATT peripheral named `RLCD Monitor`.
- `host/` — a Rust collector for macOS and Windows that scrapes local
  system/agent stats plus weather and news, then pushes them to the board over
  BLE. Up to three machines can run it against one board at the same time.

They are coupled only by the JSON snapshot described under "BLE protocol" below.
Any field added on one side must be added on the other or it is silently dropped.

## Build and run

Firmware (the wrapper sources ESP-IDF from `IDF_PATH`, or `~/esp/esp-idf-v5.5.1`):

```bash
./idf.sh build
```

```bash
./idf.sh -p /dev/cu.usbmodemXXXX flash monitor
```

Host collector:

```bash
cd host && cargo build --release
```

```bash
cd host && cargo test
```

A single Rust test: `cargo test ascii_removes_unsupported_glyphs_and_limits_length`.
There are no firmware tests; verify firmware changes by flashing and watching the
serial monitor. Install the collector as a launchd agent with `host/service.sh
install`, and read `/tmp/rlcd-host.log` via `host/service.sh logs`. On Windows, `host/service.ps1 install`
registers a logon task instead.

## Firmware architecture

`app_main` in `main/main.cpp` wires everything and then owns two FreeRTOS tasks:

- `lvgl_task` (core 1) drains one pending BLE snapshot per iteration, merges in
  local sensor readings, calls `MonitorUi::update`, and runs `lv_timer_handler`.
  Sensors are polled every 10 s on USB and 60 s on battery, and the noise level
  only redraws on a change of 3 dB or more, because a full refresh is expensive.
- `button_task` (core 0) blocks on a queue fed by level-triggered interrupts on
  KEY (GPIO18) and BOOT (GPIO0), which double as light sleep wake sources. The
  handler masks its pin and the task re-arms it after the release. A press over
  700 ms is a long press. The UI never runs on this task.

The display is 1-bit. LVGL renders RGB565 into two full PSRAM framebuffers and the
`flush` callback converts each pixel to black or white by luminance threshold,
then pushes the whole frame. There is no partial refresh, so keep redraws rare.

`MonitorModel` (`main/monitor_model.h`) is the single plain struct holding all
displayed state; `monitor_demo_model()` provides the pre-connection placeholder
content. `MonitorUi` builds every page up front and rebuilds them all on update.

Fonts are pre-generated 1-bit LVGL C arrays (`rlcd_font_bold_*.c`), not runtime
fonts. Each file's header comment records the exact `lv_font_conv` command,
including the restricted glyph set; regenerate with that command and add the new
file to `main/CMakeLists.txt` and `main/rlcd_fonts.h`. The limited glyph set is
why the host strips text to ASCII.

Board peripherals: SHTC3 temperature/humidity on I2C0 (SDA 13, SCL 14, address
0x70), battery on ADC channel 3, ES7210 microphone over I2S TDM (MCLK 16, BCLK 9,
WS 45, DIN 10) in `audio_meter.cpp`, ES8311 speaker DAC on the same I2S clocks
with DOUT 8 and the amplifier enable on GPIO46 in `audio_player.cpp`, display
SPI3 in `components/rlcd`.

The microphone and the speaker share the I2S clock pins, so only one may have
its channel enabled at a time. `audio_meter` holds a bus mutex from enable to
disable of its receive channel; `audio_player` takes it to ring and creates its
transmit channel fresh each time, then tears everything down and gives the bus
back, so between alerts the DAC and amplifier are fully off.

`components/rlcd/display_bsp.cpp` is derived from Waveshare's Apache-2.0 ST7305
example and contains Chinese comments; treat it as vendor code and change it only
when the panel behaviour is wrong.

## Host collector architecture

`host/src/main.rs` is a single file with a flat function layout: connect, then
loop building a `Snapshot` and writing it out. Most system data comes from
shelling out to macOS tools through `command_output`, so missing values degrade to
zero rather than erroring. Those functions are gated to macOS; every other
platform uses the `portable` module built on `sysinfo`, which is compiled on the
Mac too so a Mac build still type-checks the Windows path. Battery readings are
macOS only. Limits are shown as percentages with their reset times honoured (`limits.rs`):
Codex from the `rate_limits` in its session records, Claude from
`~/.claude/rlcd-usage.json`, which the user's status line script writes from the
figures Claude Code passes it. Token counts leave cache rereads out and, for
Claude, come from `ccusage` via `npx`, one call per five minutes; never sum the
transcript records directly, since resumed sessions repeat them. Weather (Open-Meteo) and Google News RSS refresh on
`online_refresh_minutes`.

`host/config.yaml` is re-read every loop iteration, so config changes take effect
without a restart. Keep that property when adding settings.

Hardware readings (`sensors.rs`): GPU from `nvidia-smi`, unprivileged; fan
speeds and CPU temperature from `host/sensors.json`, written by `sensors.ps1`
running as a separate elevated task. Never elevate the collector itself: it
executes shell commands from a user-writable file. The board keeps trend history
itself (`main/trends.cpp`, one point per snapshot, 60 points) so the snapshot
does not grow; charts are `lv_chart` with `LV_CHART_POINT_NONE` for the points a
young history does not have yet.

All strings sent to the board must pass through `ascii()` with a length limit,
since the display fonts have no glyphs for anything else.

## BLE protocol

The full host contract, including the snapshot schema and how every field is
derived, is in `docs/host-api.md`. Keep it in sync when either side changes.

Service `7a1e0001-7e57-4d6d-8a1b-2c3d4e5f6071`, telemetry characteristic
`7a1e0002-...6071`, write-without-response. Each write is a 4-byte header
(protocol version, message id, chunk index, chunk count) followed by 16 payload
bytes of the UTF-8 JSON snapshot, sized for the minimum ATT MTU. The firmware
buffers up to 4096 bytes and drops a message on any chunk it did not expect, so
chunks must arrive in order under one message id. Bump `PROTOCOL_VERSION` in
`main/monitor_ble.cpp` and `v` in the host `Snapshot` together if the framing
changes.

Button characteristic `7a1e0003-...6071`, notify only, going the other way. Each
notification is four bytes: protocol version, sequence, event, and the one-based
page that was on screen. Events are 1 KEY, 2 KEY held, 3 BOOT, 4 BOOT held. The
host maps each to a shell command under `buttons` in `host/config.yaml`; the
board never sends a command, only a fixed code.

Multiple hosts: the board keeps advertising until three are connected and gives
each connection its own reassembly slot, so their chunks never interleave. Each
snapshot carries a `source` name, the host name unless `device_name` overrides
it. The board shows one source, stored by name in NVS under `rlcd/source`; a
source that drops keeps its last snapshot, marked stale, instead of the screen
jumping to another host. KEY held switches source on every page except news.
Buttons go only to the shown host. The board also sends event 5 to a host when
it becomes shown and 6 when hidden, and a hidden host slows to
`hidden_update_seconds` until woken.

Never send a notification or do heavy work inside a NimBLE callback such as
`gap_event` or `telemetry_access`. They run on the NimBLE host task, and sending
from there once overflowed its stack and reset the board. Queue state under
`model_mutex` and let `monitor_ble_flush_events` send from the UI task. A crash
dump is kept in the `coredump` partition; read it with `./idf.sh -p <port>
coredump-info`, since panic output otherwise goes only to UART0.

## Flashing on Windows: when the board goes dark

Flash with esptool over the native USB port using `--before usb_reset`; the
default reset cannot enter download mode while the firmware runs. About one
flash in four, the reset request wedges the chip instead: esptool reports "No
serial data received", the COM port then returns nothing, BLE stops
advertising, and `--before no_reset` fails too. Nothing in the build causes
this; it is the board. Only power clears it: replug USB or PWR off and on. If
the port still does not appear, hold BOOT, click PWR, keep BOOT held for a
second, then flash. Do not toggle DTR or RTS on the port by hand for a boot
capture; that triggers the same wedge.

The panic text does not reach the USB console. Read a crash with
`idf.py -p COMx coredump-info` against the ELF of the build that crashed. A
boot loop with "Starting" once a second and no panic on the console was a
stack overflow in the main task, found that way.

## LVGL memory and a silent hang

LVGL allocates from the C library heap (`CONFIG_LV_USE_CLIB_MALLOC`), which on
this board includes the 8 MB of PSRAM. It used to run on its own 64 KB pool,
and nine pages of objects plus six charts did not fit. The failure was silent:
LVGL asserts on a glyph buffer allocation that returns NULL, and its default
assert handler is `while(1)`, so the render thread parked for good with the
task watchdog firing every five seconds and nothing on the console to say
why. Four backtraces sampled at the same instruction in `lv_draw_label.c` was
the tell. The assert handler is now `abort()`, set from the root
`CMakeLists.txt` because the Kconfig offers only the header to include, so a
repeat becomes a core dump and a reboot rather than a hang. The semicolon in
that definition is escaped: an unescaped one is a CMake list separator.

The USB console can go quiet after a flash while BLE keeps working; only a
power cycle has brought it back. Evidence from before it goes quiet is still
evidence.

## Two display optimisations that did not survive contact

Both are still in `components/rlcd/display_bsp.cpp`, and neither is called.
Reintroducing either without the ST7305 datasheet is likely to repeat the same
result.

- `RLCD_DisplayChanged()` writes only the changed window instead of all 15,000
  bytes. Used from `flush`, the panel glitched once a page had been on screen a
  while. Either the page and column address arithmetic is subtly wrong, or the
  panel needs the periodic rewrite that skipping an unchanged frame removes.
- `RLCD_SetLowPower(true)` idles the panel at its low refresh rate between
  writes. It washed the picture out to pale grey: the drive waveform at that
  rate does not hold contrast on this reflective panel.

Neither was worth much. The full frame write is one 6 ms SPI transfer against a
BLE radio that wakes twice a second. If redraws need to be cheaper, the target
is `MonitorUi::update`, which deletes and rebuilds every page on every update;
setting label text in place would cut the LVGL render, which is the real cost.

## Conventions

The existing code is deliberately comment-light and uses full words for names.
Prefer extending `MonitorModel` and the existing page builders over introducing
new abstractions. `sdkconfig` is generated and git-ignored; persistent config
belongs in `sdkconfig.defaults`.
