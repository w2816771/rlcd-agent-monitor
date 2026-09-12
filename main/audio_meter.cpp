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
#include "freertos/task.h"

namespace {
constexpr char TAG[] = "audio-meter";
std::atomic<uint8_t> noise_db{0};

void meter_task(void *) {
    i2s_chan_handle_t rx = nullptr;
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&channel_config, nullptr, &rx) != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel initialization failed");
        vTaskDelete(nullptr);
        return;
    }
    i2s_tdm_config_t tdm_config = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
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
    sample.sample_rate = 16000;
    sample.channel = 2;
    sample.bits_per_sample = 16;
    if (!recorder || esp_codec_dev_open(recorder, &sample) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Microphone open failed");
        vTaskDelete(nullptr);
        return;
    }
    esp_codec_dev_set_in_gain(recorder, 35.0f);
    int16_t samples[512]{};
    while (true) {
        if (esp_codec_dev_read(recorder, samples, sizeof(samples)) != ESP_CODEC_DEV_OK) continue;
        double sum = 0.0;
        for (int16_t value : samples) {
            const double normalized = value / 32768.0;
            sum += normalized * normalized;
        }
        const double rms = std::sqrt(sum / 512.0);
        // Approximate SPL. The +100 dB offset is a practical calibration for the onboard mic.
        const int db = rms > 0.00001 ? static_cast<int>(std::lround(20.0 * std::log10(rms) + 100.0)) : 20;
        const uint8_t measured = static_cast<uint8_t>(std::clamp(db, 20, 110));
        const uint8_t previous = noise_db.load();
        noise_db.store(previous ? static_cast<uint8_t>((previous * 3 + measured) / 4) : measured);
    }
}
}  // namespace

void audio_meter_init() { xTaskCreatePinnedToCore(meter_task, "audio-meter", 6144, nullptr, 3, nullptr, 0); }

uint8_t audio_meter_db() { return noise_db.load(); }
