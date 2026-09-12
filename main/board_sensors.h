#pragma once

#include <cstdint>

struct BoardSensorReadings {
    float temperature_c;
    uint8_t humidity_percent;
    uint8_t battery_percent;
    float battery_voltage;
    bool climate_valid;
    bool battery_valid;
};

void board_sensors_init();
BoardSensorReadings board_sensors_read();
