#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "audio_meter.h"

struct AgentUsage {
    std::string name;
    bool active;
    uint32_t tokens_k;
    uint8_t context_percent;
    uint8_t sessions;
    float cost_usd;
    std::string five_hour_usage;
    std::string weekly_usage;
};

struct ProcessUsage {
    std::string name;
    float cpu_percent;
    uint16_t ram_mb;
    // Processor time plus disk traffic, not watts. The host cannot measure per
    // process power without elevated access, so this is a composite of the kind
    // Task Manager and Activity Monitor show.
    float impact = 0.0f;
};

// How the process rows are ordered. The host sends enough candidates that any
// of these is correct without asking it for a different list.
enum class ProcessSort : uint8_t { Cpu = 0, Ram = 1, Impact = 2 };
constexpr uint8_t kProcessSortCount = 3;

// The host sends the leaders of every ordering, deduplicated, so the set is
// larger than the rows on screen.
constexpr std::size_t kMaxProcesses = 12;

// One row of the todo page. The host computes the remaining minutes so the
// board needs no clock of its own.
struct TodoItem {
    std::string text;
    bool has_due = false;
    int32_t due_min = 0;  // Negative once overdue.
};
constexpr std::size_t kMaxTodos = 8;
constexpr std::size_t kMaxCores = 32;
constexpr std::size_t kMaxFans = 4;
// An alert older than this when it arrives is ignored: it is a host that kept
// sending one the board already dismissed before a reboot.
constexpr uint32_t kAlertMaxAgeSeconds = 1800;

constexpr std::size_t kMaxSources = 3;

// One connected or remembered host, for the devices page and the header tag.
struct SourceStatus {
    bool connected = false;
    bool shown = false;
    bool has_data = false;
    int8_t rssi = 0;
    uint32_t age_seconds = 0;
    std::string name;
};

struct MonitorModel {
    bool bluetooth_connected;
    bool usb_connected;
    int8_t bluetooth_rssi;
    uint8_t board_battery_percent;
    uint8_t cpu_percent;
    uint8_t ram_percent;
    uint8_t gpu_percent;
    uint8_t swap_percent;
    float ram_used_gb;
    float ram_total_gb;
    float swap_used_gb;
    float cpu_temp_c;
    float system_power_w;
    float room_temp_c;
    uint8_t room_humidity_percent;
    uint8_t noise_db;
    uint16_t process_count;
    float net_down_kbs;
    float net_up_kbs;
    std::string time_text;
    std::string date_text;
    int weather_c;
    std::string weather_text;
    std::string region_text;
    AgentUsage codex;
    AgentUsage claude;
    std::array<ProcessUsage, kMaxProcesses> processes;
    // How many of those the host actually sent.
    std::size_t process_rows = 0;
    std::array<std::string, 8> headlines;
    std::array<std::string, 8> headline_ages;
    // Local, not from the host. Trailing so the demo model stays positional.
    AudioLevels noise{};
    float noise_offset_db = 85.0f;
    // Which host this snapshot came from, and every slot for the devices page.
    std::string source_name;
    bool source_live = false;
    uint32_t source_age_seconds = 0;
    std::array<SourceStatus, kMaxSources> sources{};
    // Todo rows and the ringing alert, both from the host. Trailing and
    // defaulted, like the fields above, so the demo model stays positional.
    std::array<TodoItem, kMaxTodos> todos{};
    std::size_t todo_rows = 0;
    uint16_t alert_id = 0;  // 0 means nothing is ringing.
    std::string alert_title;
    uint32_t alert_age_s = 0;
    // Per logical processor, and the GPU and fan readings a host with the
    // sensors can supply. Zero temperatures and an empty fan list mean the host
    // had nothing, which the pages show as N/A rather than as a cold, still
    // machine.
    std::array<uint8_t, kMaxCores> cores{};
    std::size_t core_count = 0;
    float gpu_temp_c = 0.0f;
    uint16_t gpu_mem_used_mb = 0;
    uint16_t gpu_mem_total_mb = 0;
    float gpu_power_w = 0.0f;
    std::array<int32_t, kMaxFans> fan_rpm{};
    std::array<bool, kMaxFans> fan_known{};
    std::size_t fan_count = 0;
};

MonitorModel monitor_demo_model();
