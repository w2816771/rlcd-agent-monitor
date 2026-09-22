#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "monitor_model.h"

// One point per snapshot, so the span is update_seconds times the point count:
// an hour at the default minute, a quarter of an hour at fifteen seconds. The
// host sends no history; the board remembers what it was shown.
constexpr std::size_t kTrendPoints = 60;

struct TrendSeries {
    std::array<int16_t, kTrendPoints> values{};
    std::size_t count = 0;  // Valid points, oldest first once full.
    std::size_t next = 0;   // Ring slot the next point goes into.

    void push(int16_t value);
    // Oldest first, the newest at index count - 1.
    int16_t at(std::size_t index) const;
    int16_t maximum() const;
};

struct Trends {
    TrendSeries ram_percent;
    TrendSeries net_down_kbs;
    TrendSeries net_up_kbs;
    TrendSeries gpu_percent;
    TrendSeries cpu_temp_c;
    TrendSeries gpu_temp_c;
    // Which host these belong to. A different one starts fresh rather than
    // splicing two machines into one line.
    std::string source;
};

// Records one snapshot. Call once per snapshot applied to the display.
void trends_push(const MonitorModel &model);
const Trends &trends_get();
