#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Rolling level history for the sound page, oldest first.
constexpr std::size_t kNoiseHistory = 64;

struct AudioLevels {
    uint8_t current;
    uint8_t minimum;
    uint8_t average;
    uint8_t maximum;
    uint8_t peak;
    uint8_t filled;
    std::array<uint8_t, kNoiseHistory> history;
};

void audio_meter_init();
uint8_t audio_meter_db();
AudioLevels audio_meter_levels();

// How often a window is measured. Between windows the codec and the I2S clocks
// are shut down, so this is the main cost of the microphone.
void audio_meter_set_period_ms(uint32_t period_ms);

// dB added to the measured level to reach an approximate SPL. The microphone
// has no published sensitivity, so this is calibrated against a reference.
void audio_meter_set_offset_db(float offset_db);
