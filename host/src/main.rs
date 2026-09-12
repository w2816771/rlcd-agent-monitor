use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    sync::{Mutex, OnceLock},
    time::{Duration, Instant, SystemTime},
};

use anyhow::{Context, Result, bail};
use btleplug::{
    api::{Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType},
    platform::{Adapter, Manager, Peripheral},
};
use chrono::{DateTime, Local, Utc};
use rss::Channel;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use tokio::time::{sleep, timeout};
use uuid::Uuid;
use walkdir::WalkDir;

const DEVICE_NAME: &str = "RLCD Monitor";
const TELEMETRY_UUID: &str = "7a1e0002-7e57-4d6d-8a1b-2c3d4e5f6071";
const CHUNK_PAYLOAD: usize = 16; // Always valid with the minimum BLE ATT MTU.

#[derive(Clone, Serialize)]
struct AgentUsage {
    active: bool,
    tokens_k: u64,
    context: u8,
    sessions: u8,
    cost: f32,
    five_hour: String,
    weekly: String,
}

#[derive(Clone, Serialize)]
struct ProcessUsage {
    name: String,
    cpu: f32,
    ram_mb: u32,
}

#[derive(Serialize)]
struct Snapshot {
    v: u8,
    time: String,
    date: String,
    weather_c: i32,
    weather: String,
    battery: u8,
    cpu: u8,
    ram: u8,
    ram_used_gb: f32,
    ram_total_gb: f32,
    gpu: u8,
    swap: u8,
    swap_used_gb: f32,
    temp: f32,
    power: f32,
    codex: AgentUsage,
    claude: AgentUsage,
    processes: Vec<ProcessUsage>,
    process_count: u16,
    headlines: Vec<String>,
    headline_ages: Vec<String>,
}

#[derive(Clone)]
struct OnlineData {
    weather_c: i32,
    weather: String,
    headlines: Vec<String>,
    headline_ages: Vec<String>,
}

#[derive(Clone, Deserialize, PartialEq)]
#[serde(default)]
struct Config {
    update_seconds: u64,
    online_refresh_minutes: u64,
    weather: WeatherConfig,
    news: NewsConfig,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            update_seconds: 15,
            online_refresh_minutes: 10,
            weather: WeatherConfig::default(),
            news: NewsConfig::default(),
        }
    }
}

#[derive(Clone, Deserialize, PartialEq)]
#[serde(default)]
struct WeatherConfig {
    latitude: f64,
    longitude: f64,
}

impl Default for WeatherConfig {
    fn default() -> Self {
        Self {
            latitude: 39.9042,
            longitude: 116.4074,
        }
    }
}

#[derive(Clone, Deserialize, PartialEq)]
#[serde(default)]
struct NewsConfig {
    topic: String,
    language: String,
    country: String,
    edition: String,
    rss_url: String,
}

impl Default for NewsConfig {
    fn default() -> Self {
        Self {
            topic: "technology".into(),
            language: "en-US".into(),
            country: "US".into(),
            edition: "US:en".into(),
            rss_url: String::new(),
        }
    }
}

fn load_config() -> Config {
    let path = env::var_os("RLCD_CONFIG")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("config.yaml"));
    match fs::read_to_string(&path) {
        Ok(contents) => serde_yaml::from_str(&contents).unwrap_or_else(|error| {
            eprintln!("Invalid {}: {error}; using defaults", path.display());
            Config::default()
        }),
        Err(error) => {
            eprintln!("Cannot read {}: {error}; using defaults", path.display());
            Config::default()
        }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    println!("RLCD host monitor starting");
    let adapter = bluetooth_adapter().await?;
    let characteristic_uuid = Uuid::parse_str(TELEMETRY_UUID)?;
    let mut message_id = 0u8;
    let mut config = load_config();
    let mut online = refresh_online(&config).await;
    let mut online_updated = Instant::now();

    loop {
        match find_and_connect(&adapter, characteristic_uuid).await {
            Ok((peripheral, characteristic)) => {
                println!("Connected to {DEVICE_NAME}");
                loop {
                    let still_connected =
                        timeout(Duration::from_secs(2), peripheral.is_connected())
                            .await
                            .ok()
                            .and_then(Result::ok)
                            .unwrap_or(false);
                    if !still_connected {
                        println!("Board disconnected; scanning again");
                        break;
                    }
                    let next_config = load_config();
                    if next_config != config
                        || online_updated.elapsed()
                            >= Duration::from_secs(next_config.online_refresh_minutes.max(1) * 60)
                    {
                        online = refresh_online(&next_config).await;
                        online_updated = Instant::now();
                    }
                    config = next_config;
                    let snapshot = collect_snapshot(&online);
                    let json = serde_json::to_vec(&snapshot)?;
                    message_id = message_id.wrapping_add(1);
                    let sent = timeout(
                        Duration::from_secs(10),
                        send_snapshot(&peripheral, &characteristic, message_id, &json),
                    )
                    .await;
                    if !matches!(sent, Ok(Ok(()))) {
                        match sent {
                            Ok(Err(error)) => eprintln!("BLE update failed: {error:#}"),
                            Err(_) => eprintln!("BLE update timed out"),
                            Ok(Ok(())) => unreachable!(),
                        }
                        let _ = timeout(Duration::from_secs(3), peripheral.disconnect()).await;
                        break;
                    }
                    println!(
                        "Updated {:02}:{:02}:{:02} ({} bytes)",
                        Local::now().hour(),
                        Local::now().minute(),
                        Local::now().second(),
                        json.len()
                    );
                    sleep(Duration::from_secs(config.update_seconds.max(5))).await;
                }
            }
            Err(error) => {
                eprintln!("{error:#}; retrying in 3 seconds");
                sleep(Duration::from_secs(3)).await;
            }
        }
    }
}

use chrono::Timelike;

async fn bluetooth_adapter() -> Result<Adapter> {
    let manager = Manager::new().await.context("create Bluetooth manager")?;
    manager
        .adapters()
        .await?
        .into_iter()
        .next()
        .context("no Bluetooth adapter found")
}

async fn find_and_connect(adapter: &Adapter, uuid: Uuid) -> Result<(Peripheral, Characteristic)> {
    adapter.start_scan(ScanFilter::default()).await?;
    for _ in 0..20 {
        for peripheral in adapter.peripherals().await? {
            let properties = timeout(Duration::from_secs(2), peripheral.properties())
                .await
                .context("Bluetooth properties timed out")??;
            let is_target = properties.and_then(|p| p.local_name).as_deref() == Some(DEVICE_NAME);
            if !is_target {
                continue;
            }
            adapter.stop_scan().await?;
            let is_connected = timeout(Duration::from_secs(2), peripheral.is_connected())
                .await
                .context("Bluetooth state check timed out")??;
            if !is_connected {
                timeout(Duration::from_secs(10), peripheral.connect())
                    .await
                    .context("Bluetooth connection timed out")??;
            }
            timeout(Duration::from_secs(10), peripheral.discover_services())
                .await
                .context("Bluetooth service discovery timed out")??;
            let characteristic = peripheral
                .characteristics()
                .into_iter()
                .find(|item| item.uuid == uuid)
                .context("telemetry characteristic not found")?;
            return Ok((peripheral, characteristic));
        }
        sleep(Duration::from_millis(500)).await;
    }
    adapter.stop_scan().await?;
    bail!("{DEVICE_NAME} not found")
}

async fn send_snapshot(
    peripheral: &Peripheral,
    characteristic: &Characteristic,
    message_id: u8,
    json: &[u8],
) -> Result<()> {
    let total = json.len().div_ceil(CHUNK_PAYLOAD);
    if total == 0 || total > u8::MAX as usize {
        bail!("snapshot too large");
    }
    for (index, payload) in json.chunks(CHUNK_PAYLOAD).enumerate() {
        let mut packet = Vec::with_capacity(payload.len() + 4);
        packet.extend_from_slice(&[1, message_id, index as u8, total as u8]);
        packet.extend_from_slice(payload);
        peripheral
            .write(characteristic, &packet, WriteType::WithoutResponse)
            .await?;
        sleep(Duration::from_millis(8)).await;
    }
    Ok(())
}

fn collect_snapshot(online: &OnlineData) -> Snapshot {
    let (processes, process_count, cpu, codex_active, claude_active) = process_usage();
    let (ram, ram_used_gb, ram_total_gb) = memory_usage();
    let (swap, swap_used_gb) = swap_usage();
    let now = Local::now();
    Snapshot {
        v: 1,
        time: now.format("%H:%M").to_string(),
        date: now.format("%a %d %b").to_string().to_uppercase(),
        weather_c: online.weather_c,
        weather: online.weather.clone(),
        battery: battery_percent(),
        cpu,
        ram,
        ram_used_gb,
        ram_total_gb,
        gpu: 0,
        swap,
        swap_used_gb,
        temp: 0.0,
        power: battery_power_watts(),
        codex: codex_usage(codex_active),
        claude: claude_usage(claude_active),
        processes,
        process_count,
        headlines: online.headlines.clone(),
        headline_ages: online.headline_ages.clone(),
    }
}

fn command_output(program: &str, args: &[&str]) -> String {
    Command::new(program)
        .args(args)
        .output()
        .ok()
        .filter(|o| o.status.success())
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default()
}

fn process_usage() -> (Vec<ProcessUsage>, u16, u8, bool, bool) {
    let output = command_output("ps", &["-axo", "pcpu=,rss=,comm="]);
    let mut values: Vec<_> = output
        .lines()
        .filter_map(|line| {
            let mut fields = line
                .trim()
                .splitn(3, char::is_whitespace)
                .filter(|s| !s.is_empty());
            let cpu = fields.next()?.parse::<f32>().ok()?;
            let ram_mb = fields.next()?.parse::<u32>().ok()? / 1024;
            let path = fields.next()?.trim();
            let name = Path::new(path).file_name()?.to_string_lossy();
            Some(ProcessUsage {
                name: ascii(&name, 20),
                cpu,
                ram_mb,
            })
        })
        .collect();
    let core_count = std::thread::available_parallelism().map_or(1, usize::from) as f32;
    let cpu = (values.iter().map(|p| p.cpu).sum::<f32>() / core_count).clamp(0.0, 100.0) as u8;
    let codex_active = any_process(&values, &["codex", "chatgpt"]);
    let claude_active = any_process(&values, &["claude"]);
    let process_count = values.len().min(u16::MAX as usize) as u16;
    values.sort_by(|a, b| b.cpu.total_cmp(&a.cpu));
    values.truncate(5);
    while values.len() < 5 {
        values.push(ProcessUsage {
            name: "-".into(),
            cpu: 0.0,
            ram_mb: 0,
        });
    }
    (values, process_count, cpu, codex_active, claude_active)
}

fn memory_usage() -> (u8, f32, f32) {
    let text = command_output("memory_pressure", &[]);
    let percent = text
        .lines()
        .find_map(|line| line.strip_prefix("System-wide memory free percentage: "))
        .and_then(|v| v.trim_end_matches('%').parse::<u8>().ok())
        .map_or(0, |free| 100u8.saturating_sub(free));
    let total_bytes = command_output("sysctl", &["-n", "hw.memsize"])
        .trim()
        .parse::<f32>()
        .unwrap_or(0.0);
    let total_gb = total_bytes / 1_073_741_824.0;
    (percent, total_gb * percent as f32 / 100.0, total_gb)
}

fn swap_usage() -> (u8, f32) {
    let text = command_output("sysctl", &["-n", "vm.swapusage"]);
    let number = |name: &str| {
        text.split_whitespace()
            .collect::<Vec<_>>()
            .windows(2)
            .find(|w| w[0] == name)
            .and_then(|w| w[1].trim_end_matches('M').parse::<f32>().ok())
    };
    match (number("total"), number("used")) {
        (Some(total), Some(used)) if total > 0.0 => (
            (used * 100.0 / total).clamp(0.0, 100.0) as u8,
            used / 1024.0,
        ),
        _ => (0, 0.0),
    }
}

fn battery_percent() -> u8 {
    let text = command_output("pmset", &["-g", "batt"]);
    text.split_whitespace()
        .find_map(|word| word.strip_suffix("%;"))
        .and_then(|value| value.parse().ok())
        .unwrap_or(0)
}

fn battery_power_watts() -> f32 {
    let text = command_output("ioreg", &["-rn", "AppleSmartBattery", "-l"]);
    let field = |name: &str| -> Option<u64> {
        let marker = format!("\"{name}\" = ");
        text.lines()
            .find_map(|line| line.trim().strip_prefix(&marker))?
            .trim()
            .parse()
            .ok()
    };
    let volts = field("Voltage").unwrap_or(0) as f64 / 1000.0;
    let amps_raw = field("InstantAmperage").unwrap_or(0);
    let milliamps = amps_raw as i64 as f64;
    (volts * milliamps.abs() / 1000.0) as f32
}

fn codex_usage(active: bool) -> AgentUsage {
    let root = home().join(".codex/sessions");
    let mut tokens = 0u64;
    let mut sessions = 0u8;
    let mut context = 0u8;
    let mut five_hour = "5H --".to_string();
    let mut weekly = "7D --".to_string();
    for path in recent_jsonl(&root) {
        sessions = sessions.saturating_add(1);
        let mut last_total = 0;
        for value in json_lines(&path) {
            if value.pointer("/payload/type").and_then(Value::as_str) != Some("token_count") {
                continue;
            }
            if let Some(total) = value
                .pointer("/payload/info/total_token_usage/total_tokens")
                .and_then(Value::as_u64)
            {
                last_total = total;
            }
            let used = value
                .pointer("/payload/info/last_token_usage/total_tokens")
                .and_then(Value::as_u64);
            let limit = value
                .pointer("/payload/info/model_context_window")
                .and_then(Value::as_u64);
            if let (Some(used), Some(limit)) = (used, limit) {
                context = ((used * 100 / limit.max(1)).min(100)) as u8;
            }
            if let Some(percent) = value
                .pointer("/payload/rate_limits/primary/used_percent")
                .and_then(Value::as_f64)
            {
                five_hour = format!("5H {:.0}%", percent);
            }
            if let Some(percent) = value
                .pointer("/payload/rate_limits/secondary/used_percent")
                .and_then(Value::as_f64)
            {
                weekly = format!("7D {:.0}%", percent);
            }
        }
        tokens = tokens.saturating_add(last_total);
    }
    AgentUsage {
        active,
        tokens_k: tokens / 1000,
        context,
        sessions,
        cost: 0.0,
        five_hour,
        weekly,
    }
}

fn claude_usage(active: bool) -> AgentUsage {
    let root = home().join(".claude/projects");
    let mut tokens = 0u64;
    let mut sessions = 0u8;
    let mut last_context = 0u64;
    for path in recent_jsonl(&root) {
        sessions = sessions.saturating_add(1);
        for value in json_lines(&path) {
            let usage = match value.pointer("/message/usage") {
                Some(v) => v,
                None => continue,
            };
            let input = usage
                .get("input_tokens")
                .and_then(Value::as_u64)
                .unwrap_or(0);
            let output = usage
                .get("output_tokens")
                .and_then(Value::as_u64)
                .unwrap_or(0);
            let cached = usage
                .get("cache_read_input_tokens")
                .and_then(Value::as_u64)
                .unwrap_or(0);
            let created = usage
                .get("cache_creation_input_tokens")
                .and_then(Value::as_u64)
                .unwrap_or(0);
            tokens = tokens.saturating_add(input + output + cached + created);
            last_context = input + cached + created;
        }
    }
    let (five_hour, weekly) = claude_ccusage_windows();
    AgentUsage {
        active,
        tokens_k: tokens / 1000,
        context: ((last_context * 100 / 200_000).min(100)) as u8,
        sessions,
        cost: 0.0,
        five_hour: format!("5H {}", compact_tokens(five_hour)),
        weekly: format!("7D {}", compact_tokens(weekly)),
    }
}

fn compact_tokens(tokens: u64) -> String {
    if tokens >= 1_000_000 {
        format!("{:.1}M", tokens as f64 / 1_000_000.0)
    } else if tokens >= 1_000 {
        format!("{}K", tokens / 1_000)
    } else {
        tokens.to_string()
    }
}

fn claude_ccusage_windows() -> (u64, u64) {
    static CACHE: OnceLock<Mutex<Option<(Instant, (u64, u64))>>> = OnceLock::new();
    let cache = CACHE.get_or_init(|| Mutex::new(None));
    let mut guard = cache
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner());
    if let Some((time, values)) = *guard {
        if time.elapsed() < Duration::from_secs(300) {
            return values;
        }
    }
    let run = |args: &[&str]| {
        let binary = if Path::new("/opt/homebrew/bin/npx").exists() {
            "/opt/homebrew/bin/npx"
        } else {
            "npx"
        };
        command_output(binary, args)
    };
    let blocks: Value = serde_json::from_str(&run(&[
        "--yes",
        "ccusage@latest",
        "claude",
        "blocks",
        "--json",
        "--offline",
        "--active",
    ]))
    .unwrap_or(Value::Null);
    let five_hour = blocks
        .get("blocks")
        .and_then(Value::as_array)
        .and_then(|items| {
            items
                .iter()
                .find(|item| item.get("isActive").and_then(Value::as_bool) == Some(true))
        })
        .and_then(|item| item.get("totalTokens"))
        .and_then(Value::as_u64)
        .unwrap_or(0);
    let report: Value = serde_json::from_str(&run(&[
        "--yes",
        "ccusage@latest",
        "claude",
        "weekly",
        "--json",
        "--offline",
    ]))
    .unwrap_or(Value::Null);
    let weekly = report
        .get("weekly")
        .and_then(Value::as_array)
        .and_then(|items| items.last())
        .and_then(|item| item.get("totalTokens"))
        .and_then(Value::as_u64)
        .unwrap_or(0);
    let values = (five_hour, weekly);
    *guard = Some((Instant::now(), values));
    values
}

fn any_process(processes: &[ProcessUsage], needles: &[&str]) -> bool {
    processes.iter().any(|p| {
        let name = p.name.to_lowercase();
        needles.iter().any(|needle| name.contains(needle))
    })
}

fn recent_jsonl(root: &Path) -> Vec<PathBuf> {
    let cutoff = SystemTime::now() - Duration::from_secs(24 * 60 * 60);
    let mut paths: Vec<_> = WalkDir::new(root)
        .into_iter()
        .filter_map(Result::ok)
        .filter(|entry| entry.path().extension().and_then(|e| e.to_str()) == Some("jsonl"))
        .filter(|entry| {
            entry
                .metadata()
                .ok()
                .and_then(|m| m.modified().ok())
                .is_some_and(|time| time >= cutoff)
        })
        .map(|entry| entry.into_path())
        .collect();
    paths.sort_by_key(|path| fs::metadata(path).and_then(|m| m.modified()).ok());
    paths
}

fn json_lines(path: &Path) -> Vec<Value> {
    fs::read_to_string(path)
        .unwrap_or_default()
        .lines()
        .filter_map(|line| serde_json::from_str(line).ok())
        .collect()
}

async fn refresh_online(config: &Config) -> OnlineData {
    let (weather_c, weather) = fetch_weather(&config.weather)
        .await
        .unwrap_or((0, "WEATHER UNAVAILABLE".into()));
    let (headlines, headline_ages) = fetch_headlines(&config.news)
        .await
        .unwrap_or_else(|_| (vec!["NEWS UNAVAILABLE".into(); 8], vec!["--".into(); 8]));
    OnlineData {
        weather_c,
        weather,
        headlines,
        headline_ages,
    }
}

async fn fetch_weather(config: &WeatherConfig) -> Result<(i32, String)> {
    let lat = config.latitude;
    let lon = config.longitude;
    let url = format!(
        "https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m,weather_code"
    );
    let value: Value = reqwest::get(url).await?.json().await?;
    let temp = value
        .pointer("/current/temperature_2m")
        .and_then(Value::as_f64)
        .context("weather temperature missing")?;
    let code = value
        .pointer("/current/weather_code")
        .and_then(Value::as_i64)
        .unwrap_or(-1);
    Ok((temp.round() as i32, weather_name(code).into()))
}

async fn fetch_headlines(config: &NewsConfig) -> Result<(Vec<String>, Vec<String>)> {
    let url = if config.rss_url.trim().is_empty() {
        format!(
            "https://news.google.com/rss/search?q={}&hl={}&gl={}&ceid={}",
            urlencoding::encode(&config.topic),
            config.language,
            config.country,
            config.edition
        )
    } else {
        config.rss_url.clone()
    };
    let bytes = reqwest::get(url).await?.bytes().await?;
    let channel = Channel::read_from(&bytes[..])?;
    let mut values = Vec::new();
    let mut ages = Vec::new();
    for item in channel.items().iter().take(8) {
        values.push(ascii(item.title().unwrap_or("Untitled"), 68));
        ages.push(relative_age(item.pub_date()));
    }
    while values.len() < 8 {
        values.push("-".into());
        ages.push("--".into());
    }
    Ok((values, ages))
}

fn relative_age(value: Option<&str>) -> String {
    let Some(date) = value.and_then(|s| DateTime::parse_from_rfc2822(s).ok()) else {
        return "--".into();
    };
    let minutes = (Utc::now() - date.with_timezone(&Utc)).num_minutes().max(0);
    if minutes < 60 {
        format!("{minutes}m")
    } else if minutes < 1440 {
        format!("{}h", minutes / 60)
    } else {
        format!("{}d", minutes / 1440)
    }
}

fn weather_name(code: i64) -> &'static str {
    match code {
        0 => "CLEAR",
        1..=3 => "PARTLY CLOUDY",
        45 | 48 => "FOG",
        51..=57 => "DRIZZLE",
        61..=67 | 80..=82 => "RAIN",
        71..=77 | 85 | 86 => "SNOW",
        95..=99 => "THUNDERSTORM",
        _ => "UNKNOWN",
    }
}

fn ascii(text: &str, limit: usize) -> String {
    let cleaned: String = text
        .chars()
        .map(|c| {
            if c.is_ascii_graphic() || c == ' ' {
                c
            } else {
                ' '
            }
        })
        .collect();
    cleaned
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .chars()
        .take(limit)
        .collect()
}

fn home() -> PathBuf {
    env::var_os("HOME").map(PathBuf::from).unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ascii_removes_unsupported_glyphs_and_limits_length() {
        assert_eq!(ascii("AI — café news", 9), "AI caf ne");
    }

    #[test]
    fn weather_codes_are_grouped_for_the_small_display() {
        assert_eq!(weather_name(0), "CLEAR");
        assert_eq!(weather_name(63), "RAIN");
        assert_eq!(weather_name(95), "THUNDERSTORM");
    }
}
