#include "audio_player.h"

#include <atomic>
#include <cmath>
#include <cstring>

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "es8311_codec.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_meter.h"

namespace {
const char *TAG = "audio-player";

// Same rate as the microphone so nothing about the shared clocks is a surprise.
constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 2;
// GPIO46 enables the amplifier, active high, per Waveshare's board profile.
constexpr int PA_PIN = 46;
constexpr gpio_num_t DOUT_PIN = GPIO_NUM_8;
// A quarter of full scale. The header takes a small speaker and a square wave
// has all its energy in the fundamental and odd harmonics; louder clips.
constexpr int16_t AMPLITUDE = 8000;
// Pause between repeats, so it rings rather than screams.
constexpr int REPEAT_GAP_MS = 2500;

struct Note {
    float hertz;  // 0 is a rest.
    int ms;
};

// A rising figure that resolves, the kind of thing a Game Boy would say. Short
// enough that the repeat gap dominates.
constexpr Note JINGLE[] = {
    {659.25f, 90}, {0.0f, 20}, {783.99f, 90}, {0.0f, 20}, {1046.50f, 90}, {0.0f, 20},
    {1318.51f, 140}, {0.0f, 60}, {1046.50f, 90}, {0.0f, 20}, {1318.51f, 220},
};

std::atomic<bool> want_ring{false};
std::atomic<bool> ringing{false};
TaskHandle_t player_task_handle = nullptr;

// Writes one note as a square wave. Returns false if playback should stop.
bool play_note(esp_codec_dev_handle_t device, const Note &note) {
    int16_t block[256 * CHANNELS];
    constexpr int frames_per_block = sizeof(block) / sizeof(block[0]) / CHANNELS;
    int frames_left = SAMPLE_RATE * note.ms / 1000;
    const int period = note.hertz > 0.0f ? static_cast<int>(SAMPLE_RATE / note.hertz) : 0;
    int phase = 0;
    while (frames_left > 0) {
        if (!want_ring.load()) return false;
        const int frames = frames_left < frames_per_block ? frames_left : frames_per_block;
        for (int frame = 0; frame < frames; ++frame) {
            int16_t value = 0;
            if (period > 0) {
                value = (phase < period / 2) ? AMPLITUDE : -AMPLITUDE;
                if (++phase >= period) phase = 0;
            }
            block[frame * CHANNELS] = value;
            block[frame * CHANNELS + 1] = value;
        }
        if (esp_codec_dev_write(device, block, frames * CHANNELS * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            return false;
        }
        frames_left -= frames;
    }
    return true;
}

void player_task(void *) {
    while (true) {
        // Parked until something asks for a ring; no clocks, no PA, no cost.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!want_ring.load()) continue;

        // The meter holds the bus while it measures; a window is a quarter of a
        // second, so this rarely waits long, and a ring is worth a short wait.
        if (!audio_meter_take_bus(3000)) {
            ESP_LOGW(TAG, "Audio bus busy; alert stays silent");
            continue;
        }
        ringing.store(true);

        i2s_chan_handle_t tx = nullptr;
        i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        esp_codec_dev_handle_t device = nullptr;
        const audio_codec_ctrl_if_t *control = nullptr;
        const audio_codec_gpio_if_t *gpio = nullptr;
        const audio_codec_if_t *codec = nullptr;
        const audio_codec_data_if_t *data = nullptr;
        bool ready = false;
        do {
            if (i2s_new_channel(&channel_config, &tx, nullptr) != ESP_OK) {
                ESP_LOGE(TAG, "I2S tx channel failed");
                break;
            }
            i2s_std_config_t std_config = {
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
                .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .mclk = GPIO_NUM_16,
                    .bclk = GPIO_NUM_9,
                    .ws = GPIO_NUM_45,
                    .dout = DOUT_PIN,
                    .din = I2S_GPIO_UNUSED,
                    .invert_flags = {},
                },
            };
            if (i2s_channel_init_std_mode(tx, &std_config) != ESP_OK) {
                ESP_LOGE(TAG, "I2S std init failed");
                break;
            }
            i2c_master_bus_handle_t i2c_bus = nullptr;
            i2c_master_get_bus_handle(I2C_NUM_0, &i2c_bus);
            audio_codec_i2c_cfg_t i2c_config{};
            i2c_config.port = I2C_NUM_0;
            i2c_config.addr = ES8311_CODEC_DEFAULT_ADDR;
            i2c_config.bus_handle = i2c_bus;
            control = audio_codec_new_i2c_ctrl(&i2c_config);
            gpio = audio_codec_new_gpio();
            es8311_codec_cfg_t es8311_config{};
            es8311_config.ctrl_if = control;
            es8311_config.gpio_if = gpio;
            es8311_config.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
            es8311_config.pa_pin = PA_PIN;
            es8311_config.pa_reverted = false;
            es8311_config.use_mclk = true;
            es8311_config.hw_gain.pa_voltage = 5.0f;
            es8311_config.hw_gain.codec_dac_voltage = 3.3f;
            es8311_config.hw_gain.pa_gain = 6.0f;
            codec = es8311_codec_new(&es8311_config);
            audio_codec_i2s_cfg_t data_config{};
            data_config.port = I2S_NUM_0;
            data_config.tx_handle = tx;
            data = audio_codec_new_i2s_data(&data_config);
            esp_codec_dev_cfg_t device_config{};
            device_config.codec_if = codec;
            device_config.data_if = data;
            device_config.dev_type = ESP_CODEC_DEV_TYPE_OUT;
            device = esp_codec_dev_new(&device_config);
            esp_codec_dev_sample_info_t sample{};
            sample.sample_rate = SAMPLE_RATE;
            sample.channel = CHANNELS;
            sample.bits_per_sample = 16;
            if (!device || esp_codec_dev_open(device, &sample) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(TAG, "Speaker codec open failed");
                break;
            }
            esp_codec_dev_set_out_vol(device, 70);
            ready = true;
        } while (false);

        if (ready) {
            ESP_LOGI(TAG, "Ringing");
            while (want_ring.load()) {
                bool keep = true;
                for (const Note &note : JINGLE) {
                    if (!play_note(device, note)) { keep = false; break; }
                }
                if (!keep) break;
                // Silence between repeats is still written, so the DAC does not
                // pop as the amplifier idles.
                if (!play_note(device, Note{0.0f, REPEAT_GAP_MS})) break;
            }
            esp_codec_dev_close(device);
        }
        // Torn down every time rather than kept, so between alerts the DAC and
        // amplifier are fully off and the bus is the microphone's again.
        if (device) esp_codec_dev_delete(device);
        if (data) audio_codec_delete_data_if(data);
        if (codec) audio_codec_delete_codec_if(codec);
        if (control) audio_codec_delete_ctrl_if(control);
        if (gpio) audio_codec_delete_gpio_if(gpio);
        if (tx) i2s_del_channel(tx);
        ringing.store(false);
        audio_meter_give_bus();
        ESP_LOGI(TAG, "Quiet");
    }
}
}  // namespace

void audio_player_init() {
    xTaskCreatePinnedToCore(player_task, "audio-player", 6144, nullptr, 4, &player_task_handle, 0);
}

void audio_player_ring() {
    want_ring.store(true);
    if (player_task_handle) xTaskNotifyGive(player_task_handle);
}

void audio_player_stop() { want_ring.store(false); }

bool audio_player_ringing() { return ringing.load(); }
