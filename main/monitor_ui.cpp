#include "monitor_ui.h"

#include <algorithm>
#include <cstdio>

#include "rlcd_fonts.h"

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
}  // namespace

void MonitorUi::create(const MonitorModel &model) {
    lv_obj_set_style_bg_color(lv_screen_active(), BLACK, 0);
    create_overview(model);
    create_system(model);
    create_activity(model);
    create_processes(model);
    create_news(model);
    create_sound(model);
    create_devices(model);
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
    rule(pages_[0], 294, 201, 1, 67, WHITE);
    text(pages_[0], "SOUND", 310, 248, &rlcd_font_bold_14, WHITE);
    lv_obj_t *sound = lv_bar_create(pages_[0]);
    lv_obj_set_pos(sound, 307, 215); lv_obj_set_size(sound, 80, 16);
    lv_bar_set_range(sound, 20, 100); lv_bar_set_value(sound, m.noise_db ? m.noise_db : 20, LV_ANIM_OFF);
    lv_obj_set_style_radius(sound, 0, LV_PART_MAIN); lv_obj_set_style_radius(sound, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sound, BLACK, LV_PART_MAIN); lv_obj_set_style_border_color(sound, WHITE, LV_PART_MAIN);
    lv_obj_set_style_border_width(sound, 2, LV_PART_MAIN); lv_obj_set_style_bg_color(sound, WHITE, LV_PART_INDICATOR);
    footer(pages_[0], "BOOT: BACK", "KEY: NEXT");
}

void MonitorUi::create_system(const MonitorModel &m) {
    pages_[1] = lv_obj_create(lv_screen_active()); base(pages_[1], "HOST SYSTEM", 2); source_tag(pages_[1], m);
    lv_obj_t *card = panel(pages_[1], 0, 32, 400, 202);
    char value[24];
    text(card, "CPU USAGE", 12, 10, &rlcd_font_bold_18);
    snprintf(value, sizeof(value), "%u%%", m.cpu_percent);
    lv_obj_t *cpu = text(card, value, 0, 3, &rlcd_font_bold_40);
    lv_obj_set_width(cpu, 100); lv_obj_set_style_text_align(cpu, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(cpu, LV_ALIGN_TOP_RIGHT, -12, 3);
    usage_bar(card, 12, 56, 376, m.cpu_percent);
    rule(card, 0, 94, 400);
    text(card, "RAM USAGE", 12, 104, &rlcd_font_bold_18);
    snprintf(value, sizeof(value), "%u%%", m.ram_percent);
    lv_obj_t *ram = text(card, value, 0, 97, &rlcd_font_bold_40);
    lv_obj_set_width(ram, 100); lv_obj_set_style_text_align(ram, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(ram, LV_ALIGN_TOP_RIGHT, -12, 97);
    usage_bar(card, 12, 150, 376, m.ram_percent);
    snprintf(value, sizeof(value), "%.1f / %.0f GB", m.ram_used_gb, m.ram_total_gb);
    text(card, value, 12, 176, &rlcd_font_bold_14);
    snprintf(value, sizeof(value), "%.1fG", m.swap_used_gb);
    lv_obj_t *swap_label = text(card, "SWAP", 0, 178, &rlcd_font_bold_14);
    lv_obj_set_width(swap_label, 250); lv_obj_set_style_text_align(swap_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(swap_label, LV_ALIGN_BOTTOM_RIGHT, -64, -8);
    lv_obj_t *swap = text(card, value, 0, 178, &rlcd_font_bold_14);
    lv_obj_set_width(swap, 160); lv_obj_set_style_text_align(swap, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(swap, LV_ALIGN_BOTTOM_RIGHT, -12, -8);

    lv_obj_t *net = panel(pages_[1], 0, 238, 400, 38);
    text(net, "NET", 12, 11, &rlcd_font_bold_14);
    char down[20]; char up[20];
    format_rate(down, sizeof(down), "DN", m.net_down_kbs);
    format_rate(up, sizeof(up), "UP", m.net_up_kbs);
    text(net, down, 54, 11, &rlcd_font_bold_14);
    text(net, up, 186, 11, &rlcd_font_bold_14);
    lv_obj_t *mode = text(net, m.usb_connected ? "FULL" : "POWER SAVE", 0, 11, &rlcd_font_bold_14);
    lv_obj_set_width(mode, 150); lv_obj_set_style_text_align(mode, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(mode, LV_ALIGN_RIGHT_MID, -12, 0);
    footer(pages_[1], "BOOT: BACK", "HOLD KEY: SOURCE");
}

void MonitorUi::create_activity(const MonitorModel &m) {
    pages_[2] = lv_obj_create(lv_screen_active()); base(pages_[2], "AGENT USAGE", 3); source_tag(pages_[2], m);
    lv_obj_t *total = panel(pages_[2], 0, 32, 400, 98);
    text(total, "TOKENS TODAY", 12, 8, &rlcd_font_bold_18);
    char token_total[24];
    snprintf(token_total, sizeof(token_total), "%.2fM", (m.codex.tokens_k + m.claude.tokens_k) / 1000.0f);
    text(total, token_total, 10, 23, &rlcd_font_bold_72);
    const AgentUsage *agents[] = {&m.codex, &m.claude};
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *card = panel(pages_[2], i * 201, 132, 199, 136);
        text(card, agents[i]->name.c_str(), 11, 9, &rlcd_font_bold_18);
        lv_obj_t *state = text(card, agents[i]->active ? "RUN" : "IDLE", 118, 9, &rlcd_font_bold_14, WHITE);
        lv_obj_set_style_bg_color(state, BLACK, 0); lv_obj_set_style_bg_opa(state, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(state, 3, 0);
        text(card, agents[i]->five_hour_usage.c_str(), 10, 42, &rlcd_font_bold_26);
        text(card, agents[i]->weekly_usage.c_str(), 10, 74, &rlcd_font_bold_26);
        lv_obj_t *meta = text(card, "", 10, 108, &rlcd_font_bold_14);
        lv_label_set_text_fmt(meta, "%luK - %u SESS", (unsigned long)agents[i]->tokens_k, agents[i]->sessions);
    }
    footer(pages_[2], "24H LOCAL LOGS", "CODEX + CLAUDE");
}

void MonitorUi::create_processes(const MonitorModel &m) {
    pages_[3] = lv_obj_create(lv_screen_active()); base(pages_[3], "TOP - BY CPU", 4); source_tag(pages_[3], m);
    for (int i = 0; i < 4; ++i) {
        const ProcessUsage &p = m.processes[i];
        lv_obj_t *row = panel(pages_[3], 0, 32 + i * 60, 400, 58);
        lv_obj_t *name = text(row, p.name.empty() ? "-" : p.name.c_str(), 11, 5, &rlcd_font_bold_26);
        lv_obj_set_width(name, 260); lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        char process_meta[32]; snprintf(process_meta, sizeof(process_meta), "%.1fG - %.1f%%", p.ram_mb / 1024.0f,
                                        m.ram_total_gb > 0 ? p.ram_mb / 1024.0f * 100.0f / m.ram_total_gb : 0.0f);
        text(row, "RAM", 12, 38, &rlcd_font_bold_14); text(row, process_meta, 50, 38, &rlcd_font_bold_14);
        char process_cpu[16]; snprintf(process_cpu, sizeof(process_cpu), "%.0f%%", p.cpu_percent);
        lv_obj_t *cpu = text(row, process_cpu, 0, 6, &rlcd_font_bold_40);
        lv_obj_set_width(cpu, 120); lv_obj_set_style_text_align(cpu, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(cpu, LV_ALIGN_TOP_RIGHT, -10, 5);
    }
    char count[24]; snprintf(count, sizeof(count), "4 OF %u", m.process_count);
    footer(pages_[3], count, "CPU + RAM");
}

void MonitorUi::create_news(const MonitorModel &m) {
    pages_[4] = lv_obj_create(lv_screen_active()); base(pages_[4], "TECH NEWS", 5); source_tag(pages_[4], m);
    for (int i = 0; i < 4; ++i) {
        int index = news_page_ * 4 + i;
        lv_obj_t *row = panel(pages_[4], 0, 32 + i * 60, 400, 58);
        lv_obj_t *headline = text(row, m.headlines[index].c_str(), 10, 6, &rlcd_font_bold_18);
        lv_obj_set_width(headline, 325); lv_obj_set_height(headline, 48);
        lv_label_set_long_mode(headline, LV_LABEL_LONG_WRAP);
        lv_obj_t *age = text(row, m.headline_ages[index].c_str(), 0, 19, &rlcd_font_bold_14);
        lv_obj_set_width(age, 55); lv_obj_set_style_text_align(age, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(age, LV_ALIGN_RIGHT_MID, -9, 0);
    }
    char page[32]; snprintf(page, sizeof(page), "%d OF 2 PAGES", news_page_ + 1);
    footer(pages_[4], page, "HOLD KEY: MORE");
}

void MonitorUi::create_sound(const MonitorModel &m) {
    pages_[5] = lv_obj_create(lv_screen_active()); base(pages_[5], "SOUND", 6);
    const AudioLevels &n = m.noise;

    lv_obj_t *card = panel(pages_[5], 0, 32, 400, 84);
    lv_obj_t *value = text(card, "--", 12, -2, &rlcd_font_bold_72);
    if (n.current) lv_label_set_text_fmt(value, "%u", n.current);
    text(card, "DB SPL", 150, 44, &rlcd_font_bold_18);
    const char *names[] = {"MIN", "AVG", "MAX", "PEAK"};
    const uint8_t stats[] = {n.minimum, n.average, n.maximum, n.peak};
    for (int i = 0; i < 4; ++i) {
        const int x = 236 + (i % 2) * 84;
        const int y = 8 + (i / 2) * 38;
        text(card, names[i], x, y, &rlcd_font_bold_14);
        lv_obj_t *item = text(card, "--", x, y + 15, &rlcd_font_bold_18);
        if (stats[i]) lv_label_set_text_fmt(item, "%u", stats[i]);
    }

    // One bar per measured window, oldest on the left. The scale is fixed so a
    // quiet room and a loud one are comparable at a glance.
    constexpr int kFloor = 20;
    constexpr int kRange = 80;
    constexpr int kGraphHeight = 138;
    lv_obj_t *graph = panel(pages_[5], 0, 120, 400, kGraphHeight);
    for (int line = 1; line < 4; ++line) {
        rule(graph, 0, kGraphHeight - line * kGraphHeight / 4, 400);
    }
    const int bar_width = 5;
    for (size_t i = 0; i < n.filled && i < kNoiseHistory; ++i) {
        const int level = std::clamp(static_cast<int>(n.history[i]) - kFloor, 1, kRange);
        const int height = level * (kGraphHeight - 4) / kRange;
        panel(graph, 9 + static_cast<int>(i) * (bar_width + 1), kGraphHeight - height, bar_width,
              height, BLACK);
    }
    if (!n.filled) text(graph, "LISTENING", 12, 8, &rlcd_font_bold_18);

    char span[28];
    snprintf(span, sizeof(span), "%u WINDOWS", static_cast<unsigned>(n.filled));
    footer(pages_[5], span, "BOOT: BACK");
}

void MonitorUi::create_devices(const MonitorModel &m) {
    pages_[6] = lv_obj_create(lv_screen_active()); base(pages_[6], "DEVICES", 7);
    int rows = 0;
    int live = 0;
    for (const SourceStatus &source : m.sources) {
        if (!source.has_data) continue;
        if (source.connected) ++live;
        lv_obj_t *row = panel(pages_[6], 0, 32 + rows * 78, 400, 74);
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
    if (!rows) text(pages_[6], "WAITING FOR A HOST", 12, 48, &rlcd_font_bold_18, WHITE);
    char count[24];
    snprintf(count, sizeof(count), "%d LIVE OF %d", live, rows);
    footer(pages_[6], "HOLD KEY: SWITCH", count);
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
    if (current_page_ != 4) return;
    news_page_ = 1 - news_page_;
    lv_obj_delete(pages_[4]); pages_[4] = nullptr;
    create_news(m); lv_obj_invalidate(pages_[4]);
}

void MonitorUi::update(const MonitorModel &m) {
    for (auto &page : pages_) { if (page) lv_obj_delete(page); page = nullptr; }
    create(m);
}
