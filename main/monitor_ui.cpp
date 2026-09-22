#include "monitor_ui.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "debug_log.h"
#include "rlcd_fonts.h"
#include "trends.h"

namespace {
const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
const lv_color_t WHITE = LV_COLOR_MAKE(255, 255, 255);

lv_obj_t *text(lv_obj_t *parent, const char *value, int x, int y,
               const lv_font_t *font = &rlcd_font_bold_14, lv_color_t color = BLACK) {
    lv_obj_t *item = lv_label_create(parent);
    lv_label_set_text(item, value);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_style_text_font(item, font, 0);
    lv_obj_set_style_text_color(item, color, 0);
    lv_obj_set_style_text_letter_space(item, 0, 0);
    return item;
}

lv_obj_t *panel(lv_obj_t *parent, int x, int y, int width, int height, lv_color_t color = WHITE) {
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_size(item, width, height);
    lv_obj_set_style_bg_color(item, color, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    return item;
}

void rule(lv_obj_t *parent, int x, int y, int width, int height = 1, lv_color_t color = BLACK) {
    panel(parent, x, y, width, height, color);
}

void base(lv_obj_t *page, const char *title, int index) {
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 400, 300);
    lv_obj_set_style_bg_color(page, BLACK, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    text(page, title, 10, 5, &rlcd_font_bold_18, WHITE);
    lv_obj_t *number = text(page, "", 0, 5, &rlcd_font_bold_18, WHITE);
    lv_obj_set_width(number, 80);
    lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(number, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_label_set_text_fmt(number, "%d / %d", index, MonitorUi::kPageCount);
}

void footer(lv_obj_t *page, const char *left, const char *right) {
    text(page, left, 9, 281, &rlcd_font_bold_14, WHITE);
    lv_obj_t *r = text(page, right, 0, 281, &rlcd_font_bold_14, WHITE);
    lv_obj_set_width(r, 210);
    lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(r, LV_ALIGN_BOTTOM_RIGHT, -9, -3);
}

// Names the host a page's data came from. Outlined while that host is live,
// inverted with the data's age once it has dropped, so stale numbers never pass
// for current ones.
void source_tag(lv_obj_t *page, const MonitorModel &m) {
    if (m.source_name.empty()) return;
    char label[40];
    if (m.source_live) {
        snprintf(label, sizeof(label), "%s", m.source_name.c_str());
    } else if (m.source_age_seconds >= 3600) {
        snprintf(label, sizeof(label), "%s %luH", m.source_name.c_str(), (unsigned long)(m.source_age_seconds / 3600));
    } else {
        snprintf(label, sizeof(label), "%s %luM", m.source_name.c_str(), (unsigned long)(m.source_age_seconds / 60));
    }
    lv_obj_t *tag = lv_label_create(page);
    lv_label_set_text(tag, label);
    lv_label_set_long_mode(tag, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(tag, &rlcd_font_bold_14, 0);
    lv_obj_set_style_text_align(tag, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(tag, 184, 5);
    lv_obj_set_size(tag, 132, 21);
    lv_obj_set_style_pad_top(tag, 2, 0);
    lv_obj_set_style_border_color(tag, WHITE, 0);
    lv_obj_set_style_border_width(tag, 2, 0);
    lv_obj_set_style_bg_color(tag, WHITE, 0);
    lv_obj_set_style_bg_opa(tag, m.source_live ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tag, m.source_live ? WHITE : BLACK, 0);
}

void format_rate(char *out, size_t size, const char *label, float kb_per_second) {
    if (kb_per_second >= 1024.0f) snprintf(out, size, "%s %.1fM/S", label, kb_per_second / 1024.0f);
    else snprintf(out, size, "%s %.0fK/S", label, kb_per_second);
}

void usage_bar(lv_obj_t *parent, int x, int y, int width, int value) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, width, 18);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, BLACK, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, BLACK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
}
// A line over the last kTrendPoints snapshots. Points the series does not
// have yet are left out, so a young history is a short line at the right
// rather than one that climbs from zero. With zero_is_gap, a zero is a
// snapshot without that reading and is left out too.
lv_obj_t *trend_chart(lv_obj_t *parent, int x, int y, int width, int height,
                      const TrendSeries &series, int32_t low, int32_t high, bool zero_is_gap = false) {
    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_remove_style_all(chart);
    lv_obj_set_pos(chart, x, y);
    lv_obj_set_size(chart, width, height);
    lv_obj_set_style_bg_color(chart, WHITE, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, BLACK, 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_line_color(chart, BLACK, LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, kTrendPoints);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, low, high);
    lv_chart_series_t *line = lv_chart_add_series(chart, BLACK, LV_CHART_AXIS_PRIMARY_Y);
    const std::size_t missing = kTrendPoints - series.count;
    for (std::size_t i = 0; i < kTrendPoints; ++i) {
        int32_t value = LV_CHART_POINT_NONE;
        if (i >= missing) {
            value = series.at(i - missing);
            if (zero_is_gap && value == 0) value = LV_CHART_POINT_NONE;
        }
        lv_chart_set_next_value(chart, line, value);
    }
    lv_chart_refresh(chart);
    return chart;
}
}  // namespace

void MonitorUi::create(const MonitorModel &model) {
    lv_obj_set_style_bg_color(lv_screen_active(), BLACK, 0);
    create_overview(model);
    create_cpu(model);
    create_trends(model);
    create_processes(model);
    create_activity(model);
    create_news(model);
    create_todo(model);
    create_devices(model);
    create_debug(model);
    for (int i = 0; i < kPageCount; ++i) if (i != current_page_) lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
}

void MonitorUi::create_overview(const MonitorModel &m) {
    pages_[0] = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(pages_[0]);
    lv_obj_set_size(pages_[0], 400, 300);
    lv_obj_set_style_bg_color(pages_[0], BLACK, 0);
    lv_obj_set_style_bg_opa(pages_[0], LV_OPA_COVER, 0);
    lv_obj_t *status = panel(pages_[0], 0, 0, 400, 30);
    lv_obj_t *bt = text(status, "BT --", 9, 5, &rlcd_font_bold_18);
    if (m.bluetooth_connected && m.bluetooth_rssi) lv_label_set_text_fmt(bt, "BT %d", m.bluetooth_rssi);
    else if (m.bluetooth_connected) lv_label_set_text(bt, "BT ON");
    // The signal stays on the left; the source it belongs to sits beside it.
    if (!m.source_name.empty()) {
        lv_obj_t *source = text(status, "", 98, 8, &rlcd_font_bold_14);
        lv_obj_set_width(source, 130);
        lv_label_set_long_mode(source, LV_LABEL_LONG_CLIP);
        if (m.source_live) lv_label_set_text(source, m.source_name.c_str());
        else lv_label_set_text_fmt(source, "%s OFF", m.source_name.c_str());
    }
    text(status, m.codex.active ? "CDX*" : "CDX", 236, 5, &rlcd_font_bold_18);
    text(status, m.claude.active ? "CLD*" : "CLD", 297, 5, &rlcd_font_bold_18);
    // On USB the cell voltage is held up by the charger, so a percentage there
    // would read full even with no battery in the holder.
    lv_obj_t *bat = text(status, "", 354, 5, &rlcd_font_bold_18);
    if (m.usb_connected) lv_label_set_text(bat, "USB");
    else lv_label_set_text_fmt(bat, "%u%%", m.board_battery_percent);

    lv_obj_t *clock = panel(pages_[0], 0, 32, 400, 166);
    // The digit-only font has a 76 px line box, so centre it in the band above
    // the rule rather than trusting the nominal 104 px size.
    lv_obj_t *time = text(clock, m.time_text.c_str(), 0, 21, &rlcd_font_bold_104);
    lv_obj_set_width(time, 400); lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, 0);
    rule(clock, 0, 118, 400);
    text(clock, m.date_text.c_str(), 9, 123, &rlcd_font_bold_14);
    lv_obj_t *region = text(clock, m.region_text.c_str(), 0, 145, &rlcd_font_bold_14);
    lv_obj_set_width(region, 400); lv_obj_set_style_text_align(region, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *weather = text(clock, "", 0, 123, &rlcd_font_bold_14);
    lv_obj_set_width(weather, 145);
    lv_obj_set_style_text_align(weather, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(weather, LV_ALIGN_TOP_RIGHT, -9, 123);
    lv_label_set_text_fmt(weather, "%dC %s", m.weather_c, m.weather_text.c_str());

    const int x[] = {7, 105, 203};
    const char *names[] = {"TEMP", "HUMID", "NOISE"};
    char values[3][16];
    if (m.room_temp_c != 0.0f) snprintf(values[0], sizeof(values[0]), "%.1fC", m.room_temp_c);
    else snprintf(values[0], sizeof(values[0]), "--.-C");
    snprintf(values[1], sizeof(values[1]), "%u%%", m.room_humidity_percent);
    if (m.noise_db) snprintf(values[2], sizeof(values[2]), "%udB", m.noise_db);
    else snprintf(values[2], sizeof(values[2]), "--dB");
    for (int i = 0; i < 3; ++i) {
        if (i) rule(pages_[0], x[i] - 7, 201, 1, 67, WHITE);
        text(pages_[0], values[i], x[i], 205, &rlcd_font_bold_26, WHITE);
        text(pages_[0], names[i], x[i] + 8, 248, &rlcd_font_bold_14, WHITE);
    }
    // The sound page folded into this cell. Every second window is drawn, two
    // pixels wide, which is half the objects of a full history for a trend that
    // reads the same at this size.
    rule(pages_[0], 294, 201, 1, 67, WHITE);
    const AudioLevels &n = m.noise;
    lv_obj_t *peak = text(pages_[0], "SOUND", 310, 248, &rlcd_font_bold_14, WHITE);
    if (n.peak) lv_label_set_text_fmt(peak, "PEAK %u", n.peak);
    constexpr int kFloor = 20;
    constexpr int kRange = 80;
    constexpr int kSparkHeight = 38;
    constexpr int kSparkTop = 205;
    constexpr int kSparkBars = 32;
    const int shown = std::min<int>(static_cast<int>(n.filled), static_cast<int>(kNoiseHistory));
    // Newest on the right, so the bars march the way the history grows.
    for (int i = 0; i < kSparkBars; ++i) {
        const int sample = shown - 1 - (kSparkBars - 1 - i) * 2;
        if (sample < 0) continue;
        const int level = std::clamp(static_cast<int>(n.history[sample]) - kFloor, 1, kRange);
        const int height = level * kSparkHeight / kRange;
        panel(pages_[0], 305 + i * 2, kSparkTop + kSparkHeight - height, 2, height, WHITE);
    }
    if (!shown) text(pages_[0], "LISTENING", 305, 218, &rlcd_font_bold_14, WHITE);
    footer(pages_[0], "BOOT: BACK", "KEY: NEXT");
}

void MonitorUi::create_cpu(const MonitorModel &m) {
    pages_[1] = lv_obj_create(lv_screen_active()); base(pages_[1], "CPU", 2); source_tag(pages_[1], m);
    char value[40];

    lv_obj_t *head = panel(pages_[1], 0, 32, 400, 34);
    snprintf(value, sizeof(value), "CPU %u%%", m.cpu_percent);
    text(head, value, 12, 3, &rlcd_font_bold_26);
    if (m.cpu_temp_c > 0.0f) snprintf(value, sizeof(value), "TEMP %.0fC", m.cpu_temp_c);
    else snprintf(value, sizeof(value), "TEMP N/A");
    lv_obj_t *temp = text(head, value, 0, 7, &rlcd_font_bold_18);
    lv_obj_set_width(temp, 160); lv_obj_set_style_text_align(temp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(temp, LV_ALIGN_TOP_RIGHT, -12, 7);

    // One bar per logical processor, a pixel per percent. The pitch follows
    // the count, so a 32 thread machine still fits; labels go once they would
    // touch.
    lv_obj_t *bars = panel(pages_[1], 0, 70, 400, 130);
    const int count = static_cast<int>(std::min(m.core_count, m.cores.size()));
    constexpr int kBaseline = 108;
    rule(bars, 10, kBaseline, 380);
    if (count) {
        const int pitch = 380 / count;
        const int width = std::max(3, pitch - 5);
        for (int i = 0; i < count; ++i) {
            const int height = std::max(1, static_cast<int>(m.cores[i]));
            panel(bars, 12 + i * pitch, kBaseline - height, width, height, BLACK);
            if (pitch >= 20) {
                char index[16]; snprintf(index, sizeof(index), "%d", i + 1);
                text(bars, index, 12 + i * pitch + (i + 1 < 10 ? 5 : 1), kBaseline + 4, &rlcd_font_bold_14);
            }
        }
    } else {
        text(bars, "NO PER CORE DATA", 12, 40, &rlcd_font_bold_18);
    }

    lv_obj_t *gpu = panel(pages_[1], 0, 204, 400, 34);
    text(gpu, "GPU", 12, 8, &rlcd_font_bold_18);
    snprintf(value, sizeof(value), "%u%%", m.gpu_percent);
    text(gpu, value, 62, 3, &rlcd_font_bold_26);
    if (m.gpu_temp_c > 0.0f) snprintf(value, sizeof(value), "%.0fC", m.gpu_temp_c);
    else snprintf(value, sizeof(value), "--C");
    text(gpu, value, 150, 8, &rlcd_font_bold_18);
    if (m.gpu_power_w > 0.0f) snprintf(value, sizeof(value), "%.1fW", m.gpu_power_w);
    else value[0] = 0;
    text(gpu, value, 215, 8, &rlcd_font_bold_18);
    if (m.gpu_mem_total_mb) snprintf(value, sizeof(value), "%.1f/%.1fG", m.gpu_mem_used_mb / 1024.0f, m.gpu_mem_total_mb / 1024.0f);
    else value[0] = 0;
    lv_obj_t *memory = text(gpu, value, 0, 11, &rlcd_font_bold_14);
    lv_obj_set_width(memory, 100); lv_obj_set_style_text_align(memory, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(memory, LV_ALIGN_TOP_RIGHT, -12, 11);

    // N/A is a fan the sensors did not report, never a stopped one: a fan
    // that has stopped reads 0 RPM.
    lv_obj_t *fans = panel(pages_[1], 0, 242, 400, 34);
    const int fan_count = static_cast<int>(std::min(m.fan_count, m.fan_rpm.size()));
    if (!fan_count) {
        text(fans, "FAN N/A", 12, 8, &rlcd_font_bold_18);
        lv_obj_t *hint = text(fans, "NO SENSOR TASK", 0, 11, &rlcd_font_bold_14);
        lv_obj_set_width(hint, 200); lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -12, 11);
    } else {
        const int shown = std::min(fan_count, 2);
        for (int i = 0; i < shown; ++i) {
            if (m.fan_known[i]) snprintf(value, sizeof(value), "FAN%d %ld", i + 1, static_cast<long>(m.fan_rpm[i]));
            else snprintf(value, sizeof(value), "FAN%d N/A", i + 1);
            text(fans, value, 12 + i * 170, 8, &rlcd_font_bold_18);
        }
        lv_obj_t *unit = text(fans, "RPM", 0, 11, &rlcd_font_bold_14);
        lv_obj_set_width(unit, 60); lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(unit, LV_ALIGN_TOP_RIGHT, -12, 11);
    }
    snprintf(value, sizeof(value), "%d THREADS", count);
    footer(pages_[1], value, "HOLD KEY: SOURCE");
}

void MonitorUi::create_trends(const MonitorModel &m) {
    pages_[2] = lv_obj_create(lv_screen_active()); base(pages_[2], "TRENDS", 3); source_tag(pages_[2], m);
    const Trends &t = trends_get();
    char value[32];
    constexpr int kRow = 61;

    lv_obj_t *ram = panel(pages_[2], 0, 32, 400, 58);
    text(ram, "RAM", 10, 4, &rlcd_font_bold_18);
    snprintf(value, sizeof(value), "%u%%", m.ram_percent);
    text(ram, value, 10, 24, &rlcd_font_bold_26);
    trend_chart(ram, 100, 4, 288, 50, t.ram_percent, 0, 100);

    // Down and up side by side rather than on one chart: two black lines on a
    // one bit panel cannot be told apart. Each scales to its own busiest
    // minute, with a floor so an idle link is not drawn as a mountain range.
    lv_obj_t *net = panel(pages_[2], 0, 32 + kRow, 400, 58);
    text(net, "NET", 10, 4, &rlcd_font_bold_18);
    format_rate(value, sizeof(value), "DN", m.net_down_kbs);
    text(net, value, 10, 24, &rlcd_font_bold_14);
    format_rate(value, sizeof(value), "UP", m.net_up_kbs);
    text(net, value, 10, 40, &rlcd_font_bold_14);
    trend_chart(net, 100, 4, 140, 50, t.net_down_kbs, 0, std::max<int32_t>(64, t.net_down_kbs.maximum()));
    trend_chart(net, 248, 4, 140, 50, t.net_up_kbs, 0, std::max<int32_t>(64, t.net_up_kbs.maximum()));

    lv_obj_t *gpu = panel(pages_[2], 0, 32 + 2 * kRow, 400, 58);
    text(gpu, "GPU", 10, 4, &rlcd_font_bold_18);
    snprintf(value, sizeof(value), "%u%%", m.gpu_percent);
    text(gpu, value, 10, 24, &rlcd_font_bold_26);
    trend_chart(gpu, 100, 4, 288, 50, t.gpu_percent, 0, 100);

    lv_obj_t *temp = panel(pages_[2], 0, 32 + 3 * kRow, 400, 58);
    text(temp, "TEMP", 10, 4, &rlcd_font_bold_18);
    if (m.cpu_temp_c > 0.0f) snprintf(value, sizeof(value), "CPU %.0fC", m.cpu_temp_c);
    else snprintf(value, sizeof(value), "CPU N/A");
    text(temp, value, 10, 24, &rlcd_font_bold_14);
    if (m.gpu_temp_c > 0.0f) snprintf(value, sizeof(value), "GPU %.0fC", m.gpu_temp_c);
    else snprintf(value, sizeof(value), "GPU N/A");
    text(temp, value, 10, 40, &rlcd_font_bold_14);
    trend_chart(temp, 100, 4, 140, 50, t.cpu_temp_c, 20, 100, true);
    trend_chart(temp, 248, 4, 140, 50, t.gpu_temp_c, 20, 100, true);

    snprintf(value, sizeof(value), "%u OF %u POINTS", static_cast<unsigned>(t.ram_percent.count), static_cast<unsigned>(kTrendPoints));
    footer(pages_[2], value, "1 PER SNAPSHOT");
}

void MonitorUi::create_processes(const MonitorModel &m) {
    pages_[3] = lv_obj_create(lv_screen_active()); base(pages_[3], "TOP PROCESSES", 4); source_tag(pages_[3], m);
    constexpr int kRows = 4;
    // The host sends the leaders of every ordering, so the ordering can change
    // here without waiting for the next snapshot.
    const std::size_t sent = m.process_rows ? std::min(m.process_rows, m.processes.size()) : m.processes.size();
    std::array<const ProcessUsage *, kMaxProcesses> ranked{};
    std::size_t ranked_count = 0;
    for (std::size_t i = 0; i < sent; ++i) {
        if (m.processes[i].name.empty() || m.processes[i].name == "-") continue;
        ranked[ranked_count++] = &m.processes[i];
    }
    const ProcessSort sort = process_sort_;
    std::stable_sort(ranked.begin(), ranked.begin() + ranked_count,
                     [sort](const ProcessUsage *a, const ProcessUsage *b) {
                         switch (sort) {
                             case ProcessSort::Ram: return a->ram_mb > b->ram_mb;
                             case ProcessSort::Impact: return a->impact > b->impact;
                             case ProcessSort::Cpu: break;
                         }
                         return a->cpu_percent > b->cpu_percent;
                     });
    static const ProcessUsage kBlank{"-", 0.0f, 0, 0.0f};
    for (int i = 0; i < kRows; ++i) {
        const ProcessUsage &process =
            (static_cast<std::size_t>(i) < ranked_count) ? *ranked[i] : kBlank;
        lv_obj_t *row = panel(pages_[3], 0, 32 + i * 60, 400, 58);
        lv_obj_t *name = text(row, process.name.empty() ? "-" : process.name.c_str(), 11, 5, &rlcd_font_bold_26);
        lv_obj_set_width(name, 250); lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        // The big figure is whatever the rows are ranked by; the small line
        // carries the rest, so nothing is lost by switching.
        char meta[40];
        char leader[16];
        switch (process_sort_) {
            case ProcessSort::Ram:
                snprintf(meta, sizeof(meta), "CPU %.0f%%   IMPACT %.0f", process.cpu_percent, process.impact);
                snprintf(leader, sizeof(leader), "%.1fG", process.ram_mb / 1024.0f);
                break;
            case ProcessSort::Impact:
                snprintf(meta, sizeof(meta), "CPU %.0f%%   RAM %.1fG", process.cpu_percent, process.ram_mb / 1024.0f);
                snprintf(leader, sizeof(leader), "%.0f", process.impact);
                break;
            case ProcessSort::Cpu:
                snprintf(meta, sizeof(meta), "RAM %.1fG   IMPACT %.0f", process.ram_mb / 1024.0f, process.impact);
                snprintf(leader, sizeof(leader), "%.0f%%", process.cpu_percent);
                break;
        }
        text(row, meta, 12, 38, &rlcd_font_bold_14);
        lv_obj_t *figure = text(row, leader, 0, 6, &rlcd_font_bold_40);
        lv_obj_set_width(figure, 130); lv_obj_set_style_text_align(figure, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(figure, LV_ALIGN_TOP_RIGHT, -10, 5);
    }
    const char *sort_name = process_sort_ == ProcessSort::Ram      ? "HOLD KEY: BY RAM"
                            : process_sort_ == ProcessSort::Impact ? "HOLD KEY: BY IMPACT"
                                                                   : "HOLD KEY: BY CPU";
    char count[28]; snprintf(count, sizeof(count), "TOP %d OF %u", kRows, m.process_count);
    footer(pages_[3], count, sort_name);
}

void MonitorUi::create_activity(const MonitorModel &m) {
    pages_[4] = lv_obj_create(lv_screen_active()); base(pages_[4], "AGENT USAGE", 5); source_tag(pages_[4], m);
    lv_obj_t *total = panel(pages_[4], 0, 32, 400, 98);
    text(total, "TOKENS TODAY", 12, 8, &rlcd_font_bold_18);
    char token_total[24];
    snprintf(token_total, sizeof(token_total), "%.2fM", (m.codex.tokens_k + m.claude.tokens_k) / 1000.0f);
    text(total, token_total, 10, 23, &rlcd_font_bold_72);
    const AgentUsage *agents[] = {&m.codex, &m.claude};
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *card = panel(pages_[4], i * 201, 132, 199, 136);
        text(card, agents[i]->name.c_str(), 11, 9, &rlcd_font_bold_18);
        lv_obj_t *state = text(card, agents[i]->active ? "RUN" : "IDLE", 118, 9, &rlcd_font_bold_14, WHITE);
        lv_obj_set_style_bg_color(state, BLACK, 0); lv_obj_set_style_bg_opa(state, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(state, 3, 0);
        text(card, agents[i]->five_hour_usage.c_str(), 10, 42, &rlcd_font_bold_26);
        text(card, agents[i]->weekly_usage.c_str(), 10, 74, &rlcd_font_bold_26);
        lv_obj_t *meta = text(card, "", 10, 108, &rlcd_font_bold_14);
        lv_label_set_text_fmt(meta, "%luK - %u SESS", (unsigned long)agents[i]->tokens_k, agents[i]->sessions);
    }
    footer(pages_[4], "24H LOCAL LOGS", "CODEX + CLAUDE");
}

void MonitorUi::create_news(const MonitorModel &m) {
    pages_[5] = lv_obj_create(lv_screen_active()); base(pages_[5], "TECH NEWS", 6); source_tag(pages_[5], m);
    for (int i = 0; i < 4; ++i) {
        int index = news_page_ * 4 + i;
        lv_obj_t *row = panel(pages_[5], 0, 32 + i * 60, 400, 58);
        lv_obj_t *headline = text(row, m.headlines[index].c_str(), 10, 6, &rlcd_font_bold_18);
        lv_obj_set_width(headline, 325); lv_obj_set_height(headline, 48);
        lv_label_set_long_mode(headline, LV_LABEL_LONG_WRAP);
        lv_obj_t *age = text(row, m.headline_ages[index].c_str(), 0, 19, &rlcd_font_bold_14);
        lv_obj_set_width(age, 55); lv_obj_set_style_text_align(age, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(age, LV_ALIGN_RIGHT_MID, -9, 0);
    }
    char page[32]; snprintf(page, sizeof(page), "%d OF 2 PAGES", news_page_ + 1);
    footer(pages_[5], page, "HOLD KEY: MORE");
}

void MonitorUi::create_todo(const MonitorModel &m) {
    pages_[6] = lv_obj_create(lv_screen_active()); base(pages_[6], "TODO", 7); source_tag(pages_[6], m);
    const std::size_t rows = std::min(m.todo_rows, m.todos.size());
    lv_obj_t *card = panel(pages_[6], 0, 32, 400, 244);
    if (!rows) text(card, "NOTHING LISTED", 12, 10, &rlcd_font_bold_18);
    int overdue = 0;
    for (std::size_t i = 0; i < rows; ++i) {
        const TodoItem &item = m.todos[i];
        const int y = 6 + static_cast<int>(i) * 30;
        char number[8]; snprintf(number, sizeof(number), "%u.", static_cast<unsigned>(i + 1));
        text(card, number, 12, y + 2, &rlcd_font_bold_18);
        lv_obj_t *line = text(card, item.text.c_str(), 40, y + 2, &rlcd_font_bold_18);
        lv_obj_set_width(line, 250); lv_label_set_long_mode(line, LV_LABEL_LONG_CLIP);
        // The remaining time reads as a person would say it, and turns to a
        // minus once the deadline has passed rather than vanishing.
        char remaining[16] = "";
        if (item.has_due) {
            const int minutes = item.due_min;
            const int magnitude = minutes < 0 ? -minutes : minutes;
            const char *sign = minutes < 0 ? "-" : "";
            if (magnitude >= 1440) snprintf(remaining, sizeof(remaining), "%s%dD %dH", sign, magnitude / 1440, (magnitude % 1440) / 60);
            else if (magnitude >= 60) snprintf(remaining, sizeof(remaining), "%s%dH %02dM", sign, magnitude / 60, magnitude % 60);
            else snprintf(remaining, sizeof(remaining), "%s%dM", sign, magnitude);
            if (minutes < 0) ++overdue;
        }
        lv_obj_t *due = text(card, remaining, 0, y + 4, &rlcd_font_bold_14);
        lv_obj_set_width(due, 90); lv_obj_set_style_text_align(due, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(due, LV_ALIGN_TOP_RIGHT, -12, y + 4);
        if (i + 1 < rows) rule(card, 12, y + 27, 376);
    }
    char count[32];
    if (overdue) snprintf(count, sizeof(count), "%u ITEMS, %d OVERDUE", static_cast<unsigned>(rows), overdue);
    else snprintf(count, sizeof(count), "%u ITEMS", static_cast<unsigned>(rows));
    footer(pages_[6], count, "FROM CONFIG");
}

void MonitorUi::create_devices(const MonitorModel &m) {
    pages_[7] = lv_obj_create(lv_screen_active()); base(pages_[7], "DEVICES", 8);
    int rows = 0;
    int live = 0;
    for (const SourceStatus &source : m.sources) {
        if (!source.has_data) continue;
        if (source.connected) ++live;
        lv_obj_t *row = panel(pages_[7], 0, 32 + rows * 78, 400, 74);
        if (source.shown) rule(row, 0, 0, 8, 74);
        lv_obj_t *name = text(row, source.name.c_str(), 20, 6, &rlcd_font_bold_26);
        lv_obj_set_width(name, 260); lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        char detail[40];
        if (source.connected && source.age_seconds < 120) {
            snprintf(detail, sizeof(detail), "BT %d   UPDATED %luS", source.rssi, (unsigned long)source.age_seconds);
        } else if (source.connected) {
            snprintf(detail, sizeof(detail), "BT %d   UPDATED %luM", source.rssi, (unsigned long)(source.age_seconds / 60));
        } else if (source.age_seconds >= 3600) {
            snprintf(detail, sizeof(detail), "LAST SEEN %luH AGO", (unsigned long)(source.age_seconds / 3600));
        } else {
            snprintf(detail, sizeof(detail), "LAST SEEN %luM AGO", (unsigned long)(source.age_seconds / 60));
        }
        text(row, detail, 20, 44, &rlcd_font_bold_14);
        const char *state_text = source.shown ? "SHOWING" : (source.connected ? "LIVE" : "OFFLINE");
        lv_obj_t *state = text(row, state_text, 0, 0, &rlcd_font_bold_14, source.shown ? WHITE : BLACK);
        lv_obj_set_style_pad_all(state, 5, 0);
        if (source.shown) {
            lv_obj_set_style_bg_color(state, BLACK, 0);
            lv_obj_set_style_bg_opa(state, LV_OPA_COVER, 0);
        } else if (source.connected) {
            lv_obj_set_style_border_color(state, BLACK, 0);
            lv_obj_set_style_border_width(state, 2, 0);
        }
        lv_obj_align(state, LV_ALIGN_RIGHT_MID, -10, 0);
        ++rows;
    }
    if (!rows) text(pages_[7], "WAITING FOR A HOST", 12, 48, &rlcd_font_bold_18, WHITE);
    char count[24];
    snprintf(count, sizeof(count), "%d LIVE OF %d", live, rows);
    footer(pages_[7], "HOLD KEY: SWITCH", count);
}

void MonitorUi::create_debug(const MonitorModel &m) {
    pages_[8] = lv_obj_create(lv_screen_active()); base(pages_[8], "DEBUG LOG", 9);
    (void)m;
    const int total_lines = static_cast<int>(debug_log_count());
    // Newest at the bottom, the way a terminal reads. Without scrolling the
    // view sits at the end, so the latest line is always the one on screen.
    constexpr int kVisible = 11;
    const int max_scroll = total_lines > kVisible ? total_lines - kVisible : 0;
    if (debug_scroll_ > max_scroll) debug_scroll_ = max_scroll;
    if (debug_scroll_ < 0) debug_scroll_ = 0;
    const int first = max_scroll - debug_scroll_;
    lv_obj_t *card = panel(pages_[8], 0, 32, 400, 244);
    if (!total_lines) {
        text(card, "NO LOG LINES YET", 12, 10, &rlcd_font_bold_18);
    }
    char line[kLogLineChars];
    for (int i = 0; i < kVisible; ++i) {
        const int index = first + i;
        if (index >= total_lines) break;
        debug_log_copy(static_cast<std::size_t>(index), line, sizeof(line));
        lv_obj_t *row = text(card, line, 8, 6 + i * 21, &rlcd_font_bold_14);
        lv_obj_set_width(row, 384);
        lv_label_set_long_mode(row, LV_LABEL_LONG_CLIP);
    }
    char position[32];
    if (total_lines) {
        snprintf(position, sizeof(position), "%d-%d OF %lu", first + 1,
                 (first + kVisible < total_lines) ? first + kVisible : total_lines,
                 debug_log_total());
    } else {
        snprintf(position, sizeof(position), "0 LINES");
    }
    footer(pages_[8], position, max_scroll ? "HOLD KEY: SCROLL" : "BOOT: BACK");
}

void MonitorUi::show_alert(const char *title) {
    hide_alert();
    // The top layer sits above every screen, so the box survives the pages
    // being deleted and rebuilt under it on the next snapshot.
    lv_obj_t *box = panel(lv_layer_top(), 30, 60, 340, 180, WHITE);
    lv_obj_set_style_border_color(box, BLACK, 0);
    lv_obj_set_style_border_width(box, 4, 0);
    rule(box, 0, 40, 340, 3, BLACK);
    lv_obj_t *label = text(box, "ALERT", 0, 8, &rlcd_font_bold_18);
    lv_obj_set_width(label, 340); lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *what = text(box, title, 12, 54, &rlcd_font_bold_26);
    lv_obj_set_width(what, 316); lv_obj_set_height(what, 76);
    lv_obj_set_style_text_align(what, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(what, LV_LABEL_LONG_WRAP);
    lv_obj_t *hint = text(box, "PRESS ANY BUTTON", 0, 146, &rlcd_font_bold_14);
    lv_obj_set_width(hint, 340); lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    alert_ = box;
    lv_obj_invalidate(lv_layer_top());
}

void MonitorUi::hide_alert() {
    if (!alert_) return;
    lv_obj_delete(alert_);
    alert_ = nullptr;
    // The area under the box has to be repainted from the page below.
    if (pages_[current_page_]) lv_obj_invalidate(pages_[current_page_]);
}

void MonitorUi::show_next() {
    lv_obj_add_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
    current_page_ = (current_page_ + 1) % kPageCount;
    lv_obj_remove_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(pages_[current_page_]);
}

void MonitorUi::show_previous() {
    lv_obj_add_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
    current_page_ = (current_page_ + kPageCount - 1) % kPageCount;
    lv_obj_remove_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(pages_[current_page_]);
}

void MonitorUi::show_home() {
    if (current_page_ == 0) return;
    lv_obj_add_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
    current_page_ = 0;
    lv_obj_remove_flag(pages_[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(pages_[0]);
}

void MonitorUi::handle_long_press(const MonitorModel &m) {
    if (current_page_ == kPageProcesses) {
        // Cycles processor, memory, impact. This page gives up switching the
        // shown source for it; the other pages still offer that.
        process_sort_ = static_cast<ProcessSort>(
            (static_cast<uint8_t>(process_sort_) + 1) % kProcessSortCount);
        lv_obj_delete(pages_[kPageProcesses]); pages_[kPageProcesses] = nullptr;
        create_processes(m); lv_obj_invalidate(pages_[kPageProcesses]);
        return;
    }
    if (current_page_ == kPageNews) {
        news_page_ = 1 - news_page_;
        lv_obj_delete(pages_[kPageNews]); pages_[kPageNews] = nullptr;
        create_news(m); lv_obj_invalidate(pages_[kPageNews]);
        return;
    }
    if (current_page_ == kPageDebug) {
        // One screen older each time, wrapping back to the newest at the end,
        // since two buttons leave no room for a separate scroll-up.
        const int total_lines = static_cast<int>(debug_log_count());
        constexpr int kVisible = 11;
        const int max_scroll = total_lines > kVisible ? total_lines - kVisible : 0;
        debug_scroll_ = (debug_scroll_ + kVisible > max_scroll) ? 0 : debug_scroll_ + kVisible;
        lv_obj_delete(pages_[kPageDebug]); pages_[kPageDebug] = nullptr;
        create_debug(m); lv_obj_invalidate(pages_[kPageDebug]);
        return;
    }
}

void MonitorUi::update(const MonitorModel &m) {
    for (auto &page : pages_) { if (page) lv_obj_delete(page); page = nullptr; }
    create(m);
}
