#pragma once

#include "monitor_model.h"

// Starts the RLCD Monitor BLE peripheral. The host writes a JSON snapshot in
// ordered 20-byte chunks: version, message id, chunk index, chunk count, data.
void monitor_ble_init();
bool monitor_ble_connected();
bool monitor_ble_take_update(MonitorModel &model);
