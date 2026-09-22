//! Loopback-only settings, embedded in the Windows executable.
use super::*;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use windows_sys::Win32::Security::Cryptography::{BCryptGenRandom, BCRYPT_USE_SYSTEM_PREFERRED_RNG};
use windows_sys::Win32::Storage::FileSystem::{MoveFileExW, MOVEFILE_REPLACE_EXISTING, MOVEFILE_WRITE_THROUGH};

const HTML: &str = include_str!("settings.html");
const LIMIT: usize = 64 * 1024;

pub fn start(path: PathBuf) -> Result<String> {
    let listener = TcpListener::bind(("127.0.0.1", 17832))
        .or_else(|_| TcpListener::bind(("127.0.0.1", 0)))?;
    let host = listener.local_addr()?.to_string();
    let mut random = [0u8; 32];
    // SAFETY: valid writable buffer; the system provider needs no algorithm handle.
    if unsafe { BCryptGenRandom(std::ptr::null_mut(), random.as_mut_ptr(), 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG) } < 0 {
        bail!("Cannot generate settings token");
    }
    let token: String = random.iter().map(|b| format!("{b:02x}")).collect();
    let url = format!("http://{host}/#{token}");
    // This local shortcut has the same access boundary as config.yaml.
    fs::write(path.with_file_name("settings.url"), format!("[InternetShortcut]\r\nURL={url}\r\n"))?;
    println!("Settings listening on http://{host}/ (open from tray)");
    std::thread::spawn(move || {
        // ponytail: one local settings client at a time, 2s read deadline;
        // use an async HTTP server if this becomes a multi-user interface.
        for stream in listener.incoming().flatten() {
            if let Err(error) = handle(stream, &host, &token, &path) {
                eprintln!("Settings request: {error:#}");
            }
        }
    });
    Ok(url)
}

fn reply(stream: &mut TcpStream, code: u16, kind: &str, body: &str) -> Result<()> {
    write!(stream, "HTTP/1.1 {code} Response\r\nContent-Type: {kind}; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n\r\n{body}", body.len())?;
    Ok(())
}

fn handle(mut stream: TcpStream, host: &str, token: &str, path: &Path) -> Result<()> {
    stream.set_read_timeout(Some(Duration::from_secs(2)))?;
    stream.set_write_timeout(Some(Duration::from_secs(2)))?;
    let deadline = Instant::now() + Duration::from_secs(2);
    let mut buffer = Vec::new();
    let mut chunk = [0u8; 2048];
    let header_end = loop {
        if Instant::now() >= deadline || buffer.len() > 16384 {
            return reply(&mut stream, 413, "text/plain", "请求过大或超时");
        }
        let n = stream.read(&mut chunk)?;
        if n == 0 { return Ok(()); }
        buffer.extend_from_slice(&chunk[..n]);
        if let Some(i) = buffer.windows(4).position(|w| w == b"\r\n\r\n") { break i + 4; }
    };
    let mut headers = [httparse::EMPTY_HEADER; 32];
    let mut request = httparse::Request::new(&mut headers);
    if !matches!(request.parse(&buffer[..header_end]), Ok(httparse::Status::Complete(_))) {
        return reply(&mut stream, 400, "text/plain", "无效请求");
    }
    let header = |name: &str| request.headers.iter().find(|h| h.name.eq_ignore_ascii_case(name))
        .and_then(|h| std::str::from_utf8(h.value).ok());
    let origin = format!("http://{host}");
    if header("host") != Some(host) || header("origin").is_some_and(|s| s != origin)
        || header("sec-fetch-site").is_some_and(|s| !matches!(s, "same-origin" | "none")) {
        return reply(&mut stream, 403, "text/plain", "仅允许从本机设置页访问");
    }
    let method = request.method.unwrap_or("").to_string();
    let route = request.path.unwrap_or("").to_string();
    if method == "GET" && route == "/" { return reply(&mut stream, 200, "text/html", HTML); }
    if header("x-rlcd-token") != Some(token) {
        return reply(&mut stream, 403, "text/plain", "请从托盘重新打开设置页");
    }
    if method == "GET" && route == "/api/status" {
        return reply(&mut stream, 200, "application/json", &serde_json::json!({"status": tray::status()}).to_string());
    }
    if method == "GET" && route == "/api/config" {
        return match read_config(path) {
            Ok(value) => reply(&mut stream, 200, "application/json", &value.to_string()),
            Err(error) => reply(&mut stream, 422, "text/plain", &format!("无法读取配置：{error:#}")),
        };
    }
    if method != "PUT" || route != "/api/config" { return reply(&mut stream, 404, "text/plain", "未找到页面"); }
    if header("origin") != Some(origin.as_str()) || header("content-type") != Some("application/json")
        || header("transfer-encoding").is_some()
        || request.headers.iter().filter(|h| h.name.eq_ignore_ascii_case("content-length")).count() != 1 {
        return reply(&mut stream, 400, "text/plain", "无效保存请求");
    }
    let size = header("content-length").and_then(|s| s.parse::<usize>().ok()).unwrap_or(usize::MAX);
    if size > LIMIT { return reply(&mut stream, 413, "text/plain", "配置不能超过 64 KB"); }
    while buffer.len() < header_end + size {
        if Instant::now() >= deadline { return reply(&mut stream, 408, "text/plain", "请求超时"); }
        let n = stream.read(&mut chunk)?;
        if n == 0 { return Ok(()); }
        buffer.extend_from_slice(&chunk[..n]);
    }
    let payload = serde_json::from_slice::<Value>(&buffer[header_end..header_end + size]);
    let result = payload.map_err(anyhow::Error::from).and_then(|v| save_config(path, &v));
    match result {
        Ok(value) => reply(&mut stream, 200, "application/json", &value.to_string()),
        Err(error) => reply(&mut stream, 422, "text/plain", &error.to_string()),
    }
}

fn read_config(path: &Path) -> Result<Value> {
    let revision = fs::read_to_string(path)?;
    let typed: Config = serde_yaml::from_str(&revision)?;
    let mut config = serde_json::to_value(typed)?;
    let original: Value = serde_yaml::from_str(&revision)?;
    merge(&mut config, original);
    Ok(serde_json::json!({"config": config, "revision": revision}))
}

fn merge(base: &mut Value, overlay: Value) {
    if let (Some(a), Some(b)) = (base.as_object_mut(), overlay.as_object()) {
        for (key, value) in b { merge(a.entry(key.clone()).or_insert(Value::Null), value.clone()); }
    } else { *base = overlay; }
}

fn validate(value: &Value) -> Result<()> {
    let c: Config = serde_json::from_value(value.clone()).context("配置字段类型不正确")?;
    if !(1..=86400).contains(&c.update_seconds) || !(1..=86400).contains(&c.hidden_update_seconds)
        || !(1..=1440).contains(&c.online_refresh_minutes) { bail!("刷新间隔超出允许范围"); }
    if !(-90.0..=90.0).contains(&c.weather.latitude) || !(-180.0..=180.0).contains(&c.weather.longitude)
        || !(-120.0..=120.0).contains(&c.noise_offset_db) { bail!("经纬度或噪声校准值超出范围"); }
    if !c.news.rss_url.is_empty() {
        let u = reqwest::Url::parse(&c.news.rss_url).context("RSS 地址不正确")?;
        if !matches!(u.scheme(), "http" | "https") { bail!("RSS 地址必须以 http 或 https 开头"); }
    }
    if c.alerts.len() > 100 || c.todos.len() > 100 { bail!("提醒和待办最多各 100 条"); }
    let clock = |s: &str| chrono::NaiveTime::parse_from_str(s, "%H:%M").is_ok();
    for a in c.alerts {
        if a.title.trim().is_empty() { bail!("提醒标题不能为空"); }
        if [&a.at, &a.every, &a.countdown].iter().filter(|s| !s.is_empty()).count() != 1 { bail!("每条提醒请选择一种触发方式"); }
        if (!a.at.is_empty() && !clock(&a.at)) || (!a.from.is_empty() && !clock(&a.from)) { bail!("提醒时间应为 HH:MM"); }
        for d in [&a.every, &a.countdown].into_iter().filter(|d| !d.is_empty()) {
            // Bound digits before the existing duration parser's arithmetic.
            if d.len() > 16 || d.split(|c: char| !c.is_ascii_digit()).any(|s| s.len() > 6)
                || alerts::parse_duration(d).is_none_or(|v| v.as_secs() == 0 || v.as_secs() > 31_536_000) { bail!("时长格式如 25m、1h30m，范围为 1 秒到 365 天"); }
        }
        if !a.between.is_empty() && !a.between.split_once('-').is_some_and(|(s,e)| clock(s) && clock(e)) { bail!("提醒时段格式为 09:00-18:00"); }
        if a.days.iter().any(|d| !["mon","tue","wed","thu","fri","sat","sun","weekdays","weekends","daily","every","all"].contains(&d.to_lowercase().as_str())) { bail!("提醒日期请使用 mon、weekdays、weekends 等"); }
    }
    for t in c.todos {
        if t.text.trim().is_empty() { bail!("待办内容不能为空"); }
        if !t.due.is_empty() && !clock(&t.due) && chrono::NaiveDateTime::parse_from_str(&t.due, "%Y-%m-%d %H:%M").is_err() { bail!("截止时间应为 HH:MM 或 YYYY-MM-DD HH:MM"); }
    }
    Ok(())
}

fn save_config(path: &Path, payload: &Value) -> Result<Value> {
    let config = payload.get("config").context("缺少配置")?;
    validate(config)?;
    let current = fs::read_to_string(path)?;
    if payload["revision"].as_str() != Some(current.as_str()) { bail!("配置已在别处修改，请重新载入后再保存"); }
    let yaml = serde_yaml::to_string(config)?;
    fs::write(path.with_extension("yaml.bak"), &current).context("备份失败，未修改配置")?;
    let temp = path.with_extension("yaml.tmp");
    let mut file = fs::File::create(&temp)?;
    file.write_all(yaml.as_bytes())?;
    file.sync_all()?;
    drop(file);
    use std::os::windows::ffi::OsStrExt;
    let from: Vec<u16> = temp.as_os_str().encode_wide().chain(Some(0)).collect();
    let to: Vec<u16> = path.as_os_str().encode_wide().chain(Some(0)).collect();
    // SAFETY: both paths are nul-terminated and live throughout the call.
    if unsafe { MoveFileExW(from.as_ptr(), to.as_ptr(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) } == 0 {
        return Err(std::io::Error::last_os_error().into());
    }
    read_config(path)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn saves_preserve_unknown_fields_and_reject_invalid_or_stale_edits() {
        let dir = env::temp_dir().join(format!("rlcd-web-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = dir.join("config.yaml");
        fs::write(&path, "# keep a backup\nupdate_seconds: 60\nfuture_option: yes\n").unwrap();
        let mut value = read_config(&path).unwrap();
        value["config"]["update_seconds"] = 20.into();
        let saved = save_config(&path, &value).unwrap();
        assert_eq!(saved["config"]["future_option"], "yes");
        assert!(save_config(&path, &value).is_err());
        let mut invalid = saved.clone();
        invalid["config"]["weather"]["latitude"] = 200.into();
        assert!(save_config(&path, &invalid).is_err());
        assert_eq!(read_config(&path).unwrap(), saved);
        assert!(fs::read_to_string(path.with_extension("yaml.bak")).unwrap().starts_with("# keep"));
        fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn http_rejects_foreign_origins_missing_tokens_and_oversized_writes() {
        let dir = env::temp_dir().join(format!("rlcd-http-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = dir.join("config.yaml");
        fs::write(&path, "update_seconds: 60\n").unwrap();
        let url = start(path).unwrap();
        let (base, token) = url.split_once("/#").unwrap();
        let host = base.strip_prefix("http://").unwrap();
        let send = |method: &str, route: &str, extra: &str| {
            let mut stream = TcpStream::connect(host).unwrap();
            stream.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
            write!(stream, "{method} {route} HTTP/1.1\r\nHost: {host}\r\n{extra}\r\n").unwrap();
            let mut response = String::new();
            stream.read_to_string(&mut response).unwrap();
            response
        };
        assert!(send("GET", "/", "").starts_with("HTTP/1.1 200"));
        assert!(send("GET", "/api/config", "").starts_with("HTTP/1.1 403"));
        assert!(send("GET", "/api/config", &format!("X-RLCD-Token: {token}\r\n")).contains("update_seconds"));
        assert!(send("PUT", "/api/config", &format!("X-RLCD-Token: {token}\r\nOrigin: https://example.com\r\n")).starts_with("HTTP/1.1 403"));
        assert!(send("PUT", "/api/config", &format!("X-RLCD-Token: {token}\r\nOrigin: {base}\r\nContent-Type: application/json\r\nContent-Length: 65537\r\n")).starts_with("HTTP/1.1 413"));
        fs::remove_dir_all(dir).unwrap();
    }
}
