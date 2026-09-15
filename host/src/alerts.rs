//! Scheduled alerts and todo deadlines, both read from config.yaml.
//!
//! The host owns this because it has a clock and the file; the board only
//! receives "this alert is ringing" in the snapshot and reports the button
//! press that dismissed it. Three trigger kinds: a fixed time of day, a
//! repeating interval, and a countdown from when the host started.

use chrono::{DateTime, Datelike, Local, NaiveDate, NaiveDateTime, NaiveTime, Weekday};
use serde::{Deserialize, Serialize};
use std::time::Duration;

#[derive(Clone, Deserialize, PartialEq, Default)]
#[serde(default)]
pub struct AlertConfig {
    pub title: String,
    /// `HH:MM`, fires once a day at that time.
    pub at: String,
    /// Days for `at`, like `mon` or `weekdays`; empty means every day.
    pub days: Vec<String>,
    /// `45m`, `2h`, `1h30m`: fires repeatedly, first time one interval after start.
    pub every: String,
    /// `09:00-18:00`: `every` only fires inside this window.
    pub between: String,
    /// `25m`: fires once, that long after the host started or `from`.
    pub countdown: String,
    /// `HH:MM` today as the countdown start instead of host start.
    pub from: String,
}

#[derive(Clone, Deserialize, PartialEq, Default)]
#[serde(default)]
pub struct TodoConfig {
    pub text: String,
    /// `YYYY-MM-DD HH:MM`, or `HH:MM` for today. Empty means no deadline.
    pub due: String,
}

/// One row on the board's todo page.
#[derive(Clone, Serialize)]
pub struct TodoItem {
    pub text: String,
    /// Minutes until due, negative once overdue, null with no deadline.
    pub due_min: Option<i64>,
}

/// The alert currently ringing, carried in every snapshot until dismissed.
#[derive(Clone, Serialize)]
pub struct ActiveAlert {
    pub id: u16,
    pub title: String,
    /// Seconds since it fired, so a board that reboots can ignore a stale one.
    pub age_s: u64,
}

/// Parses `1h30m`, `45m`, `90s`, `2h`. Bare numbers are minutes.
pub fn parse_duration(text: &str) -> Option<Duration> {
    let text = text.trim();
    if text.is_empty() {
        return None;
    }
    if let Ok(minutes) = text.parse::<u64>() {
        return Some(Duration::from_secs(minutes * 60));
    }
    let mut total = 0u64;
    let mut number = 0u64;
    let mut saw_unit = false;
    for c in text.chars() {
        match c {
            '0'..='9' => number = number * 10 + (c as u64 - '0' as u64),
            'h' => {
                total += number * 3600;
                number = 0;
                saw_unit = true;
            }
            'm' => {
                total += number * 60;
                number = 0;
                saw_unit = true;
            }
            's' => {
                total += number;
                number = 0;
                saw_unit = true;
            }
            ' ' => {}
            _ => return None,
        }
    }
    if !saw_unit || number != 0 || total == 0 {
        return None;
    }
    Some(Duration::from_secs(total))
}

fn parse_clock(text: &str) -> Option<NaiveTime> {
    NaiveTime::parse_from_str(text.trim(), "%H:%M").ok()
}

/// `YYYY-MM-DD HH:MM`, or `HH:MM` meaning today.
fn parse_due(text: &str, today: NaiveDate) -> Option<NaiveDateTime> {
    let text = text.trim();
    if let Ok(full) = NaiveDateTime::parse_from_str(text, "%Y-%m-%d %H:%M") {
        return Some(full);
    }
    parse_clock(text).map(|time| today.and_time(time))
}

fn day_matches(days: &[String], weekday: Weekday) -> bool {
    if days.is_empty() {
        return true;
    }
    let short = weekday.to_string().to_lowercase();
    days.iter().any(|day| {
        let day = day.trim().to_lowercase();
        match day.as_str() {
            "weekdays" => weekday.number_from_monday() <= 5,
            "weekends" => weekday.number_from_monday() >= 6,
            "daily" | "every" | "all" => true,
            _ => day.starts_with(&short[..3]),
        }
    })
}

fn in_window(window: &str, now: NaiveTime) -> bool {
    let Some((start, end)) = window.split_once('-') else {
        return true;
    };
    match (parse_clock(start), parse_clock(end)) {
        (Some(start), Some(end)) if start <= end => now >= start && now < end,
        // A window across midnight, like 22:00-06:00.
        (Some(start), Some(end)) => now >= start || now < end,
        _ => true,
    }
}

/// A time-of-day trigger fires in the two minutes after its time, so a host
/// started at noon does not ring every 09:00 it missed.
fn late_grace() -> chrono::Duration {
    chrono::Duration::minutes(2)
}

fn span(duration: Duration) -> chrono::Duration {
    chrono::Duration::from_std(duration).unwrap_or_else(|_| chrono::Duration::zero())
}

#[derive(Clone, Default)]
struct AlertState {
    /// Local date the `at` trigger last fired, so it fires once a day.
    fired_on: Option<NaiveDate>,
    /// When `every` last fired; the host start before the first time.
    last: Option<DateTime<Local>>,
    /// Countdown fired already.
    done: bool,
}

pub struct Scheduler {
    started: DateTime<Local>,
    states: Vec<AlertState>,
    todo_fired: Vec<bool>,
    next_id: u16,
    active: Option<(ActiveAlert, std::time::Instant)>,
}

impl Scheduler {
    pub fn new(alerts: usize, todos: usize) -> Self {
        Self {
            started: Local::now(),
            states: vec![AlertState::default(); alerts],
            todo_fired: vec![false; todos],
            next_id: 0,
            active: None,
        }
    }

    /// Rebuilt when the config changes, which is also how a countdown restarts:
    /// edit the file and it starts over from now.
    pub fn reset_for(&mut self, alerts: usize, todos: usize) {
        self.started = Local::now();
        self.states = vec![AlertState::default(); alerts];
        self.todo_fired = vec![false; todos];
    }

    fn countdown_start(&self, alert: &AlertConfig, today: NaiveDate) -> DateTime<Local> {
        parse_clock(&alert.from)
            .and_then(|time| today.and_time(time).and_local_timezone(Local).single())
            .unwrap_or(self.started)
    }

    /// Checks every trigger against `now`. Returns the title of one that fired,
    /// which becomes the active alert until the board reports it dismissed.
    pub fn poll(
        &mut self,
        alerts: &[AlertConfig],
        todos: &[TodoConfig],
        now: DateTime<Local>,
    ) -> Option<String> {
        let today = now.date_naive();
        let clock = now.time();
        let started = self.started;
        let mut fired: Option<String> = None;
        for (index, alert) in alerts.iter().enumerate() {
            let Some(state) = self.states.get_mut(index) else { break };
            if let Some(at) = parse_clock(&alert.at) {
                let late = now.naive_local() - today.and_time(at);
                if state.fired_on != Some(today)
                    && day_matches(&alert.days, now.weekday())
                    && late >= chrono::Duration::zero()
                    && late < late_grace()
                {
                    state.fired_on = Some(today);
                    fired = Some(alert.title.clone());
                    break;
                }
            }
            if let Some(every) = parse_duration(&alert.every) {
                let reference = state.last.unwrap_or(started);
                if now - reference >= span(every) {
                    state.last = Some(now);
                    if in_window(&alert.between, clock) {
                        fired = Some(alert.title.clone());
                        break;
                    }
                }
            }
            if let Some(countdown) = parse_duration(&alert.countdown) {
                if !state.done {
                    let from = parse_clock(&alert.from)
                        .and_then(|time| today.and_time(time).and_local_timezone(Local).single())
                        .unwrap_or(started);
                    if now - from >= span(countdown) {
                        state.done = true;
                        fired = Some(alert.title.clone());
                        break;
                    }
                }
            }
        }
        if fired.is_none() {
            for (index, todo) in todos.iter().enumerate() {
                let Some(due) = parse_due(&todo.due, today) else { continue };
                let late = now.naive_local() - due;
                let already = self.todo_fired.get(index).copied().unwrap_or(true);
                if !already && late >= chrono::Duration::zero() && late < late_grace() {
                    self.todo_fired[index] = true;
                    fired = Some(format!("DUE: {}", todo.text));
                    break;
                }
            }
        }
        fired.map(|title| self.activate(&title))
    }

    fn activate(&mut self, title: &str) -> String {
        self.next_id = self.next_id.wrapping_add(1).max(1);
        let alert = ActiveAlert { id: self.next_id, title: title.to_string(), age_s: 0 };
        self.active = Some((alert, std::time::Instant::now()));
        title.to_string()
    }

    /// The board pressed a button while the overlay was up.
    pub fn dismiss(&mut self) {
        self.active = None;
    }

    pub fn active(&self) -> Option<ActiveAlert> {
        self.active.as_ref().map(|(alert, since)| ActiveAlert {
            age_s: since.elapsed().as_secs(),
            ..alert.clone()
        })
    }

    /// Rows for the todo page: every todo, then any countdown still running, so
    /// "counting down" is visible before it rings.
    pub fn todo_items(
        &self,
        alerts: &[AlertConfig],
        todos: &[TodoConfig],
        now: DateTime<Local>,
    ) -> Vec<TodoItem> {
        let today = now.date_naive();
        let mut items: Vec<TodoItem> = todos
            .iter()
            .map(|todo| TodoItem {
                text: todo.text.clone(),
                due_min: parse_due(&todo.due, today).map(|due| (due - now.naive_local()).num_minutes()),
            })
            .collect();
        for (index, alert) in alerts.iter().enumerate() {
            let Some(countdown) = parse_duration(&alert.countdown) else { continue };
            if self.states.get(index).is_some_and(|state| state.done) {
                continue;
            }
            let ends = self.countdown_start(alert, today) + span(countdown);
            items.push(TodoItem { text: alert.title.clone(), due_min: Some((ends - now).num_minutes()) });
        }
        items
    }

    /// How often the main loop should look again. Coarse on purpose: a check
    /// every 20 seconds keeps alerts inside their grace window without the loop
    /// spinning, and a fired alert is sent the moment it is noticed.
    pub fn next_check(&self) -> Duration {
        Duration::from_secs(20)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use chrono::TimeZone;

    fn at(h: u32, m: u32) -> DateTime<Local> {
        Local.with_ymd_and_hms(2026, 9, 15, h, m, 0).unwrap()
    }

    #[test]
    fn durations_parse_in_the_forms_people_write() {
        assert_eq!(parse_duration("45m"), Some(Duration::from_secs(2700)));
        assert_eq!(parse_duration("1h30m"), Some(Duration::from_secs(5400)));
        assert_eq!(parse_duration("90s"), Some(Duration::from_secs(90)));
        assert_eq!(parse_duration("25"), Some(Duration::from_secs(1500)));
        assert_eq!(parse_duration(""), None);
        assert_eq!(parse_duration("soon"), None);
    }

    #[test]
    fn time_of_day_fires_once_and_not_hours_late() {
        let alerts = vec![AlertConfig { title: "standup".into(), at: "09:30".into(), ..Default::default() }];
        let mut s = Scheduler::new(1, 0);
        assert_eq!(s.poll(&alerts, &[], at(9, 29)), None);
        assert_eq!(s.poll(&alerts, &[], at(9, 30)).as_deref(), Some("standup"));
        assert_eq!(s.poll(&alerts, &[], at(9, 31)), None, "already fired today");
        let mut late = Scheduler::new(1, 0);
        assert_eq!(late.poll(&alerts, &[], at(12, 0)), None, "missed by hours, stays quiet");
    }

    #[test]
    fn interval_waits_one_period_then_repeats_inside_its_window() {
        let alerts = vec![AlertConfig {
            title: "stretch".into(),
            every: "45m".into(),
            between: "09:00-18:00".into(),
            ..Default::default()
        }];
        let mut s = Scheduler::new(1, 0);
        s.started = at(10, 0);
        assert_eq!(s.poll(&alerts, &[], at(10, 30)), None);
        assert_eq!(s.poll(&alerts, &[], at(10, 45)).as_deref(), Some("stretch"));
        assert_eq!(s.poll(&alerts, &[], at(11, 0)), None);
        let mut night = Scheduler::new(1, 0);
        night.started = at(20, 0);
        assert_eq!(night.poll(&alerts, &[], at(20, 45)), None, "outside the window");
    }

    #[test]
    fn todo_deadline_counts_down_then_rings_once() {
        let todos = vec![TodoConfig { text: "ship it".into(), due: "2026-09-15 18:00".into() }];
        let mut s = Scheduler::new(0, 1);
        assert_eq!(s.todo_items(&[], &todos, at(17, 15))[0].due_min, Some(45));
        assert_eq!(s.poll(&[], &todos, at(17, 59)), None);
        assert_eq!(s.poll(&[], &todos, at(18, 0)).as_deref(), Some("DUE: ship it"));
        assert_eq!(s.poll(&[], &todos, at(18, 1)), None);
        assert_eq!(s.todo_items(&[], &todos, at(18, 30))[0].due_min, Some(-30));
    }

    #[test]
    fn countdown_shows_on_the_todo_page_then_rings_and_disappears() {
        let alerts = vec![AlertConfig { title: "pomodoro".into(), countdown: "25m".into(), ..Default::default() }];
        let mut s = Scheduler::new(1, 0);
        s.started = at(14, 0);
        let rows = s.todo_items(&alerts, &[], at(14, 10));
        assert_eq!(rows.len(), 1);
        assert_eq!(rows[0].due_min, Some(15));
        assert_eq!(s.poll(&alerts, &[], at(14, 25)).as_deref(), Some("pomodoro"));
        assert!(s.todo_items(&alerts, &[], at(14, 26)).is_empty(), "gone once it has rung");
    }

    #[test]
    fn dismiss_clears_the_active_alert() {
        let mut s = Scheduler::new(0, 0);
        s.activate("x");
        assert!(s.active().is_some());
        s.dismiss();
        assert!(s.active().is_none());
    }
}
