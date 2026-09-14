#pragma once
#include "lvgl.h"
#include "monitor_model.h"

class MonitorUi {
public:
    static constexpr int kPageCount = 7;
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
    void create_overview(const MonitorModel &model);
    void create_system(const MonitorModel &model);
    void create_activity(const MonitorModel &model);
    void create_processes(const MonitorModel &model);
    void create_news(const MonitorModel &model);
    void create_sound(const MonitorModel &model);
    void create_devices(const MonitorModel &model);
};
