#pragma once

#include <array>
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
    std::array<ProcessUsage, 5> processes;
    std::array<std::string, 8> headlines;
    std::array<std::string, 8> headline_ages;
    // Local, not from the host. Trailing so the demo model stays positional.
    AudioLevels noise{};
    float noise_offset_db = 85.0f;
};

MonitorModel monitor_demo_model();
