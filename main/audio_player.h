#pragma once

#include <cstdint>

// The alert sound: a short square wave jingle synthesised on the chip, in the
// eight bit style, played through the ES8311 and the speaker header. Nothing is
// stored as audio; a note is a frequency and a length.
//
// The DAC shares the I2S clock pins with the microphone ADC, so playback takes
// the audio bus from the meter for as long as it rings.
void audio_player_init();

// Starts ringing: the jingle repeats every few seconds until stopped, which is
// what a reminder that has to be dismissed by hand needs. Safe to call again
// while already ringing.
void audio_player_ring();

// Stops at the end of the current note and gives the bus back.
void audio_player_stop();

bool audio_player_ringing();
