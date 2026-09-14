#pragma once
#include "lvgl.h"
#include "monitor_model.h"

class MonitorUi {
public:
    // Overview, host system, agent usage, news, devices, debug. Processes live
    // on the system page and the sound history on the overview, because a page
    // costs button presses on a two button board.
    static constexpr int kPageCount = 6;
    static constexpr int kPageSystem = 1;
    static constexpr int kPageNews = 3;
    static constexpr int kPageDevices = 4;
    static constexpr int kPageDebug = 5;
    void create(const MonitorModel &model);
    void show_next();
    void show_previous();
    void show_home();
    void handle_long_press(const MonitorModel &model);
    void update(const MonitorModel &model);
    int current_page() const { return current_page_; }
private:
    lv_obj_t *pages_[kPageCount]{};
    lv_obj_t *status_[kPageCount]{};
    int current_page_ = 0;
    int news_page_ = 0;
    // Index of the oldest log line on screen, so KEY held can page through.
    int debug_scroll_ = 0;
    // Which ordering the process rows use. KEY held cycles it.
    ProcessSort process_sort_ = ProcessSort::Cpu;
    void create_overview(const MonitorModel &model);
    void create_system(const MonitorModel &model);
    void create_activity(const MonitorModel &model);
    void create_news(const MonitorModel &model);
    void create_devices(const MonitorModel &model);
    void create_debug(const MonitorModel &model);
};
