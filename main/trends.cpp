#include "trends.h"

#include <algorithm>
#include <cmath>

namespace {
Trends trends;

int16_t clamp16(float value) {
    return static_cast<int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
}
}  // namespace

void TrendSeries::push(int16_t value) {
    values[next] = value;
    next = (next + 1) % kTrendPoints;
    if (count < kTrendPoints) ++count;
}

int16_t TrendSeries::at(std::size_t index) const {
    if (index >= count) return 0;
    const std::size_t oldest = (count == kTrendPoints) ? next : 0;
    return values[(oldest + index) % kTrendPoints];
}

int16_t TrendSeries::maximum() const {
    int16_t best = 0;
    for (std::size_t i = 0; i < count; ++i) best = std::max(best, at(i));
    return best;
}

void trends_push(const MonitorModel &model) {
    if (trends.source != model.source_name) {
        trends = Trends{};
        trends.source = model.source_name;
    }
    trends.ram_percent.push(model.ram_percent);
    trends.net_down_kbs.push(clamp16(model.net_down_kbs));
    trends.net_up_kbs.push(clamp16(model.net_up_kbs));
    trends.gpu_percent.push(model.gpu_percent);
    // Zero means the host had no reading; it is kept as a gap value the chart
    // leaves out, so a missing sensor does not draw a line along the floor.
    trends.cpu_temp_c.push(clamp16(model.cpu_temp_c));
    trends.gpu_temp_c.push(clamp16(model.gpu_temp_c));
}

const Trends &trends_get() { return trends; }
