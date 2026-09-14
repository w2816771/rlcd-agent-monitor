# RLCD Monitor host API and requirements

This document is the contract between the Waveshare ESP32-S3-RLCD-4.2 board and
a host collector. It is written for whoever builds or runs the host on a new
machine, including an AI assistant on a Windows PC.

**Start here.** A working host already exists at `host/src/main.rs` and builds
for Windows as well as macOS. Build and run it before writing anything new. Use
this document to verify its behavior, to debug it, or as the specification if a
reimplementation is truly needed.

---

## 1. System overview

- The board is a Bluetooth Low Energy peripheral that renders seven pages on a
  400 by 300 monochrome reflective display.
- A host is a program on a computer that connects to the board, sends a JSON
  snapshot of that computer's state every few seconds, and listens for button
  events from the board.
- Up to **three hosts** can stay connected at the same time, for example a Mac
  and a PC. The board shows one host's data at a time and the user switches
  between them on the board.
- Room temperature, humidity, noise, and the board's own battery are measured
  on the board. The host never sends those.

---

## 2. Discovery and connection

| Item | Value |
|---|---|
| Advertised local name | `RLCD Monitor` |
| Service UUID | `7a1e0001-7e57-4d6d-8a1b-2c3d4e5f6071` |
| Telemetry characteristic | `7a1e0002-7e57-4d6d-8a1b-2c3d4e5f6071`, write without response |
| Button characteristic | `7a1e0003-7e57-4d6d-8a1b-2c3d4e5f6071`, notify |
| Maximum simultaneous hosts | 3 |
| Pairing or bonding | none |

Requirements:

1. Scan for a peripheral whose local name is exactly `RLCD Monitor`, connect,
   and discover services. The reference host polls the scan results every
   500 ms for up to 10 s, with a 10 s connect timeout and a 10 s discovery
   timeout, then retries after 3 s.
2. Subscribe to notifications on the button characteristic right after
   connecting.
3. Expect the board to request a connection interval of 90 to 120 ms, a
   peripheral latency of 4, and a 6 s supervision timeout. Accept it.
4. The board keeps advertising while fewer than three hosts are connected, so a
   second computer can connect while the first stays connected.
5. On any disconnect or send failure, disconnect cleanly and scan again.

---

## 3. Telemetry write protocol

A snapshot is one UTF-8 JSON object, split into chunks and written to the
telemetry characteristic with **write without response**.

Each write is a 4 byte header followed by one piece of the JSON:

| Byte | Field | Rule |
|---|---|---|
| 0 | protocol version | always `1` |
| 1 | message id | increments per snapshot, wraps from 255 to 0 |
| 2 | chunk index | zero-based |
| 3 | chunk count | total chunks in this message, 1 to 255 |
| 4 onward | JSON bytes | 1 to 240 bytes per write |

Requirements:

1. Send chunks strictly in order, all under the same message id and chunk
   count. The board drops the whole message on any out-of-order chunk.
2. Keep each write to **16 JSON bytes** unless a larger MTU has been
   negotiated. 16 plus the header fits the minimum 23 byte ATT MTU.
3. Pause about 8 ms between writes. The reference host gives a whole snapshot
   10 s to send, and treats a timeout as a failure.
4. Keep the JSON under **4,080 bytes**, which is 255 chunks of 16. The board's
   reassembly buffer holds 4,096. A typical snapshot is about 1,500 bytes.
5. A chunk with index 0 always starts a new message and discards any partial
   one.
6. Writes are unacknowledged, so the host cannot tell whether the board parsed
   a snapshot. Treat BLE write errors and disconnects as the only failure
   signals.

The board applies a snapshot only after the last chunk arrives and the JSON
parses. **Every snapshot starts from built-in placeholder values**, not from the
previous snapshot. Any key you omit shows placeholder demo data on screen, so
send every key in section 4 every time.

---

## 4. Snapshot JSON schema

All keys are required by this contract. "Shown" means the value currently
appears on a page; keys marked as not shown are still parsed, so keep sending
them with the value given.

| Key | Type | Meaning and units | Board handling | Shown |
|---|---|---|---|---|
| `v` | integer | schema version, send `1` | not read; version lives in the chunk header | no |
| `source` | string | this host's name, see section 6 | empty becomes `HOST 1` to `HOST 3` | yes |
| `time` | string | local time `HH:MM`, 24 hour | drawn with a font that has **only digits and a colon** | yes |
| `date` | string | local date like `SUN 14 SEP` | | yes |
| `weather_c` | integer | current outdoor temperature, whole degrees Celsius | | yes |
| `weather` | string | weather condition, see section 8 | | yes |
| `region` | string | place name shown under the clock | | yes |
| `battery` | integer | host laptop battery percent, 0 to 100 | clamped 0 to 100; the board shows its own battery instead | no |
| `cpu` | integer | whole-system CPU use percent, 0 to 100 | clamped 0 to 100 | yes |
| `ram` | integer | memory use percent, 0 to 100 | clamped 0 to 100 | yes |
| `ram_used_gb` | number | memory in use, GiB | | yes |
| `ram_total_gb` | number | installed memory, GiB | also the divisor for per-process memory percent | yes |
| `gpu` | integer | GPU use percent, send `0` if unknown | clamped 0 to 100 | no |
| `swap` | integer | swap or page file use percent | clamped 0 to 100 | no |
| `swap_used_gb` | number | swap or page file in use, GiB | | yes |
| `temp` | number | CPU temperature Celsius, send `0` if unknown | | no |
| `power` | number | system power draw watts, send `0` if unknown | | no |
| `net_down` | number | download rate, **kilobytes per second** | shown as K/S, or M/S from 1,024 | yes |
| `net_up` | number | upload rate, kilobytes per second | same | yes |
| `noise_offset` | number | microphone calibration in dB, from config | applied to the board's microphone | indirectly |
| `process_count` | integer | number of running processes | clamped 0 to 65,535 | yes |
| `processes` | array | 5 to 12 items, see 4.2 | more than 12 ignored | top 3, reordered on the board |
| `codex` | object | Codex usage, see 4.1 | | yes |
| `claude` | object | Claude usage, see 4.1 | | yes |
| `headlines` | array of string | exactly 8 news titles | extra items ignored | yes |
| `headline_ages` | array of string | exactly 8 ages matching `headlines`, like `12m`, `3h`, `2d` | | yes |

### 4.1 Agent usage object, used by `codex` and `claude`

| Key | Type | Meaning | Board handling | Shown |
|---|---|---|---|---|
| `active` | boolean | the agent app is running now | star in status bar, RUN or IDLE | yes |
| `tokens_k` | integer | tokens used **today**, in thousands | negative becomes 0 | yes |
| `context` | integer | context window use percent of the latest turn today | clamped 0 to 100 | no |
| `sessions` | integer | sessions with activity today | clamped 0 to 255 | yes |
| `cost` | number | cost in USD, send `0` | | no |
| `five_hour` | string | 5 hour window, like `5H 12%` or `5H 1.4M` | | yes |
| `weekly` | string | 7 day window, like `7D 20%` or `7D 9.1M` | | yes |

The agent page shows `(codex.tokens_k + claude.tokens_k) / 1000` as millions
under TOKENS TODAY.

### 4.2 Process item

| Key | Type | Meaning | Board handling |
|---|---|---|---|
| `name` | string | process name, no path or extension | clipped at the row width |
| `cpu` | number | percent of **one core**, so it can exceed 100 | drawn as a whole number |
| `ram_mb` | integer | resident memory, MiB | clamped 0 to 65,535 |
| `impact` | number | composite cost, see below | missing falls back to `cpu` |

If fewer than 5 processes exist, pad with
`{"name": "-", "cpu": 0, "ram_mb": 0, "impact": 0}`.

**Instances are added together.** A browser or agent runs as many processes;
send one item per name with the totals, or the board shows the same application
in every row. The process name has its `.exe` stripped, and a name with no
printable ASCII left in it becomes `PID <number>` so unrelated applications do
not merge into one row.

**The board reorders these itself.** Holding KEY on the host system page cycles
the rows between CPU, RAM, and impact without asking the host for a different
list, so the array must contain every process that could lead any of the three
orderings: send the union of the top 5 by each, deduplicated, up to 12 items.
Sending only the top 5 by CPU makes the other two orderings wrong.

`impact` is **not watts**. Neither platform exposes per process power without
elevated access, so the reference host sends processor percent plus disk
throughput in MiB/s times 2, which is the same kind of composite Task Manager
and Activity Monitor display. The board labels it IMPACT. A host with nothing
better to offer should send `cpu`, which makes that ordering match the CPU one.
The reference host does exactly that on macOS, where `ps` reports no disk
figures, and on the first snapshot, where there is no interval to measure a
rate over.

### 4.3 Example

```json
{
  "v": 1,
  "source": "PC",
  "time": "14:05",
  "date": "SUN 14 SEP",
  "weather_c": 24,
  "weather": "PARTLY CLOUDY",
  "region": "BEIJING - CHINA",
  "battery": 0,
  "cpu": 37,
  "ram": 58,
  "ram_used_gb": 18.4,
  "ram_total_gb": 32.0,
  "gpu": 0,
  "swap": 4,
  "swap_used_gb": 0.6,
  "temp": 0,
  "power": 0,
  "codex": {"active": true, "tokens_k": 2423, "context": 42, "sessions": 3,
            "cost": 0, "five_hour": "5H 12%", "weekly": "7D 20%"},
  "claude": {"active": false, "tokens_k": 6480, "context": 31, "sessions": 2,
             "cost": 0, "five_hour": "5H 1.4M", "weekly": "7D 9.1M"},
  "processes": [
    {"name": "code", "cpu": 24.0, "ram_mb": 1843, "impact": 26.4},
    {"name": "claude", "cpu": 18.0, "ram_mb": 1229, "impact": 18.0},
    {"name": "chrome", "cpu": 11.0, "ram_mb": 846, "impact": 39.2},
    {"name": "explorer", "cpu": 3.0, "ram_mb": 180, "impact": 3.0},
    {"name": "-", "cpu": 0, "ram_mb": 0, "impact": 0}
  ],
  "process_count": 214,
  "net_down": 51.9,
  "net_up": 10.7,
  "noise_offset": 87,
  "headlines": ["New models push efficient on-device AI forward", "-", "-", "-",
                "-", "-", "-", "-"],
  "headline_ages": ["2h", "--", "--", "--", "--", "--", "--", "--"]
}
```

---

## 5. Notifications from the board

The board notifies the button characteristic with exactly 4 bytes:

| Byte | Field |
|---|---|
| 0 | protocol version, `1` |
| 1 | sequence number, increments per notification, wraps |
| 2 | event code |
| 3 | page number, one-based, `0` for events 5 and 6 |

| Event | Meaning | Required host action |
|---|---|---|
| 1 | KEY pressed | run the `buttons.key` command if set |
| 2 | KEY held on the news or debug page | run `buttons.key_hold` if set |
| 3 | BOOT pressed | run `buttons.boot` if set |
| 4 | BOOT held | run `buttons.boot_hold` if set |
| 5 | this host is now the one shown | switch to the fast update interval; if it was hidden, send a snapshot immediately |
| 6 | this host is now hidden | switch to the slow update interval |

Page numbers: 1 overview, 2 host system, 3 agent usage, 4 news, 5 devices,
6 debug log. The overview carries the sound history, the host system page
carries the top processes, and the debug log shows the board's own log lines.

Rules the board enforces:

- Button events go **only to the shown host**.
- Holding KEY on any page except host system, news and the debug log switches
  the shown host. That switch is not sent as event 2. On those three pages KEY
  held reorders the process rows, pages through headlines, and pages through log
  lines respectively, and is sent as event 2.
- The board sends event 5 or 6 once a host has delivered its first snapshot,
  and again whenever that host's shown state changes. Ignore unknown event
  codes.

---

## 6. Multiple hosts and the source name

- The board identifies each host by the `source` string, so **every computer
  needs a different name**, such as `MAC` and `PC`. Two hosts with the same name
  will overwrite each other on the board.
- The user's chosen source is saved on the board by name and survives reboots.
  Until that host connects, another connected host is shown temporarily.
- If the shown host disconnects, the board keeps showing its last snapshot,
  with the age in the header, rather than jumping to another host.
- A host should assume it is shown when it first connects, send its first
  snapshot at once, then follow events 5 and 6.

---

## 7. Text rules

The board fonts contain only printable ASCII, `0x20` to `0x7E`. Replace any
other character with a space, collapse repeated spaces, then trim to length.

| Field | Limit used by the reference host | Notes |
|---|---|---|
| `source` | 16 characters, uppercase | about 13 are visible on the devices page; keep it short |
| `region` | 34, uppercase | |
| process `name` | 20 | |
| each headline | 68 | wraps to two lines |
| `weather` | fixed words from section 8 | |
| `five_hour`, `weekly` | about 10 | |
| `time` | exactly `HH:MM` | no letters, no seconds, no AM or PM |

---

## 8. How each value is derived

The reference host computes values this way. A reimplementation should match
the meaning, not necessarily the method.

### Time and place
- `time` is local time formatted `%H:%M`. `date` is local `%a %d %b`, uppercased.
- `region` comes from config `weather.region`.

### Weather, refreshed every `online_refresh_minutes`
- Request `https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m,weather_code`.
- `weather_c` is `current.temperature_2m` rounded.
- `weather` maps `current.weather_code`:

| Code | Text |
|---|---|
| 0 | `CLEAR` |
| 1 to 3 | `PARTLY CLOUDY` |
| 45, 48 | `FOG` |
| 51 to 57 | `DRIZZLE` |
| 61 to 67, 80 to 82 | `RAIN` |
| 71 to 77, 85, 86 | `SNOW` |
| 95 to 99 | `THUNDERSTORM` |
| anything else | `UNKNOWN` |

- On failure send `weather_c` 0 and `weather` `WEATHER UNAVAILABLE`.

### News, refreshed every `online_refresh_minutes`
- If `news.rss_url` is empty, fetch
  `https://news.google.com/rss/search?q={topic}&hl={language}&gl={country}&ceid={edition}`
  with the topic URL-encoded; otherwise fetch `news.rss_url`.
- Take the first 8 items. Ages come from each item's publish date: minutes
  under an hour as `12m`, hours under a day as `3h`, otherwise days as `2d`,
  and `--` when there is no date.
- Pad to 8 with `-` and `--`. On failure send 8 copies of `NEWS UNAVAILABLE`
  with ages `--`.

### System, every snapshot
- `cpu`: whole-system CPU percent. Sampling needs two readings over time, so
  keep the sampler alive between snapshots.
- `ram`, `ram_used_gb`, `ram_total_gb`: used and total physical memory, with
  GiB meaning bytes divided by 1,073,741,824.
- `swap`, `swap_used_gb`: swap on macOS, page file on Windows.
- `processes`: all processes sorted by CPU descending, top 5, per-core CPU
  percent, resident memory in MiB. `process_count` is the full count.
- `net_down`, `net_up`: sum the cumulative received and sent byte counters of
  all non-loopback interfaces, subtract the previous snapshot's totals, divide
  by elapsed seconds and by 1,024. Send 0 for the first snapshot.
- `battery` and `power`: laptop battery percent and watts on macOS. Send 0 on a
  desktop PC. `gpu` and `temp`: send 0.

### Agent activity
- `codex.active` is true when a running process name matches `codex` or
  `chatgpt`. `claude.active` is true for `claude`.

### Tokens today
"Today" means records with a timestamp at or after **local midnight**. Every
record in both formats has a top-level RFC 3339 `timestamp`. A record without a
usable timestamp counts as today. Only read files modified since local midnight.

**Codex**, files under `%USERPROFILE%\.codex\sessions\` on Windows, or
`~/.codex/sessions/` on macOS, any depth, `*.jsonl`:
- Use lines where `payload.type` is `token_count`.
- `payload.info.total_token_usage.total_tokens` is **cumulative for the
  session**. Today's tokens for one file are its last cumulative value minus the
  last cumulative value recorded before midnight, or minus 0 if there is none.
- `tokens_k` is the sum over files active today, divided by 1,000.
- `sessions` counts files with at least one record today.
- `context` is `payload.info.last_token_usage.total_tokens` divided by
  `payload.info.model_context_window`, as a percent, from the latest record today.
- `five_hour` is `5H {payload.rate_limits.primary.used_percent}%` and `weekly`
  is `7D {payload.rate_limits.secondary.used_percent}%`, both rounded, from the
  latest record today. Default `5H --` and `7D --`.

**Claude**, files under `%USERPROFILE%\.claude\projects\` on Windows, or
`~/.claude/projects/` on macOS, any depth, `*.jsonl`:
- Use lines that have `message.usage`.
- Each record's tokens are `input_tokens + output_tokens +
  cache_read_input_tokens + cache_creation_input_tokens`. Sum today's records
  for `tokens_k`, divided by 1,000.
- `sessions` counts files with at least one record today.
- `context` is the latest record's `input_tokens + cache_read_input_tokens +
  cache_creation_input_tokens` divided by 200,000, as a percent, capped at 100.
- `five_hour` and `weekly` come from `ccusage`, run at most once every
  5 minutes and cached between runs. This needs Node.js; on Windows call
  `npx.cmd`.
  - `npx --yes ccusage@latest claude blocks --json --offline --active`: take
    `totalTokens` of the item in `blocks` whose `isActive` is true.
  - `npx --yes ccusage@latest claude weekly --json --offline`: take
    `totalTokens` of the last item in `weekly`.
  - Format both as `5H` or `7D` followed by `1.4M` for a million or more,
    `940K` for a thousand or more, otherwise the plain number.

Verify both folders exist on the PC. If an agent is not installed, send its
object with zeros and `--` strings.

---

## 9. Host run loop

1. Load config. Scan and connect. Set "shown" to true.
2. Each iteration:
   1. Check the connection, with a 2 s timeout. If it dropped, go back to
      scanning.
   2. Reload `config.yaml`. Refresh weather and news if the config changed or
      `online_refresh_minutes` has passed, at least 1 minute apart.
   3. Build the snapshot, increment the message id, and send it within 10 s.
      On failure, disconnect with a 3 s timeout and go back to scanning.
   4. Wait. If shown, wait `max(update_seconds, 5)` seconds. If hidden, wait
      `max(hidden_update_seconds, update_seconds, 5)` seconds. End the wait
      early when event 5 arrives while hidden.
3. Handle button notifications concurrently with the loop. Run commands in the
   background so a slow command never blocks sending.
4. After a scan or connect failure, wait 3 s and retry forever.

Button commands run through `cmd /C` on Windows and `sh -c` elsewhere, with
environment variables `RLCD_EVENT` and `RLCD_PAGE` set. The command text comes
only from the local config; the board sends a code, never a command.

---

## 10. Configuration file

The host reads `config.yaml` from its working directory, or from the path in
the `RLCD_CONFIG` environment variable, on every loop iteration. Missing keys
take the defaults below.

`config.yaml` is **not tracked by git**, so coordinates and button commands
stay local. `config.example.yaml` is the template to copy; `service.ps1
install` copies it for you when no `config.yaml` exists yet.

| Key | Default | Meaning |
|---|---|---|
| `device_name` | empty, which uses the host name | `source` sent to the board; set `PC` on the PC |
| `update_seconds` | 15, repo file uses 60 | send interval while shown, minimum 5 |
| `hidden_update_seconds` | 300 | send interval while hidden |
| `online_refresh_minutes` | 10 | weather and news refresh, minimum 1 |
| `weather.latitude` | 39.9042 | |
| `weather.longitude` | 116.4074 | |
| `weather.region` | `BEIJING - CHINA` | |
| `news.topic` | `technology` | Google News search term |
| `news.language` | `en-US` | |
| `news.country` | `US` | |
| `news.edition` | `US:en` | |
| `news.rss_url` | empty | replaces Google News when set |
| `buttons.key` | empty | command for event 1 |
| `buttons.key_hold` | empty | command for event 2 |
| `buttons.boot` | empty | command for event 3 |
| `buttons.boot_hold` | empty | command for event 4 |
| `noise_offset_db` | 87 | sent as `noise_offset` |

Example for the PC:

```yaml
device_name: "PC"
update_seconds: 60
hidden_update_seconds: 300
online_refresh_minutes: 10
weather:
  latitude: 39.9042
  longitude: 116.4074
  region: BEIJING - CHINA
news:
  topic: technology
  language: en-US
  country: US
  edition: US:en
  rss_url: ""
buttons:
  key: ""
  key_hold: ""
  boot: ""
  boot_hold: ""
noise_offset_db: 87
```

---

## 11. Building and running on Windows

Prerequisites:

- Windows 10 version 1703 or later, or Windows 11, with Bluetooth LE.
- Rust stable through rustup with the MSVC toolchain, version 1.85 or later
  for edition 2024. Install the Visual Studio Build Tools C++ workload.
- Node.js, only for the Claude 5 hour and weekly figures.
- Internet access for weather and news.

From the `host` folder in PowerShell:

```powershell
cargo build --release
cargo test --release
.\target\release\rlcd-host.exe
```

Expected output while it works:

```
RLCD host monitor starting
Connected to RLCD Monitor
Updated 14:05:02 (1463 bytes)
```

To start it at logon, run once:

```powershell
powershell -ExecutionPolicy Bypass -File .\service.ps1 install
```

On Windows the collector also shows a notification area icon. It sits in the
hidden icons overflow until dragged out; its tooltip carries the time of the
last successful send, and its menu opens the log, the config, and the folder,
or stops the collector. The icon is Windows only, since the launchd service
already exposes the log on macOS, and a tray that fails to appear is reported
and then ignored rather than stopping collection.

`service.ps1` also accepts `uninstall`, `restart`, `status`, and `logs`. The
task runs the collector through `cmd` so its output appends to
`hostlcd-host.log`; `status` says whether the collector is actually
running, and `logs` follows that file. A logon task has no console, so
without the log there is no telling a working collector from a crashed one.

Platform notes for the reference host:

- macOS collection shells out to macOS tools and is compiled only on macOS.
  Every other platform uses the `portable` module built on the `sysinfo` crate.
- Battery and power read 0 outside macOS.
- The home folder is `HOME`, or `USERPROFILE` when `HOME` is unset.

---

## 12. Acceptance checklist

1. `cargo build --release` and `cargo test --release` succeed on the PC.
2. The exe logs a connection, then an `Updated` line about every
   `update_seconds`, each under 4,080 bytes.
3. On the board, the devices page, page 7, lists `PC` as LIVE with a Bluetooth
   signal value.
4. CPU, memory, and network figures roughly match Task Manager.
5. Weather, region, and headlines appear, and TOKENS TODAY is plausible.
6. With the Mac also connected, holding KEY on page 2 switches the header tag
   between `MAC` and `PC`. The newly shown host sends a fresh snapshot within a
   few seconds, and the hidden one slows to `hidden_update_seconds`.
7. A test mapping such as `boot: "echo %RLCD_EVENT% %RLCD_PAGE% >> %TEMP%\rlcd-button.txt"`
   writes a line when BOOT is pressed while the PC is shown, and does nothing
   while the Mac is shown.
8. Turning the board off and on again reconnects on its own.

---

## 13. Known limitations

- The characteristics require no pairing, so any nearby BLE device can write a
  snapshot to the board.
- Only three hosts can connect at once.
- `gpu` and `temp` are not measured on any platform, and `battery` and `power`
  only on macOS laptops.
- The Windows path of the reference host has been compiled but not yet run on
  Windows.
