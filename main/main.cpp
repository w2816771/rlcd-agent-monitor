#include <atomic>
#include <cstdlib>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "display_bsp.h"
#include "board_sensors.h"
#include "audio_meter.h"
#include "monitor_ble.h"
#include "monitor_model.h"
#include "monitor_ui.h"

namespace {
constexpr int WIDTH = 400;
constexpr int HEIGHT = 300;
constexpr gpio_num_t KEY_PIN = GPIO_NUM_18;
constexpr gpio_num_t BOOT_PIN = GPIO_NUM_0;
DisplayPort display(12, 11, 5, 40, 41, WIDTH, HEIGHT);
MonitorUi ui;
MonitorModel model;
std::atomic_bool next_page_requested{false};
std::atomic_bool long_press_requested{false};
std::atomic_bool previous_page_requested{false};
std::atomic_bool home_requested{false};

uint32_t tick_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixels) {
    auto *px = reinterpret_cast<uint16_t *>(pixels);
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            const uint16_t color = *px++;
            const uint32_t r = ((color >> 11) & 0x1f) * 255 / 31;
            const uint32_t g = ((color >> 5) & 0x3f) * 255 / 63;
            const uint32_t b = (color & 0x1f) * 255 / 31;
            const uint32_t luminance = (r * 299 + g * 587 + b * 114) / 1000;
            display.RLCD_SetPixel(x, y, luminance < 128 ? ColorBlack : ColorWhite);
        }
    }
    display.RLCD_Display();
    lv_display_flush_ready(disp);
}

void button_task(void *) {
    int previous_key = 1;
    int previous_boot = 1;
    int64_t key_pressed_at = 0;
    int64_t boot_pressed_at = 0;
    while (true) {
        const int key = gpio_get_level(KEY_PIN);
        const int boot = gpio_get_level(BOOT_PIN);
        if (previous_key == 1 && key == 0) key_pressed_at = esp_timer_get_time();
        if (previous_key == 0 && key == 1) {
            if (esp_timer_get_time() - key_pressed_at >= 700000) long_press_requested.store(true);
            else next_page_requested.store(true);
        }
        if (previous_boot == 1 && boot == 0) boot_pressed_at = esp_timer_get_time();
        if (previous_boot == 0 && boot == 1) {
            if (esp_timer_get_time() - boot_pressed_at >= 700000) home_requested.store(true);
            else previous_page_requested.store(true);
        }
        previous_key = key;
        previous_boot = boot;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void lvgl_task(void *) {
    bool last_connected = false;
    int64_t last_sensor_update = 0;
    while (true) {
        if (next_page_requested.exchange(false)) ui.show_next();
        if (previous_page_requested.exchange(false)) ui.show_previous();
        if (home_requested.exchange(false)) ui.show_home();
        if (long_press_requested.exchange(false)) ui.handle_long_press(model);
        MonitorModel next;
        if (monitor_ble_take_update(next)) {
            model = std::move(next);
            model.bluetooth_connected = monitor_ble_connected();
            const BoardSensorReadings sensors = board_sensors_read();
            if (sensors.battery_valid) model.board_battery_percent = sensors.battery_percent;
            if (sensors.climate_valid) {
                model.room_temp_c = sensors.temperature_c;
                model.room_humidity_percent = sensors.humidity_percent;
            }
            model.noise_db = audio_meter_db();
            ui.update(model);
        } else if (monitor_ble_connected() != last_connected) {
            model.bluetooth_connected = monitor_ble_connected();
            ui.update(model);
        }
        const int64_t now = esp_timer_get_time();
        if (now - last_sensor_update >= 5000000) {
            last_sensor_update = now;
            const BoardSensorReadings sensors = board_sensors_read();
            bool changed = false;
            if (sensors.battery_valid && model.board_battery_percent != sensors.battery_percent) {
                model.board_battery_percent = sensors.battery_percent; changed = true;
            }
            if (sensors.climate_valid &&
                (model.room_temp_c != sensors.temperature_c || model.room_humidity_percent != sensors.humidity_percent)) {
                model.room_temp_c = sensors.temperature_c;
                model.room_humidity_percent = sensors.humidity_percent;
                changed = true;
            }
            const uint8_t noise = audio_meter_db();
            if (noise && model.noise_db != noise) { model.noise_db = noise; changed = true; }
            if (changed) ui.update(model);
        }
        last_connected = monitor_ble_connected();
        uint32_t wait = lv_timer_handler();
        if (wait < 5) wait = 5;
        if (wait > 20) wait = 20;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}
}

extern "C" void app_main() {
    ESP_LOGI("agent-monitor", "Starting 400x300 RLCD agent monitor");
    display.RLCD_Init();
    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *lv_disp = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lv_disp, flush);
    constexpr size_t frame_bytes = WIDTH * HEIGHT * 2;
    auto *buf1 = static_cast<uint8_t *>(heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM));
    auto *buf2 = static_cast<uint8_t *>(heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM));
    configASSERT(buf1 && buf2);
    lv_display_set_buffers(lv_disp, buf1, buf2, frame_bytes, LV_DISPLAY_RENDER_MODE_FULL);

    gpio_config_t key{};
    key.pin_bit_mask = (1ULL << KEY_PIN) | (1ULL << BOOT_PIN);
    key.mode = GPIO_MODE_INPUT;
    key.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&key));

    model = monitor_demo_model();
    model.bluetooth_connected = false;
    ui.create(model);
    board_sensors_init();
    audio_meter_init();
    monitor_ble_init();
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(button_task, "button", 3072, nullptr, 4, nullptr, 0);
}
