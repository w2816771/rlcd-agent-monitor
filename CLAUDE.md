# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Two halves of one product for the Waveshare ESP32-S3-RLCD-4.2 (400x300 reflective
monochrome LCD):

- `main/` + `components/rlcd/` — ESP-IDF 5.5.1 firmware (C++17) that renders seven
  LVGL pages and advertises a NimBLE GATT peripheral named `RLCD Monitor`.
- `host/` — a Rust collector for macOS and Windows that scrapes local
  system/agent stats plus weather and news, then pushes them to the board over
  BLE. Up to three machines can run it against one board at the same time.

They are coupled only by the JSON snapshot described under "BLE protocol" below.
Any field added on one side must be added on the other or it is silently dropped.

## Build and run

Firmware (the wrapper sources ESP-IDF from `/Users/a58/esp/esp-idf-v5.5.1`):

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
WS 45, DIN 10) in `audio_meter.cpp`, display SPI3 in `components/rlcd`.

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
macOS only. Codex quota is read from its local rate-limit records;
Claude totals come from `ccusage` via `npx`, rate limited to one call per five
minutes. Weather (Open-Meteo) and Google News RSS refresh on
`online_refresh_minutes`.

`host/config.yaml` is re-read every loop iteration, so config changes take effect
without a restart. Keep that property when adding settings.

All strings sent to the board must pass through `ascii()` with a length limit,
since the display fonts have no glyphs for anything else.

## BLE protocol

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

## Conventions

The existing code is deliberately comment-light and uses full words for names.
Prefer extending `MonitorModel` and the existing page builders over introducing
new abstractions. `sdkconfig` is generated and git-ignored; persistent config
belongs in `sdkconfig.defaults`.
