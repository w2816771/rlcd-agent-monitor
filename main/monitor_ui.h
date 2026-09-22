#pragma once
#include "lvgl.h"
#include "monitor_model.h"

class MonitorUi {
public:
    // Overview, CPU, trends, processes, agent usage, news, todo, devices,
    // debug. The sound history lives on the overview. Nine pages is a lot for
    // two buttons, but each of the three hardware pages is full.
    static constexpr int kPageCount = 9;
    static constexpr int kPageCpu = 1;
    static constexpr int kPageTrends = 2;
    static constexpr int kPageProcesses = 3;
    static constexpr int kPageNews = 5;
    static constexpr int kPageTodo = 6;
    static constexpr int kPageDevices = 7;
    static constexpr int kPageDebug = 8;
    void create(const MonitorModel &model);
    void show_next();
    void show_previous();
    void show_home();
    void handle_long_press(const MonitorModel &model);
    void update(const MonitorModel &model);
    int current_page() const { return current_page_; }
    // A centred box above whatever page is showing. It lives on the top layer,
    // so rebuilding the pages underneath leaves it in place.
    void show_alert(const char *title);
    void hide_alert();
    bool alert_visible() const { return alert_ != nullptr; }
private:
    lv_obj_t *pages_[kPageCount]{};
    lv_obj_t *status_[kPageCount]{};
    lv_obj_t *alert_ = nullptr;
    int current_page_ = 0;
    int news_page_ = 0;
    // Index of the oldest log line on screen, so KEY held can page through.
    int debug_scroll_ = 0;
    // Which ordering the process rows use. KEY held cycles it.
    ProcessSort process_sort_ = ProcessSort::Cpu;
    void create_overview(const MonitorModel &model);
    void create_cpu(const MonitorModel &model);
    void create_trends(const MonitorModel &model);
    void create_processes(const MonitorModel &model);
    void create_activity(const MonitorModel &model);
    void create_news(const MonitorModel &model);
    void create_todo(const MonitorModel &model);
    void create_devices(const MonitorModel &model);
    void create_debug(const MonitorModel &model);
};
