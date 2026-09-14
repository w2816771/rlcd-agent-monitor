#pragma once

#include <cstddef>

// Every ESP_LOG line is copied into a small ring buffer so the board can show
// its own log without a serial cable. Crash output still goes to the coredump
// partition; this is for the running system.
constexpr std::size_t kLogLines = 64;
constexpr std::size_t kLogLineChars = 56;

// Installs the log hook. Call once, before anything worth reading is logged.
void debug_log_init();

// Number of lines currently retained, at most kLogLines.
std::size_t debug_log_count();

// Copies retained line `index`, 0 being the oldest still held, into `out`.
// Safe to call from any task; returns an empty string for a bad index.
void debug_log_copy(std::size_t index, char *out, std::size_t size);

// Total lines ever written, so a reader can tell that lines were dropped.
unsigned long debug_log_total();
