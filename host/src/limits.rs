//! Subscription limit windows: how much of a rate limit is used, and when it
//! resets. This is the figure a person actually watches, and unlike a token sum
//! it means the same thing for Codex and for Claude.
//!
//! Codex writes its windows into its session records. Claude Code passes its
//! windows to the status line command, which leaves them in a small file for
//! this collector; see the README for the one line that does it.

use serde_json::Value;
use std::path::Path;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// A limits file older than this says nothing about now: both windows are
/// shorter, so every figure in it has reset since it was written.
const MAX_FILE_AGE: Duration = Duration::from_secs(8 * 24 * 3600);

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Window {
    pub used_percent: f64,
    /// Unix seconds. Past this the window has started over.
    pub resets_at: Option<i64>,
}

pub struct ClaudeLimits {
    pub five_hour: Option<Window>,
    pub seven_day: Option<Window>,
    /// Context window use of the session that wrote the file.
    pub context_percent: Option<u8>,
    /// How long ago the status line last wrote the file.
    pub age: Duration,
}

pub fn now_epoch() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |elapsed| elapsed.as_secs() as i64)
}

/// Reads one window object. Claude Code spells the figure `used_percentage`,
/// Codex spells it `used_percent`; both carry `resets_at` in Unix seconds.
pub fn window_from(value: &Value) -> Option<Window> {
    let used_percent = value
        .get("used_percentage")
        .or_else(|| value.get("used_percent"))
        .and_then(Value::as_f64)?;
    Some(Window {
        used_percent,
        resets_at: value.get("resets_at").and_then(Value::as_i64),
    })
}

/// `5H 23%`, or `5H 0%` once the reset time has passed, or `5H --` when the
/// window is unknown. A recorded percentage outlives its window, and showing it
/// after the reset was the bug this replaces.
pub fn label(prefix: &str, window: Option<Window>, now: i64) -> String {
    match window {
        None => format!("{prefix} --"),
        Some(window) if window.resets_at.is_some_and(|at| at <= now) => format!("{prefix} 0%"),
        Some(window) => format!("{prefix} {:.0}%", window.used_percent.clamp(0.0, 999.0)),
    }
}

/// Pulls the windows out of a status line payload, whether the script wrote the
/// whole payload or only the parts that matter.
pub fn parse_claude(payload: &Value) -> (Option<Window>, Option<Window>, Option<u8>) {
    let five_hour = payload.pointer("/rate_limits/five_hour").and_then(window_from);
    let seven_day = payload.pointer("/rate_limits/seven_day").and_then(window_from);
    let context = payload
        .pointer("/context_window/used_percentage")
        .and_then(Value::as_f64)
        .map(|percent| percent.clamp(0.0, 100.0) as u8);
    (five_hour, seven_day, context)
}

/// None when the file is missing, unreadable, or too old to mean anything,
/// which sends the caller back to its token based fallback.
pub fn read_claude_limits(path: &Path) -> Option<ClaudeLimits> {
    let age = std::fs::metadata(path)
        .and_then(|meta| meta.modified())
        .ok()
        .and_then(|written| SystemTime::now().duration_since(written).ok())?;
    if age > MAX_FILE_AGE {
        return None;
    }
    let payload: Value = serde_json::from_str(&std::fs::read_to_string(path).ok()?).ok()?;
    let (five_hour, seven_day, context_percent) = parse_claude(&payload);
    Some(ClaudeLimits { five_hour, seven_day, context_percent, age })
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn label_shows_percent_then_zero_after_the_reset_and_dashes_when_unknown() {
        let window = Window { used_percent: 34.4, resets_at: Some(1_000) };
        assert_eq!(label("5H", Some(window), 999), "5H 34%");
        assert_eq!(label("5H", Some(window), 1_000), "5H 0%", "the window has started over");
        assert_eq!(label("7D", None, 0), "7D --");
        let no_reset = Window { used_percent: 5.0, resets_at: None };
        assert_eq!(label("7D", Some(no_reset), i64::MAX), "7D 5%", "no reset time, so trust it");
    }

    #[test]
    fn both_spellings_of_a_window_are_read() {
        let claude = json!({"used_percentage": 23.5, "resets_at": 1738425600});
        let codex = json!({"used_percent": 34.0, "window_minutes": 300, "resets_at": 1789889748});
        assert_eq!(window_from(&claude).unwrap().used_percent, 23.5);
        assert_eq!(window_from(&codex).unwrap().resets_at, Some(1789889748));
        assert!(window_from(&json!({"window_minutes": 300})).is_none());
    }

    #[test]
    fn status_line_payload_parses_whole_or_trimmed_and_survives_missing_limits() {
        let whole = json!({
            "model": {"display_name": "Opus"},
            "context_window": {"used_percentage": 8.7},
            "rate_limits": {
                "five_hour": {"used_percentage": 23.5, "resets_at": 1},
                "seven_day": {"used_percentage": 41.2, "resets_at": 2}
            }
        });
        let (five, seven, context) = parse_claude(&whole);
        assert_eq!(five.unwrap().used_percent, 23.5);
        assert_eq!(seven.unwrap().resets_at, Some(2));
        assert_eq!(context, Some(8));
        // Not a subscriber, or the first turn of a session: no windows yet.
        let bare = json!({"context_window": {"used_percentage": null}, "rate_limits": null});
        assert_eq!(parse_claude(&bare), (None, None, None));
    }
}
