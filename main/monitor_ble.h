#pragma once

#include "monitor_model.h"

// Starts the RLCD Monitor BLE peripheral. The host writes a JSON snapshot in
// ordered 20-byte chunks: version, message id, chunk index, chunk count, data.
void monitor_ble_init();
bool monitor_ble_connected();
int8_t monitor_ble_rssi();

// Button events pushed to the host: 1 KEY, 2 KEY held, 3 BOOT, 4 BOOT held.
// The page is the one on screen when the button was pressed, one-based.
void monitor_ble_notify_button(uint8_t event, uint8_t page);
bool monitor_ble_take_update(MonitorModel &model);
