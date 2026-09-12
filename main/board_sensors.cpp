#include "board_sensors.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include "driver/i2c_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char TAG[] = "board-sensors";
constexpr gpio_num_t SDA_PIN = GPIO_NUM_13;
constexpr gpio_num_t SCL_PIN = GPIO_NUM_14;
constexpr uint8_t SHTC3_ADDRESS = 0x70;

i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t shtc3;
adc_oneshot_unit_handle_t adc;
adc_cali_handle_t calibration;
std::atomic<float> room_temp{0.0f};
std::atomic<uint8_t> humidity{0};
std::atomic<uint8_t> battery{0};
std::atomic<float> battery_voltage{0.0f};
std::atomic_bool climate_valid{false};
std::atomic_bool battery_valid{false};

uint8_t crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0xff;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
    }
    return crc;
}

void send_command(uint16_t command) {
    uint8_t bytes[] = {static_cast<uint8_t>(command >> 8), static_cast<uint8_t>(command)};
    i2c_master_transmit(shtc3, bytes, sizeof(bytes), 100);
}

void update_climate() {
    if (!shtc3) return;
    send_command(0x3517);
    vTaskDelay(pdMS_TO_TICKS(50));
    const uint8_t command[] = {0x78, 0x66};
    uint8_t result[6]{};
    esp_err_t error = i2c_master_transmit(shtc3, command, sizeof(command), 100);
    vTaskDelay(pdMS_TO_TICKS(20));
    if (error == ESP_OK) error = i2c_master_receive(shtc3, result, sizeof(result), 100);
    if (error != ESP_OK || crc8(result, 2) != result[2] || crc8(result + 3, 2) != result[5]) {
        climate_valid.store(false);
        return;
    }
    const uint16_t raw_temp = (result[0] << 8) | result[1];
    const uint16_t raw_humidity = (result[3] << 8) | result[4];
    room_temp.store(175.0f * raw_temp / 65536.0f - 45.0f - 4.0f);
    humidity.store(static_cast<uint8_t>(std::clamp(std::lround(100.0f * raw_humidity / 65536.0f), 0L, 100L)));
    climate_valid.store(true);
    send_command(0xb098);
}

void update_battery() {
    if (!adc) return;
    int raw = 0;
    if (adc_oneshot_read(adc, ADC_CHANNEL_3, &raw) != ESP_OK) return;
    int millivolts = 0;
    if (calibration && adc_cali_raw_to_voltage(calibration, raw, &millivolts) == ESP_OK) {
        const float volts = millivolts * 0.003f;
        battery_voltage.store(volts);
        battery.store(static_cast<uint8_t>(std::clamp(std::lround((volts - 3.0f) * 100.0f / 1.12f), 0L, 100L)));
        battery_valid.store(true);
    }
}

void sensor_task(void *) {
    while (true) {
        update_climate();
        update_battery();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
}  // namespace

void board_sensors_init() {
    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = SDA_PIN;
    bus_config.scl_io_num = SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus_config, &i2c_bus) == ESP_OK) {
        i2c_device_config_t device_config{};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = SHTC3_ADDRESS;
        device_config.scl_speed_hz = 400000;
        if (i2c_master_bus_add_device(i2c_bus, &device_config, &shtc3) == ESP_OK) {
            send_command(0x3517);
            vTaskDelay(pdMS_TO_TICKS(50));
            send_command(0x805d);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    adc_oneshot_unit_init_cfg_t adc_config{};
    adc_config.unit_id = ADC_UNIT_1;
    if (adc_oneshot_new_unit(&adc_config, &adc) == ESP_OK) {
        adc_oneshot_chan_cfg_t channel_config{};
        channel_config.bitwidth = ADC_BITWIDTH_12;
        channel_config.atten = ADC_ATTEN_DB_12;
        adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &channel_config);
        adc_cali_curve_fitting_config_t cal_config{};
        cal_config.unit_id = ADC_UNIT_1;
        cal_config.chan = ADC_CHANNEL_3;
        cal_config.atten = ADC_ATTEN_DB_12;
        cal_config.bitwidth = ADC_BITWIDTH_12;
        adc_cali_create_scheme_curve_fitting(&cal_config, &calibration);
    }
    ESP_LOGI(TAG, "SHTC3 and battery ADC initialized");
    xTaskCreate(sensor_task, "sensors", 3072, nullptr, 3, nullptr);
}

BoardSensorReadings board_sensors_read() {
    return {room_temp.load(), humidity.load(), battery.load(), battery_voltage.load(),
            climate_valid.load(), battery_valid.load()};
}
