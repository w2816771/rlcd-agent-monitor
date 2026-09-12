#include "audio_meter.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "driver/i2c_master.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {
constexpr char TAG[] = "audio-meter";
constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 2;
// A quarter second is long enough to average out a keystroke or a single word
// without the window itself becoming a power cost.
constexpr int WINDOW_FRAMES = SAMPLE_RATE / 4;
constexpr int FLOOR_DB = 20;
constexpr int CEILING_DB = 120;

std::atomic<uint8_t> noise_db{0};
std::atomic<uint32_t> period_ms{30000};
std::atomic<int> offset_db{85};

SemaphoreHandle_t history_mutex;
std::array<uint8_t, kNoiseHistory> history{};
size_t history_filled = 0;
uint8_t peak_db = 0;

uint8_t to_spl(double rms) {
    if (rms < 0.5) return FLOOR_DB;
    const double dbfs = 20.0 * std::log10(rms / 32768.0);
    return static_cast<uint8_t>(
        std::clamp(static_cast<int>(std::lround(dbfs)) + offset_db.load(), FLOOR_DB, CEILING_DB));
}

void record(uint8_t measured, uint8_t measured_peak) {
    if (!history_mutex || xSemaphoreTake(history_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    if (history_filled < history.size()) {
        history[history_filled++] = measured;
    } else {
        std::rotate(history.begin(), history.begin() + 1, history.end());
        history.back() = measured;
    }
    peak_db = std::max(peak_db, measured_peak);
    xSemaphoreGive(history_mutex);
}

void meter_task(void *) {
    i2s_chan_handle_t rx = nullptr;
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&channel_config, nullptr, &rx) != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel initialization failed");
        vTaskDelete(nullptr);
        return;
    }
    i2s_tdm_config_t tdm_config = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO,
            static_cast<i2s_tdm_slot_mask_t>(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3)),
        .gpio_cfg = {
            .mclk = GPIO_NUM_16,
            .bclk = GPIO_NUM_9,
            .ws = GPIO_NUM_45,
            .dout = I2S_GPIO_UNUSED,
            .din = GPIO_NUM_10,
            .invert_flags = {},
        },
    };
    tdm_config.slot_cfg.total_slot = 4;
    if (i2s_channel_init_tdm_mode(rx, &tdm_config) != ESP_OK || i2s_channel_enable(rx) != ESP_OK) {
        ESP_LOGE(TAG, "I2S TDM initialization failed");
        vTaskDelete(nullptr);
        return;
    }

    i2c_master_bus_handle_t i2c_bus = nullptr;
    i2c_master_get_bus_handle(I2C_NUM_0, &i2c_bus);
    audio_codec_i2c_cfg_t i2c_config{};
    i2c_config.port = I2C_NUM_0;
    i2c_config.addr = ES7210_CODEC_DEFAULT_ADDR;
    i2c_config.bus_handle = i2c_bus;
    const audio_codec_ctrl_if_t *control = audio_codec_new_i2c_ctrl(&i2c_config);
    es7210_codec_cfg_t es7210_config{};
    es7210_config.ctrl_if = control;
    es7210_config.mic_selected = static_cast<uint8_t>(
        ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4);
    const audio_codec_if_t *codec = es7210_codec_new(&es7210_config);
    audio_codec_i2s_cfg_t data_config{};
    data_config.port = I2S_NUM_0;
    data_config.rx_handle = rx;
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&data_config);
    esp_codec_dev_cfg_t device_config{};
    device_config.codec_if = codec;
    device_config.data_if = data;
    device_config.dev_type = ESP_CODEC_DEV_TYPE_IN;
    esp_codec_dev_handle_t recorder = esp_codec_dev_new(&device_config);
    esp_codec_dev_sample_info_t sample{};
    sample.sample_rate = SAMPLE_RATE;
    sample.channel = CHANNELS;
    sample.bits_per_sample = 16;
    if (!recorder || esp_codec_dev_open(recorder, &sample) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Microphone open failed");
        vTaskDelete(nullptr);
        return;
    }
    esp_codec_dev_set_in_gain(recorder, 35.0f);
    int16_t samples[512]{};
    constexpr int frames_per_read = sizeof(samples) / sizeof(samples[0]) / CHANNELS;
    while (true) {
        // Each microphone sits at its own resting level, so the mean has to come
        // out per channel before the energy is measured. Squaring the raw sample
        // instead measures that offset, which swamps the sound entirely.
        double sums[CHANNELS] = {};
        double squares[CHANNELS] = {};
        int16_t lowest[CHANNELS] = {};
        int16_t highest[CHANNELS] = {};
        int frames = 0;
        while (frames < WINDOW_FRAMES) {
            if (esp_codec_dev_read(recorder, samples, sizeof(samples)) != ESP_CODEC_DEV_OK) break;
            for (int frame = 0; frame < frames_per_read; ++frame) {
                for (int channel = 0; channel < CHANNELS; ++channel) {
                    const int16_t value = samples[frame * CHANNELS + channel];
                    sums[channel] += value;
                    squares[channel] += static_cast<double>(value) * value;
                    lowest[channel] = std::min(lowest[channel], value);
                    highest[channel] = std::max(highest[channel], value);
                }
            }
            frames += frames_per_read;
        }

        if (frames > 0) {
            double energy = 0.0;
            double excursion = 0.0;
            for (int channel = 0; channel < CHANNELS; ++channel) {
                const double mean = sums[channel] / frames;
                const double variance = std::max(squares[channel] / frames - mean * mean, 0.0);
                energy += variance;
                excursion = std::max(excursion,
                                     std::max(highest[channel] - mean, mean - lowest[channel]));
            }
            const uint8_t measured = to_spl(std::sqrt(energy / CHANNELS));
            record(measured, to_spl(excursion));
            noise_db.store(measured);
        }

        // Shut the codec and the I2S clocks down between windows.
        esp_codec_dev_close(recorder);
        i2s_channel_disable(rx);
        vTaskDelay(pdMS_TO_TICKS(period_ms.load()));
        if (i2s_channel_enable(rx) != ESP_OK) {
            ESP_LOGW(TAG, "Microphone wake failed; retrying");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        if (esp_codec_dev_open(recorder, &sample) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "Microphone codec wake failed; retrying");
            i2s_channel_disable(rx);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        esp_codec_dev_set_in_gain(recorder, 35.0f);
    }
}
}  // namespace

void audio_meter_init() {
    history_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(meter_task, "audio-meter", 6144, nullptr, 3, nullptr, 0);
}

uint8_t audio_meter_db() { return noise_db.load(); }

void audio_meter_set_period_ms(uint32_t value) { period_ms.store(value); }

void audio_meter_set_offset_db(float value) {
    offset_db.store(static_cast<int>(std::lround(value)));
}

AudioLevels audio_meter_levels() {
    AudioLevels levels{};
    levels.current = noise_db.load();
    if (!history_mutex || xSemaphoreTake(history_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return levels;
    levels.filled = static_cast<uint8_t>(history_filled);
    levels.history = history;
    levels.peak = peak_db;
    if (history_filled) {
        unsigned total = 0;
        levels.minimum = 255;
        for (size_t i = 0; i < history_filled; ++i) {
            total += history[i];
            levels.minimum = std::min(levels.minimum, history[i]);
            levels.maximum = std::max(levels.maximum, history[i]);
        }
        levels.average = static_cast<uint8_t>(total / history_filled);
    }
    xSemaphoreGive(history_mutex);
    return levels;
}
