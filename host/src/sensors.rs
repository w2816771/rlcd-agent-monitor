//! Hardware readings the board cannot get from `sysinfo`: fan speeds and
//! temperatures from a file the elevated sensor task writes, and GPU figures
//! from `nvidia-smi`. Everything here degrades to "unknown" rather than to a
//! wrong number, and the board shows unknown as N/A, never as zero.

use serde_json::Value;
use std::path::Path;
use std::time::{Duration, SystemTime};

/// A sensor file older than this is a sensor task that has stopped; its last
/// reading would otherwise sit on the board looking current.
const MAX_SENSOR_AGE: Duration = Duration::from_secs(60);

#[derive(Clone, Debug, Default, PartialEq)]
pub struct BoardSensors {
    /// One entry per fan the firmware reports; `None` where it gave no value.
    pub fan_rpm: Vec<Option<u32>>,
    pub cpu_temp_c: Option<f32>,
    pub gpu_temp_c: Option<f32>,
}

/// Parses the record `sensors.ps1` writes. Public so the shape is tested.
pub fn parse_sensors(payload: &Value) -> BoardSensors {
    let fan_rpm = payload
        .get("fan_rpm")
        .and_then(Value::as_array)
        .map(|fans| {
            fans.iter()
                .map(|fan| fan.as_u64().map(|rpm| rpm.min(u32::MAX as u64) as u32))
                .collect()
        })
        .unwrap_or_default();
    let temperature = |key: &str| payload.get(key).and_then(Value::as_f64).map(|t| t as f32);
    BoardSensors {
        fan_rpm,
        cpu_temp_c: temperature("cpu_temp_c"),
        gpu_temp_c: temperature("gpu_temp_c"),
    }
}

/// `None` when the file is missing, unreadable or stale, which the caller
/// treats exactly like a machine with no sensor task.
pub fn read_sensors(path: &Path) -> Option<BoardSensors> {
    let age = std::fs::metadata(path)
        .and_then(|meta| meta.modified())
        .ok()
        .and_then(|written| SystemTime::now().duration_since(written).ok())?;
    if age > MAX_SENSOR_AGE {
        return None;
    }
    let payload: Value = serde_json::from_str(&std::fs::read_to_string(path).ok()?).ok()?;
    Some(parse_sensors(&payload))
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct GpuReading {
    pub utilization_percent: u8,
    pub memory_used_mb: u32,
    pub memory_total_mb: u32,
    pub temperature_c: Option<f32>,
    pub power_w: Option<f32>,
}

/// The query that produces one line of five comma separated numbers.
pub const NVIDIA_SMI_ARGS: [&str; 2] = [
    "--query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw",
    "--format=csv,noheader,nounits",
];

/// Parses `13, 2315, 4096, 62, 7.26`. A field the driver cannot read comes
/// back as `[N/A]`, which parses to unknown rather than failing the line.
pub fn parse_nvidia_smi(output: &str) -> Option<GpuReading> {
    let line = output.lines().find(|line| !line.trim().is_empty())?;
    let fields: Vec<&str> = line.split(',').map(str::trim).collect();
    if fields.len() < 5 {
        return None;
    }
    let number = |field: &str| field.parse::<f32>().ok();
    Some(GpuReading {
        utilization_percent: number(fields[0])?.clamp(0.0, 100.0) as u8,
        memory_used_mb: number(fields[1]).unwrap_or(0.0).max(0.0) as u32,
        memory_total_mb: number(fields[2]).unwrap_or(0.0).max(0.0) as u32,
        temperature_c: number(fields[3]),
        power_w: number(fields[4]),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn sensor_record_keeps_nulls_as_unknown_not_zero() {
        let full = json!({"fan_rpm": [3800, 3800], "cpu_temp_c": 71, "gpu_temp_c": 62});
        let parsed = parse_sensors(&full);
        assert_eq!(parsed.fan_rpm, vec![Some(3800), Some(3800)]);
        assert_eq!(parsed.cpu_temp_c, Some(71.0));
        let partial = json!({"fan_rpm": [3800, null], "cpu_temp_c": null});
        let parsed = parse_sensors(&partial);
        assert_eq!(parsed.fan_rpm, vec![Some(3800), None], "a fan without a value stays unknown");
        assert_eq!(parsed.cpu_temp_c, None);
        assert_eq!(parse_sensors(&json!({"source": "none", "fan_rpm": []})), BoardSensors::default());
    }

    #[test]
    fn nvidia_smi_line_parses_and_tolerates_missing_fields() {
        let gpu = parse_nvidia_smi("13, 2315, 4096, 62, 7.26\n").unwrap();
        assert_eq!(gpu.utilization_percent, 13);
        assert_eq!(gpu.memory_used_mb, 2315);
        assert_eq!(gpu.memory_total_mb, 4096);
        assert_eq!(gpu.temperature_c, Some(62.0));
        assert_eq!(gpu.power_w, Some(7.26));
        let no_power = parse_nvidia_smi("100, 4096, 4096, 80, [N/A]").unwrap();
        assert_eq!(no_power.utilization_percent, 100);
        assert_eq!(no_power.power_w, None);
        assert!(parse_nvidia_smi("").is_none(), "no driver, no reading");
        assert!(parse_nvidia_smi("NVIDIA-SMI has failed").is_none());
    }
}
