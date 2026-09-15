use std::{
    env, fs,
    path::{Path, PathBuf},
    process::Command,
    sync::{
        Mutex, OnceLock,
        atomic::{AtomicBool, Ordering},
    },
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
use futures::StreamExt;
use tokio::time::{sleep, timeout};
use uuid::Uuid;
use walkdir::WalkDir;

mod alerts;
#[cfg(windows)]
mod tray;

use alerts::{ActiveAlert, AlertConfig, Scheduler, TodoConfig, TodoItem};

// The tray icon is Windows only; elsewhere these calls compile away and the
// launchd service already exposes the log.
#[cfg(windows)]
fn tray_status(text: impl Into<String>) { tray::set_status(text); }
#[cfg(not(windows))]
fn tray_status(_text: impl Into<String>) {}
#[cfg(windows)]
fn tray_quit_requested() -> bool { tray::quit_requested() }
#[cfg(not(windows))]
fn tray_quit_requested() -> bool { false }

const DEVICE_NAME: &str = "RLCD Monitor";
const TELEMETRY_UUID: &str = "7a1e0002-7e57-4d6d-8a1b-2c3d4e5f6071";
const BUTTON_UUID: &str = "7a1e0003-7e57-4d6d-8a1b-2c3d4e5f6071";
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
    // Not watts. Neither platform exposes per process power without elevated
    // access, so this is the same kind of composite Task Manager and Activity
    // Monitor show: processor time plus the disk traffic that keeps a drive and
    // the bus awake. The board labels it IMPACT for that reason.
    impact: f32,
}

// One megabyte per second of disk traffic counts for this much processor time.
// Pure guesswork calibrated to taste rather than to a power meter, which is why
// it sits here as a named constant instead of buried in the arithmetic.
const DISK_IMPACT_PER_MB: f32 = 2.0;

// The board sorts this set itself, so it has to contain every process that
// could top any of the three orderings, not just the busiest by processor.
const PROCESSES_PER_METRIC: usize = 5;
const PROCESSES_SENT: usize = 12;

#[derive(Serialize)]
struct Snapshot {
    v: u8,
    time: String,
    date: String,
    weather_c: i32,
    weather: String,
    region: String,
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
    net_down: f32,
    net_up: f32,
    noise_offset: f32,
    source: String,
    headlines: Vec<String>,
    headline_ages: Vec<String>,
    todos: Vec<TodoItem>,
    // Present only while an alert is ringing and not yet dismissed.
    alert: Option<ActiveAlert>,
}

#[derive(Clone)]
struct OnlineData {
    weather_c: i32,
    weather: String,
    region: String,
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
    buttons: ButtonConfig,
    // dB added to the measured microphone level to reach an approximate SPL.
    noise_offset_db: f32,
    // Name shown on the board. Empty uses this machine's host name.
    device_name: String,
    // How often to send while the board is showing another machine.
    hidden_update_seconds: u64,
    // Reminders the board rings, and the list it shows. See alerts.rs.
    alerts: Vec<AlertConfig>,
    todos: Vec<TodoConfig>,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            update_seconds: 15,
            online_refresh_minutes: 10,
            weather: WeatherConfig::default(),
            news: NewsConfig::default(),
            buttons: ButtonConfig::default(),
            noise_offset_db: 87.0,
            device_name: String::new(),
            hidden_update_seconds: 300,
            alerts: Vec::new(),
            todos: Vec::new(),
        }
    }
}

#[derive(Clone, Deserialize, PartialEq)]
#[serde(default)]
struct WeatherConfig {
    latitude: f64,
    longitude: f64,
    region: String,
}

impl Default for WeatherConfig {
    fn default() -> Self {
        Self {
            latitude: 39.9042,
            longitude: 116.4074,
            region: "BEIJING - CHINA".into(),
        }
    }
}

// Shell commands run when the board reports a button. Empty means ignore.
#[derive(Clone, Deserialize, PartialEq, Default)]
#[serde(default)]
struct ButtonConfig {
    key: String,
    key_hold: String,
    boot: String,
    boot_hold: String,
}

impl ButtonConfig {
    fn command_for(&self, event: u8) -> &str {
        match event {
            1 => &self.key,
            2 => &self.key_hold,
            3 => &self.boot,
            4 => &self.boot_hold,
            _ => "",
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
    // Its own thread: the tray needs a Windows message loop, which cannot share
    // one with the async runtime.
    #[cfg(windows)]
    {
        // Absolute, because the menu hands these to other programs whose working
        // directory is not this one.
        let here = env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let root = env::var_os("RLCD_CONFIG")
            .map(PathBuf::from)
            .and_then(|path| path.parent().map(PathBuf::from))
            .filter(|path| !path.as_os_str().is_empty())
            .map(|path| if path.is_absolute() { path } else { here.join(path) })
            .unwrap_or(here);
        let log = root.join("rlcd-host.log");
        let config = root.join("config.yaml");
        std::thread::spawn(move || tray::run(log, config));
    }
    let adapter = bluetooth_adapter().await?;
    let characteristic_uuid = Uuid::parse_str(TELEMETRY_UUID)?;
    let button_uuid = Uuid::parse_str(BUTTON_UUID)?;
    let mut message_id = 0u8;
    let mut config = load_config();
    let mut scheduler = Scheduler::new(config.alerts.len(), config.todos.len());
    let mut net = NetSampler::new();
    let mut online = refresh_online(&config).await;
    let mut online_updated = Instant::now();

    loop {
        match find_and_connect(&adapter, characteristic_uuid, button_uuid).await {
            Ok((peripheral, characteristic, button)) => {
                println!("Connected to {DEVICE_NAME}");
                tray_status("Connected");
                // Assume shown until the board says otherwise, so the first
                // snapshot after connecting is never delayed.
                SHOWN.store(true, Ordering::Relaxed);
                let listener = button.map(|b| spawn_button_listener(peripheral.clone(), b));
                loop {
                    if tray_quit_requested() {
                        println!("Quit requested from the tray");
                        let _ = timeout(Duration::from_secs(3), peripheral.disconnect()).await;
                        return Ok(());
                    }
                    let still_connected =
                        timeout(Duration::from_secs(2), peripheral.is_connected())
                            .await
                            .ok()
                            .and_then(Result::ok)
                            .unwrap_or(false);
                    if !still_connected {
                        println!("Board disconnected; scanning again");
                        tray_status("Board disconnected, scanning");
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
                    if let Ok(mut shared) = button_commands().lock() {
                        *shared = next_config.buttons.clone();
                    }
                    if next_config.alerts != config.alerts || next_config.todos != config.todos {
                        scheduler.reset_for(next_config.alerts.len(), next_config.todos.len());
                    }
                    config = next_config;
                    if ALERT_ACK.swap(false, Ordering::Relaxed) {
                        scheduler.dismiss();
                    }
                    if let Some(title) = scheduler.poll(&config.alerts, &config.todos, Local::now()) {
                        println!("Alert: {title}");
                    }
                    let todos = scheduler.todo_items(&config.alerts, &config.todos, Local::now());
                    let snapshot = collect_snapshot(
                        &online,
                        &mut net,
                        config.noise_offset_db,
                        &source_name(&config),
                        todos,
                        scheduler.active(),
                    );
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
                    tray_status(format!(
                        "Updated {:02}:{:02}:{:02}",
                        Local::now().hour(),
                        Local::now().minute(),
                        Local::now().second()
                    ));
                    println!(
                        "Updated {:02}:{:02}:{:02} ({} bytes)",
                        Local::now().hour(),
                        Local::now().minute(),
                        Local::now().second(),
                        json.len()
                    );
                    // Set RLCD_DUMP to see what was actually sent; the board
                    // gives no feedback, so this is the only way to tell real
                    // readings from zeros.
                    if env::var_os("RLCD_DUMP").is_some() {
                        println!("{}", String::from_utf8_lossy(&json));
                    }
                    // A machine the board is not showing only has to stay fresh
                    // enough to switch to, and the board wakes it the moment it
                    // becomes the shown one.
                    let interval = if SHOWN.load(Ordering::Relaxed) {
                        config.update_seconds.max(5)
                    } else {
                        config.hidden_update_seconds.max(config.update_seconds.max(5))
                    };
                    let waited = Instant::now();
                    loop {
                        let remaining = Duration::from_secs(interval).saturating_sub(waited.elapsed());
                        if remaining.is_zero() {
                            break;
                        }
                        let slice = remaining.min(scheduler.next_check());
                        tokio::select! {
                            _ = sleep(slice) => {}
                            _ = shown_wake().notified() => break,
                        }
                        // A dismissal or a fresh alert both deserve a snapshot now,
                        // not at the end of the interval.
                        if ALERT_ACK.load(Ordering::Relaxed) {
                            break;
                        }
                        if scheduler
                            .poll(&config.alerts, &config.todos, Local::now())
                            .map(|title| println!("Alert: {title}"))
                            .is_some()
                        {
                            break;
                        }
                    }
                }
                if let Some(listener) = listener {
                    listener.abort();
                }
            }
            Err(error) => {
                tray_status("Searching for the board");
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

// Connects and discovers, so a caller can treat a stale peripheral as one
// failure rather than three separate ones.
async fn attach(peripheral: &Peripheral) -> Result<()> {
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
    Ok(())
}

async fn find_and_connect(
    adapter: &Adapter,
    uuid: Uuid,
    button_uuid: Uuid,
) -> Result<(Peripheral, Characteristic, Option<Characteristic>)> {
    adapter.start_scan(ScanFilter::default()).await?;
    // Long enough to outlast a board reboot, since the operating system keeps
    // offering the stale peripheral for a while after one.
    for _ in 0..40 {
        for peripheral in adapter.peripherals().await? {
            let properties = timeout(Duration::from_secs(2), peripheral.properties())
                .await
                .context("Bluetooth properties timed out")??;
            let is_target = properties.and_then(|p| p.local_name).as_deref() == Some(DEVICE_NAME);
            if !is_target {
                continue;
            }
            // A failure here is usually a peripheral the operating system still
            // has cached from before the board rebooted. Connecting to that
            // stale handle fails with "Not connected" forever, so the handle is
            // dropped and the scan carries on rather than the error escaping to
            // the caller, which would only find the same cached entry again.
            if let Err(error) = attach(&peripheral).await {
                eprintln!("{DEVICE_NAME} found but not reachable: {error:#}; rescanning");
                let _ = timeout(Duration::from_secs(3), peripheral.disconnect()).await;
                continue;
            }
            adapter.stop_scan().await?;
            let characteristics = peripheral.characteristics();
            let characteristic = characteristics
                .iter()
                .find(|item| item.uuid == uuid)
                .cloned()
                .context("telemetry characteristic not found")?;
            let button = characteristics
                .iter()
                .find(|item| item.uuid == button_uuid)
                .cloned();
            return Ok((peripheral, characteristic, button));
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

fn collect_snapshot(
    online: &OnlineData,
    net: &mut NetSampler,
    noise_offset: f32,
    source: &str,
    todos: Vec<TodoItem>,
    alert: Option<ActiveAlert>,
) -> Snapshot {
    let (processes, process_count, cpu, codex_active, claude_active) = process_usage();
    let (ram, ram_used_gb, ram_total_gb) = memory_usage();
    let (swap, swap_used_gb) = swap_usage();
    let (net_down, net_up) = net.rate_kb_per_second();
    let now = Local::now();
    Snapshot {
        v: 1,
        time: now.format("%H:%M").to_string(),
        date: now.format("%a %d %b").to_string().to_uppercase(),
        weather_c: online.weather_c,
        weather: online.weather.clone(),
        region: online.region.clone(),
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
        net_down,
        net_up,
        noise_offset,
        source: source.to_string(),
        headlines: online.headlines.clone(),
        headline_ages: online.headline_ages.clone(),
        todos,
        alert,
    }
}

const EVENT_SHOWN: u8 = 5;
const EVENT_HIDDEN: u8 = 6;
const EVENT_ALERT_DISMISSED: u8 = 7;

// Set by the button listener, consumed by the collector loop, which then drops
// the alert from the next snapshot.
static ALERT_ACK: AtomicBool = AtomicBool::new(false);

// Whether the board is currently showing this machine. It says so over the
// button characteristic whenever the shown source changes.
static SHOWN: AtomicBool = AtomicBool::new(true);

fn shown_wake() -> &'static tokio::sync::Notify {
    static WAKE: OnceLock<tokio::sync::Notify> = OnceLock::new();
    WAKE.get_or_init(tokio::sync::Notify::new)
}

static BUTTON_COMMANDS: OnceLock<Mutex<ButtonConfig>> = OnceLock::new();

fn button_commands() -> &'static Mutex<ButtonConfig> {
    BUTTON_COMMANDS.get_or_init(|| Mutex::new(ButtonConfig::default()))
}

// The board only ever sends a fixed event code, so the command it maps to comes
// from local config and never from the device.
fn run_button_command(event: u8, page: u8) {
    let command = button_commands()
        .lock()
        .map(|c| c.command_for(event).to_string())
        .unwrap_or_default();
    if command.trim().is_empty() {
        return;
    }
    println!("Button {event} on page {page} runs: {command}");
    tokio::task::spawn_blocking(move || {
        let _ = shell_command()
            .arg(&command)
            .env("RLCD_EVENT", event.to_string())
            .env("RLCD_PAGE", page.to_string())
            .status();
    });
}

fn spawn_button_listener(
    peripheral: Peripheral,
    characteristic: Characteristic,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(async move {
        if let Err(error) = peripheral.subscribe(&characteristic).await {
            eprintln!("Button subscribe failed: {error:#}");
            return;
        }
        let mut stream = match peripheral.notifications().await {
            Ok(stream) => stream,
            Err(error) => {
                eprintln!("Button stream failed: {error:#}");
                return;
            }
        };
        while let Some(note) = stream.next().await {
            if note.uuid != characteristic.uuid || note.value.len() < 4 || note.value[0] != 1 {
                continue;
            }
            match note.value[2] {
                // Only a change from hidden needs an early send; a host that is
                // already shown would otherwise resend the snapshot it just sent.
                EVENT_SHOWN => {
                    if !SHOWN.swap(true, Ordering::Relaxed) {
                        shown_wake().notify_one();
                    }
                }
                EVENT_HIDDEN => SHOWN.store(false, Ordering::Relaxed),
                EVENT_ALERT_DISMISSED => ALERT_ACK.store(true, Ordering::Relaxed),
                event => run_button_command(event, note.value[3]),
            }
        }
    })
}

// Interface counters are cumulative, so a rate needs two samples. The window is
// one update interval, which averages out bursts rather than showing a peak.
struct NetSampler {
    last: Option<(u64, u64, Instant)>,
}

impl NetSampler {
    fn new() -> Self {
        Self { last: None }
    }

    fn rate_kb_per_second(&mut self) -> (f32, f32) {
        let (down, up) = interface_bytes();
        let now = Instant::now();
        let rate = match self.last {
            Some((previous_down, previous_up, when)) => {
                let seconds = now.duration_since(when).as_secs_f32().max(0.001);
                (
                    down.saturating_sub(previous_down) as f32 / seconds / 1024.0,
                    up.saturating_sub(previous_up) as f32 / seconds / 1024.0,
                )
            }
            None => (0.0, 0.0),
        };
        self.last = Some((down, up, now));
        rate
    }
}

#[cfg(target_os = "macos")]
fn interface_bytes() -> (u64, u64) {
    let mut down = 0;
    let mut up = 0;
    for line in command_output("netstat", &["-ibn"]).lines().skip(1) {
        let fields: Vec<&str> = line.split_whitespace().collect();
        // Only the <Link#> row of each interface carries counters once; the
        // address rows repeat them. Columns are counted from the right because
        // interfaces without a hardware address leave that column out.
        if fields.len() < 11 || !fields[0].starts_with("en") || !fields[2].starts_with("<Link#") {
            continue;
        }
        down += fields[fields.len() - 5].parse::<u64>().unwrap_or(0);
        up += fields[fields.len() - 2].parse::<u64>().unwrap_or(0);
    }
    (down, up)
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

#[cfg(target_os = "macos")]
fn process_usage() -> (Vec<ProcessUsage>, u16, u8, bool, bool) {
    let output = command_output("ps", &["-axo", "pcpu=,rss=,comm="]);
    let values: Vec<_> = output
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
                name: ascii(&name, 20),  // ps already reports a plain name here.
                cpu,
                ram_mb,
                // ps reports no disk traffic, so on macOS the impact ordering
                // collapses into the processor ordering until something richer
                // than ps is used here.
                impact: cpu,
            })
        })
        .collect();
    let core_count = std::thread::available_parallelism().map_or(1, usize::from) as f32;
    let cpu = (values.iter().map(|p| p.cpu).sum::<f32>() / core_count).clamp(0.0, 100.0) as u8;
    summarize_processes(values, cpu)
}

// The display fonts are ASCII only, so a process named in another script would
// arrive as nothing but its extension and several different apps would collapse
// into one row. Those fall back to the process id, which at least stays
// distinct and is honest about not knowing the name.
fn display_name(raw: &str, pid: u32) -> String {
    let cleaned = ascii(raw, 20);
    let stem = cleaned.strip_suffix(".exe").unwrap_or(&cleaned).trim();
    if stem.is_empty() {
        format!("PID {pid}")
    } else {
        stem.to_string()
    }
}

fn summarize_processes(
    values: Vec<ProcessUsage>,
    cpu: u8,
) -> (Vec<ProcessUsage>, u16, u8, bool, bool) {
    let codex_active = any_process(&values, &["codex", "chatgpt"]);
    let claude_active = any_process(&values, &["claude"]);
    let process_count = values.len().min(u16::MAX as usize) as u16;
    // Chrome and Claude run as many processes; three rows of the same name tell
    // the viewer nothing, so instances are added together under one name. The
    // totals are what a person means by "what is this app costing me".
    let mut merged: Vec<ProcessUsage> = Vec::new();
    for process in values {
        match merged.iter_mut().find(|held| held.name == process.name) {
            Some(held) => {
                held.cpu += process.cpu;
                held.ram_mb = held.ram_mb.saturating_add(process.ram_mb);
                held.impact += process.impact;
            }
            None => merged.push(process),
        }
    }
    let values = merged;
    let mut chosen: Vec<ProcessUsage> = Vec::new();
    // Take the leaders of each ordering in turn. A process that leads two of
    // them is already here, so it is added once and the set stays short.
    let metrics: [fn(&ProcessUsage) -> f32; 3] =
        [|p| p.cpu, |p| p.ram_mb as f32, |p| p.impact];
    for metric in metrics {
        let mut ranked: Vec<&ProcessUsage> = values.iter().collect();
        ranked.sort_by(|a, b| metric(b).total_cmp(&metric(a)));
        for process in ranked.into_iter().take(PROCESSES_PER_METRIC) {
            if chosen.len() >= PROCESSES_SENT {
                break;
            }
            if chosen.iter().any(|held| held.name == process.name) {
                continue;
            }
            chosen.push(process.clone());
        }
    }
    // The board starts every snapshot from placeholder data, so short rows are
    // padded rather than left for it to guess at.
    while chosen.len() < PROCESSES_PER_METRIC {
        chosen.push(ProcessUsage {
            name: "-".into(),
            cpu: 0.0,
            ram_mb: 0,
            impact: 0.0,
        });
    }
    (chosen, process_count, cpu, codex_active, claude_active)
}

#[cfg(target_os = "macos")]
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

#[cfg(target_os = "macos")]
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

#[cfg(target_os = "macos")]
fn battery_percent() -> u8 {
    let text = command_output("pmset", &["-g", "batt"]);
    text.split_whitespace()
        .find_map(|word| word.strip_suffix("%;"))
        .and_then(|value| value.parse().ok())
        .unwrap_or(0)
}

#[cfg(target_os = "macos")]
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

// Session files outlive a single day, so records carry their own time and the
// day boundary has to be applied per record rather than per file.
fn record_time(value: &Value) -> Option<DateTime<Utc>> {
    value
        .get("timestamp")
        .and_then(Value::as_str)
        .and_then(|text| DateTime::parse_from_rfc3339(text).ok())
        .map(|time| time.with_timezone(&Utc))
}

fn is_today(value: &Value, cutoff: DateTime<Utc>) -> bool {
    // A record with no usable timestamp is counted, since dropping it would
    // silently lose usage rather than merely dating it wrongly.
    record_time(value).map_or(true, |time| time >= cutoff)
}

fn start_of_today() -> DateTime<Utc> {
    Local::now()
        .date_naive()
        .and_hms_opt(0, 0, 0)
        .and_then(|naive| naive.and_local_timezone(Local).earliest())
        .map(|time| time.with_timezone(&Utc))
        .unwrap_or_else(Utc::now)
}

fn codex_usage(active: bool) -> AgentUsage {
    let root = home().join(".codex/sessions");
    let cutoff = start_of_today();
    let mut tokens = 0u64;
    let mut sessions = 0u8;
    let mut context = 0u8;
    let mut five_hour = "5H --".to_string();
    let mut weekly = "7D --".to_string();
    for path in recent_jsonl(&root, cutoff) {
        // total_token_usage is cumulative for the whole session, so today's
        // share is the growth since the last record written before midnight.
        let mut carried = 0u64;
        let mut last_total = 0u64;
        let mut active_today = false;
        for value in json_lines(&path) {
            if value.pointer("/payload/type").and_then(Value::as_str) != Some("token_count") {
                continue;
            }
            let today = is_today(&value, cutoff);
            if let Some(total) = value
                .pointer("/payload/info/total_token_usage/total_tokens")
                .and_then(Value::as_u64)
            {
                last_total = total;
                if !today {
                    carried = total;
                }
            }
            if !today {
                continue;
            }
            active_today = true;
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
        if active_today {
            sessions = sessions.saturating_add(1);
            tokens = tokens.saturating_add(last_total.saturating_sub(carried));
        }
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
    let cutoff = start_of_today();
    let mut sessions = 0u8;
    let mut last_context = 0u64;
    for path in recent_jsonl(&root, cutoff) {
        let mut active_today = false;
        for value in json_lines(&path) {
            let usage = match value.pointer("/message/usage") {
                Some(v) => v,
                None => continue,
            };
            if !is_today(&value, cutoff) {
                continue;
            }
            active_today = true;
            let input = usage
                .get("input_tokens")
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
            last_context = input + cached + created;
        }
        if active_today {
            sessions = sessions.saturating_add(1);
        }
    }
    // Today's total comes from ccusage rather than from summing these records.
    // Resuming a session copies earlier turns into the new file, so the same
    // usage record appears several times and a plain sum reads about twice the
    // real figure. ccusage drops the repeats by message and request id, and it
    // already supplies the two window strings shown beside this number.
    let (five_hour, weekly, today) = claude_ccusage_windows();
    AgentUsage {
        active,
        tokens_k: today / 1000,
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

// Returns the 5 hour window, the 7 day window, and today's total.
fn claude_ccusage_windows() -> (u64, u64, u64) {
    static CACHE: OnceLock<Mutex<Option<(Instant, (u64, u64, u64))>>> = OnceLock::new();
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
            if cfg!(windows) { "npx.cmd" } else { "npx" }
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
    // Match on the date rather than taking the last entry, which is the most
    // recent day with any usage and not necessarily today.
    let wanted = Local::now().format("%Y-%m-%d").to_string();
    let report: Value = serde_json::from_str(&run(&[
        "--yes",
        "ccusage@latest",
        "claude",
        "daily",
        "--json",
        "--offline",
    ]))
    .unwrap_or(Value::Null);
    let today = report
        .get("daily")
        .and_then(Value::as_array)
        .and_then(|items| {
            items
                .iter()
                .find(|item| item.get("date").and_then(Value::as_str) == Some(wanted.as_str()))
        })
        .and_then(|item| item.get("totalTokens"))
        .and_then(Value::as_u64)
        .unwrap_or(0);
    let values = (five_hour, weekly, today);
    *guard = Some((Instant::now(), values));
    values
}

fn any_process(processes: &[ProcessUsage], needles: &[&str]) -> bool {
    processes.iter().any(|p| {
        let name = p.name.to_lowercase();
        needles.iter().any(|needle| name.contains(needle))
    })
}

// A file last written before the cutoff cannot hold a record after it.
fn recent_jsonl(root: &Path, cutoff: DateTime<Utc>) -> Vec<PathBuf> {
    let cutoff = SystemTime::UNIX_EPOCH + Duration::from_secs(cutoff.timestamp().max(0) as u64);
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
        region: ascii(&config.weather.region.to_uppercase(), 34),
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

fn source_name(config: &Config) -> String {
    let name = if config.device_name.trim().is_empty() {
        sysinfo::System::host_name().unwrap_or_else(|| "HOST".into())
    } else {
        config.device_name.clone()
    };
    ascii(&name.trim_end_matches(".local").to_uppercase(), 16)
}

fn shell_command() -> Command {
    let (program, flag) = if cfg!(windows) { ("cmd", "/C") } else { ("sh", "-c") };
    let mut command = Command::new(program);
    command.arg(flag);
    command
}

#[cfg(not(target_os = "macos"))]
fn process_usage() -> (Vec<ProcessUsage>, u16, u8, bool, bool) {
    portable::process_usage()
}

#[cfg(not(target_os = "macos"))]
fn memory_usage() -> (u8, f32, f32) {
    portable::memory_usage()
}

#[cfg(not(target_os = "macos"))]
fn swap_usage() -> (u8, f32) {
    portable::swap_usage()
}

#[cfg(not(target_os = "macos"))]
fn interface_bytes() -> (u64, u64) {
    portable::interface_bytes()
}

// A desktop PC usually has no battery, and the laptop readings above come from
// macOS-only tools, so these report nothing elsewhere.
#[cfg(not(target_os = "macos"))]
fn battery_percent() -> u8 {
    0
}

#[cfg(not(target_os = "macos"))]
fn battery_power_watts() -> f32 {
    0.0
}

// Collection for machines without the macOS tools the functions above shell out
// to, such as the Windows PC. It is compiled on every platform so a Mac build
// still type-checks it.
#[cfg_attr(target_os = "macos", allow(dead_code))]
mod portable {
    use super::{ProcessUsage, summarize_processes};
    use std::sync::{Mutex, MutexGuard, OnceLock};
    use std::time::Instant;
    use sysinfo::{Networks, ProcessesToUpdate, System};

    // CPU usage is the difference between two refreshes, so the System has to
    // persist across snapshots rather than being rebuilt each time.
    fn system() -> MutexGuard<'static, System> {
        static SYSTEM: OnceLock<Mutex<System>> = OnceLock::new();
        SYSTEM
            .get_or_init(|| {
                // A percentage is the difference between two refreshes, so the
                // first one has nothing to compare against and reports every
                // process at zero and the machine at 100. Take that throwaway
                // baseline here rather than sending it to the board.
                let mut system = System::new();
                system.refresh_cpu_usage();
                system.refresh_processes(ProcessesToUpdate::All, true);
                std::thread::sleep(sysinfo::MINIMUM_CPU_UPDATE_INTERVAL);
                Mutex::new(system)
            })
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }

    pub fn process_usage() -> (Vec<ProcessUsage>, u16, u8, bool, bool) {
        let mut system = system();
        system.refresh_cpu_usage();
        system.refresh_processes(ProcessesToUpdate::All, true);
        // disk_usage reports the bytes moved since the previous refresh, so a
        // rate needs the length of that gap rather than a fixed interval.
        // On the first refresh disk_usage reports everything the process has
        // ever moved, not a delta, so there is no rate to be had and the disk
        // term is left out rather than dividing a lifetime total by a moment.
        let elapsed: Option<f32> = {
            static LAST: OnceLock<Mutex<Option<Instant>>> = OnceLock::new();
            let mut last = LAST
                .get_or_init(|| Mutex::new(None))
                .lock()
                .unwrap_or_else(|poisoned| poisoned.into_inner());
            let now = Instant::now();
            let gap = last.map(|then| now.duration_since(then).as_secs_f32().max(0.001));
            *last = Some(now);
            gap
        };
        let values = system
            .processes()
            .iter()
            .map(|(pid, process)| {
                let cpu = process.cpu_usage();
                let disk_term = match elapsed {
                    Some(seconds) => {
                        let disk = process.disk_usage();
                        (disk.read_bytes + disk.written_bytes) as f32 / 1_048_576.0 / seconds
                            * super::DISK_IMPACT_PER_MB
                    }
                    None => 0.0,
                };
                ProcessUsage {
                    name: super::display_name(&process.name().to_string_lossy(), pid.as_u32()),
                    // Measured against a single core, matching ps on macOS.
                    cpu,
                    ram_mb: (process.memory() / 1_048_576) as u32,
                    impact: cpu + disk_term,
                }
            })
            .collect();
        let cpu = system.global_cpu_usage().clamp(0.0, 100.0) as u8;
        summarize_processes(values, cpu)
    }

    pub fn memory_usage() -> (u8, f32, f32) {
        let mut system = system();
        system.refresh_memory();
        let total = system.total_memory() as f32;
        let used = system.used_memory() as f32;
        let percent = if total > 0.0 {
            (used * 100.0 / total).clamp(0.0, 100.0) as u8
        } else {
            0
        };
        (percent, used / 1_073_741_824.0, total / 1_073_741_824.0)
    }

    pub fn swap_usage() -> (u8, f32) {
        let mut system = system();
        system.refresh_memory();
        let total = system.total_swap() as f32;
        let used = system.used_swap() as f32;
        if total <= 0.0 {
            return (0, 0.0);
        }
        (
            (used * 100.0 / total).clamp(0.0, 100.0) as u8,
            used / 1_073_741_824.0,
        )
    }

    pub fn interface_bytes() -> (u64, u64) {
        static NETWORKS: OnceLock<Mutex<Networks>> = OnceLock::new();
        let mut networks = NETWORKS
            .get_or_init(|| Mutex::new(Networks::new_with_refreshed_list()))
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner());
        networks.refresh(true);
        networks
            .list()
            .iter()
            .filter(|(name, _)| !name.to_lowercase().contains("loopback"))
            .fold((0, 0), |(down, up), (_, data)| {
                (down + data.total_received(), up + data.total_transmitted())
            })
    }
}

fn home() -> PathBuf {
    env::var_os("HOME")
        .or_else(|| env::var_os("USERPROFILE"))
        .map(PathBuf::from)
        .unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ascii_removes_unsupported_glyphs_and_limits_length() {
        assert_eq!(ascii("AI — café news", 9), "AI caf ne");
    }

    #[test]
    fn records_outside_today_are_excluded() {
        let cutoff = DateTime::parse_from_rfc3339("2026-09-12T00:00:00Z")
            .unwrap()
            .with_timezone(&Utc);
        let yesterday = serde_json::json!({"timestamp": "2026-09-11T23:59:00Z"});
        let today = serde_json::json!({"timestamp": "2026-09-12T08:00:00Z"});
        let undated = serde_json::json!({"message": {}});
        assert!(!is_today(&yesterday, cutoff));
        assert!(is_today(&today, cutoff));
        assert!(is_today(&undated, cutoff));
    }

    #[test]
    fn weather_codes_are_grouped_for_the_small_display() {
        assert_eq!(weather_name(0), "CLEAR");
        assert_eq!(weather_name(63), "RAIN");
        assert_eq!(weather_name(95), "THUNDERSTORM");
    }
}
