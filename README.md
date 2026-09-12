# RLCD Agent Monitor

ESP-IDF firmware and a Rust macOS companion for the Waveshare
ESP32-S3-RLCD-4.2. It renders five 400x300 landscape pages for time and room
conditions, host system, Codex/Claude usage, top processes, and Google News.
KEY advances to the next page and BOOT goes back. Hold BOOT to return home;
hold KEY on News to switch between four-item headline pages. The PWR button is
hardware power control and is not an application input.

## Toolchain and build

ESP-IDF 5.5.1 is installed at `/Users/a58/esp/esp-idf-v5.5.1`. The project-local
wrapper activates that environment automatically:

```sh
./idf.sh build
./idf.sh -p /dev/cu.usbmodemXXXX flash monitor
```

Alternatively, activate ESP-IDF in the current shell with
`source /Users/a58/esp/esp-idf-v5.5.1/export.sh`, then use `idf.py` normally.
The first build downloads LVGL through the ESP-IDF Component Manager. The
display driver and pin mapping are based on Waveshare's Apache-2.0 ESP-IDF
example for this board.

## Current milestone

- ST7305 display initialization and landscape framebuffer
- Five big-type LVGL pages using native 1-bit fonts
- KEY/BOOT forward and back navigation, long-press Home and News paging
- NimBLE GATT peripheral named `RLCD Monitor`
- Chunked, versioned BLE telemetry protocol
- Rust macOS collector with automatic BLE reconnect
- Live CPU/RAM percentage bars, swap, per-process CPU/RAM, Codex, and Claude statistics
- Codex 5-hour/weekly quota plus cached `ccusage` Claude 5-hour/weekly totals
- Onboard SHTC3 temperature/humidity, ES7210 microphone level, and ADC battery readings
- Weather from Open-Meteo and eight headlines from Google News RSS

## Host companion

Rust is installed through Homebrew. Build or run the collector with:

```sh
cd host
cargo build --release
cargo run --release
```

Edit `host/config.yaml` to configure location, refresh rates, Google News topic
and locale, or a custom RSS URL. The running service notices changes without a
rebuild. CPU temperature and GPU utilization are shown as unavailable when
macOS does not expose them.
The companion invokes `ccusage` through `npx` at most once every five minutes;
Codex quota percentages come directly from its local rate-limit records.

To run the collector automatically after macOS login:

```sh
cd host
./service.sh install
./service.sh status
./service.sh logs
```

Use `./service.sh uninstall` to remove it. macOS may request Bluetooth access
the first time the collector starts.

## BLE protocol

Service UUID: `7a1e0001-7e57-4d6d-8a1b-2c3d4e5f6071`

Telemetry characteristic: `7a1e0002-7e57-4d6d-8a1b-2c3d4e5f6071`

Each write starts with four bytes: protocol version, message ID, zero-based
chunk index, and total chunk count. The remaining bytes are one segment of a
UTF-8 JSON snapshot. Version 1 uses 16 payload bytes per chunk so it works with
the minimum BLE ATT MTU.
