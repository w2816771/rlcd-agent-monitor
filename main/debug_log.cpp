#include "debug_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

namespace {

char lines[kLogLines][kLogLineChars];
std::size_t held = 0;      // Lines currently retained, at most kLogLines.
std::size_t next = 0;      // Ring slot the next line goes into.
unsigned long written = 0; // Every line ever taken, so drops are visible.

// A partial line waits here until its newline arrives, because one log call
// can arrive as several vprintf calls.
char pending[kLogLineChars];
std::size_t pending_length = 0;

portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

int (*forward)(const char *, va_list) = nullptr;

// The display fonts hold printable ASCII only, and log output carries colour
// escapes and tabs, so anything else becomes a space here rather than a blank
// box on the panel.
char printable(char c) {
    return (c >= 0x20 && c <= 0x7E) ? c : ' ';
}

// Caller holds the lock.
void commit_pending() {
    if (pending_length == 0) return;
    std::size_t end = pending_length;
    while (end > 0 && pending[end - 1] == ' ') --end;
    if (end > 0) {
        std::memcpy(lines[next], pending, end);
        lines[next][end] = '\0';
        next = (next + 1) % kLogLines;
        if (held < kLogLines) ++held;
        ++written;
    }
    pending_length = 0;
}

void take(const char *text, std::size_t length) {
    portENTER_CRITICAL(&lock);
    bool skipping_escape = false;
    for (std::size_t i = 0; i < length; ++i) {
        const char c = text[i];
        // Colour codes look like ESC [ 0;32m and would otherwise fill the line
        // with punctuation.
        if (skipping_escape) {
            if (c == 'm' || c == 'K') skipping_escape = false;
            continue;
        }
        if (c == 0x1B) {
            skipping_escape = true;
            continue;
        }
        if (c == '\n') {
            commit_pending();
            continue;
        }
        if (c == '\r') continue;
        // A line longer than the display width wraps into the next slot rather
        // than being cut, so nothing is silently lost.
        if (pending_length + 1 >= kLogLineChars) commit_pending();
        pending[pending_length++] = printable(c);
    }
    portEXIT_CRITICAL(&lock);
}

int capture(const char *format, va_list args) {
    // Formatted on the stack first: vsnprintf must not run inside the lock.
    char scratch[192];
    va_list copy;
    va_copy(copy, args);
    const int length = vsnprintf(scratch, sizeof(scratch), format, copy);
    va_end(copy);
    if (length > 0) {
        const std::size_t used = (static_cast<std::size_t>(length) < sizeof(scratch))
                                     ? static_cast<std::size_t>(length)
                                     : sizeof(scratch) - 1;
        take(scratch, used);
    }
    // Still print to UART, so a serial cable shows what it always did.
    return forward ? forward(format, args) : length;
}

}  // namespace

void debug_log_init() {
    if (forward) return;
    forward = esp_log_set_vprintf(capture);
}

std::size_t debug_log_count() {
    portENTER_CRITICAL(&lock);
    const std::size_t count = held;
    portEXIT_CRITICAL(&lock);
    return count;
}

unsigned long debug_log_total() {
    portENTER_CRITICAL(&lock);
    const unsigned long total = written;
    portEXIT_CRITICAL(&lock);
    return total;
}

void debug_log_copy(std::size_t index, char *out, std::size_t size) {
    if (!out || size == 0) return;
    out[0] = '\0';
    portENTER_CRITICAL(&lock);
    if (index < held) {
        // The oldest retained line sits one past the newest once it has wrapped.
        const std::size_t oldest = (held == kLogLines) ? next : 0;
        const std::size_t slot = (oldest + index) % kLogLines;
        std::strncpy(out, lines[slot], size - 1);
        out[size - 1] = '\0';
    }
    portEXIT_CRITICAL(&lock);
}
