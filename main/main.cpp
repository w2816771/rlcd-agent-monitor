#include <atomic>
#include <cstdlib>

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "debug_log.h"
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
    // Only the changed band reaches the panel, and an unchanged frame sends
    // nothing at all, which is the common case when a label was rebuilt with
    // the same text.
    display.RLCD_DisplayChanged();
    // RLCD_SetLowPower(true) belongs here, but 0x39 is read from the MIPI idle
    // mode convention rather than a datasheet to hand, so it is enabled only
    // once the layout above is confirmed good on the panel. One change at a
    // time is the difference between a bug and a mystery.
    lv_display_flush_ready(disp);
}

QueueHandle_t button_queue;

// The pins wake the chip from light sleep, which only supports level triggers,
// so the handler masks its own source and the task re-arms it after release.
void IRAM_ATTR button_isr(void *argument) {
    const auto pin = static_cast<gpio_num_t>(reinterpret_cast<intptr_t>(argument));
    gpio_intr_disable(pin);
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(button_queue, &pin, &woken);
    if (woken) portYIELD_FROM_ISR();
}

void button_task(void *) {
    gpio_num_t pin;
    while (true) {
        if (xQueueReceive(button_queue, &pin, portMAX_DELAY) != pdTRUE) continue;
        const int64_t pressed_at = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(20));
        if (gpio_get_level(pin) == 0) {
            while (gpio_get_level(pin) == 0) vTaskDelay(pdMS_TO_TICKS(20));
            const bool long_press = esp_timer_get_time() - pressed_at >= 700000;
            if (pin == KEY_PIN) {
                if (long_press) long_press_requested.store(true);
                else next_page_requested.store(true);
            } else {
                if (long_press) home_requested.store(true);
                else previous_page_requested.store(true);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        xQueueReset(button_queue);
        gpio_intr_enable(pin);
    }
}

void lvgl_task(void *) {
    bool last_connected = false;
    int64_t last_sensor_update = 0;
    uint32_t last_stale_minutes = 0;
    unsigned long last_log_total = 0;
    auto apply_sources = [](MonitorModel &target) {
        target.sources = monitor_ble_sources();
        target.source_live = false;
        target.source_age_seconds = 0;
        for (const SourceStatus &source : target.sources) {
            if (!source.shown) continue;
            target.source_name = source.name;
            target.source_live = source.connected;
            target.source_age_seconds = source.age_seconds;
        }
    };
    while (true) {
        // The page reported is the one on screen when the button went down, so
        // the host can act on what the user was looking at.
        const uint8_t page = static_cast<uint8_t>(ui.current_page() + 1);
        if (next_page_requested.exchange(false)) {
            monitor_ble_notify_button(1, page);
            ui.show_next();
        }
        if (previous_page_requested.exchange(false)) {
            monitor_ble_notify_button(3, page);
            ui.show_previous();
        }
        if (home_requested.exchange(false)) {
            monitor_ble_notify_button(4, page);
            ui.show_home();
        }
        if (long_press_requested.exchange(false)) {
            // KEY held flips headlines on the news page and switches the shown
            // source everywhere else. A switch is not forwarded as a button
            // event, so a command mapped to KEY held cannot fire on every switch.
            if (ui.current_page() == MonitorUi::kPageNews ||
                ui.current_page() == MonitorUi::kPageDebug) {
                monitor_ble_notify_button(2, page);
                ui.handle_long_press(model);
            } else {
                monitor_ble_show_next_source();
            }
        }
        monitor_ble_flush_events();

        // Full function while a USB host is attached, power save on battery.
        // The same connection already holds light sleep off, so the mode only
        // has to decide how often the board wakes to do work.
        const bool usb = usb_serial_jtag_is_connected();
        // The debug log is the only page that changes on its own between
        // snapshots, so it alone earns a faster refresh, and only on USB.
        const bool watching_debug = ui.current_page() == MonitorUi::kPageDebug;
        const bool watching_devices = ui.current_page() == MonitorUi::kPageDevices;
        const int64_t sensor_period = usb ? (watching_debug ? 3000000 : 10000000) : 60000000;
        audio_meter_set_period_ms(usb ? 2000 : 30000);
        const uint32_t wait_floor = usb ? 20 : 50;
        const uint32_t wait_ceiling = usb ? 50 : 150;

        MonitorModel next;
        if (monitor_ble_take_update(next)) {
            model = std::move(next);
            model.bluetooth_connected = monitor_ble_connected();
            model.bluetooth_rssi = monitor_ble_rssi();
            model.usb_connected = usb;
            apply_sources(model);
            const BoardSensorReadings sensors = board_sensors_read();
            if (sensors.battery_valid) model.board_battery_percent = sensors.battery_percent;
            if (sensors.climate_valid) {
                model.room_temp_c = sensors.temperature_c;
                model.room_humidity_percent = sensors.humidity_percent;
            }
            model.noise_db = audio_meter_db();
            model.noise = audio_meter_levels();
            audio_meter_set_offset_db(model.noise_offset_db);
            ui.update(model);
        } else if (monitor_ble_connected() != last_connected || model.usb_connected != usb) {
            model.bluetooth_connected = monitor_ble_connected();
            model.bluetooth_rssi = monitor_ble_rssi();
            model.usb_connected = usb;
            apply_sources(model);
            ui.update(model);
        }
        const int64_t now = esp_timer_get_time();
        if (now - last_sensor_update >= sensor_period) {
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
            const int noise_delta = static_cast<int>(model.noise_db) - static_cast<int>(noise);
            if (noise && (noise_delta <= -3 || noise_delta >= 3)) { model.noise_db = noise; changed = true; }
            model.noise = audio_meter_levels();
            apply_sources(model);
            // The devices page lists live signal and update age, and a stale
            // header counts up in minutes, so both need periodic redraws.
            const uint32_t stale_minutes = model.source_live ? 0 : model.source_age_seconds / 60;
            if (watching_devices || stale_minutes != last_stale_minutes) changed = true;
            // New lines only reach the screen if something asks for a redraw.
            if (watching_debug && debug_log_total() != last_log_total) {
                last_log_total = debug_log_total();
                changed = true;
            }
            last_stale_minutes = stale_minutes;
            if (changed) ui.update(model);
        }
        last_connected = monitor_ble_connected();
        uint32_t wait = lv_timer_handler();
        if (wait < wait_floor) wait = wait_floor;
        if (wait > wait_ceiling) wait = wait_ceiling;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}
}

extern "C" void app_main() {
    // Installed before the first log line, so the debug page can show the whole
    // boot sequence and not just what happened after the UI came up.
    debug_log_init();
    ESP_LOGI("agent-monitor", "Starting 400x300 RLCD agent monitor");

    // Automatic light sleep is not implied by dynamic frequency scaling; it has
    // to be requested here and needs tickless idle in sdkconfig to take effect.
    esp_pm_config_t power{};
    power.max_freq_mhz = 80;
    power.min_freq_mhz = 40;
    power.light_sleep_enable = true;
    ESP_ERROR_CHECK(esp_pm_configure(&power));

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
    key.intr_type = GPIO_INTR_LOW_LEVEL;
    ESP_ERROR_CHECK(gpio_config(&key));
    button_queue = xQueueCreate(4, sizeof(gpio_num_t));
    configASSERT(button_queue);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    for (const gpio_num_t pin : {KEY_PIN, BOOT_PIN}) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(pin, button_isr, reinterpret_cast<void *>(pin)));
        ESP_ERROR_CHECK(gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL));
    }
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    model = monitor_demo_model();
    model.bluetooth_connected = false;
    model.usb_connected = usb_serial_jtag_is_connected();
    ui.create(model);
    board_sensors_init();
    audio_meter_init();
    monitor_ble_init();
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(button_task, "button", 3072, nullptr, 4, nullptr, 0);
}
