#pragma once

#include "monitor_model.h"

// Starts the RLCD Monitor BLE peripheral. Up to kMaxSources hosts stay connected
// at once, each writing a JSON snapshot in ordered chunks: version, message id,
// chunk index, chunk count, data. The board shows one of them at a time.
void monitor_ble_init();

// Connection state and signal of the source currently shown.
bool monitor_ble_connected();
int8_t monitor_ble_rssi();

// Copies the shown source's snapshot when it changed since the last call,
// including when a different source became the shown one.
bool monitor_ble_take_update(MonitorModel &model);

// Shows the next source that has data and remembers the choice across reboots.
// Returns false when there is no other source to switch to.
bool monitor_ble_show_next_source();

// Every slot, for the devices page and the header tag.
std::array<SourceStatus, kMaxSources> monitor_ble_sources();

// Button events go to the shown host only: 1 KEY, 2 KEY held, 3 BOOT, 4 BOOT
// held. The page is the one on screen when the button was pressed, one-based.
// The board also sends 5 when a host becomes the shown one and 6 when hidden.
void monitor_ble_notify_button(uint8_t event, uint8_t page);

// Sends any pending shown or hidden notices. Call from the UI task, never from a
// NimBLE callback.
void monitor_ble_flush_events();
