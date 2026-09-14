#include "monitor_ble.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
constexpr char TAG[] = "monitor-ble";
constexpr size_t MAX_JSON = 4096;
constexpr uint8_t PROTOCOL_VERSION = 1;

// 7a1e0001-7e57-4d6d-8a1b-2c3d4e5f6071
const ble_uuid128_t SERVICE_UUID = BLE_UUID128_INIT(
    0x71, 0x60, 0x5f, 0x4e, 0x3d, 0x2c, 0x1b, 0x8a,
    0x6d, 0x4d, 0x57, 0x7e, 0x01, 0x00, 0x1e, 0x7a);
// 7a1e0002-7e57-4d6d-8a1b-2c3d4e5f6071
const ble_uuid128_t TELEMETRY_UUID = BLE_UUID128_INIT(
    0x71, 0x60, 0x5f, 0x4e, 0x3d, 0x2c, 0x1b, 0x8a,
    0x6d, 0x4d, 0x57, 0x7e, 0x02, 0x00, 0x1e, 0x7a);
// 7a1e0003-7e57-4d6d-8a1b-2c3d4e5f6071
const ble_uuid128_t BUTTON_UUID = BLE_UUID128_INIT(
    0x71, 0x60, 0x5f, 0x4e, 0x3d, 0x2c, 0x1b, 0x8a,
    0x6d, 0x4d, 0x57, 0x7e, 0x03, 0x00, 0x1e, 0x7a);

constexpr int SOURCES = static_cast<int>(kMaxSources);
constexpr uint16_t NO_CONNECTION = 0xffff;
constexpr uint8_t EVENT_SHOWN = 5;
constexpr uint8_t EVENT_HIDDEN = 6;

// One per host. A slot keeps its name and last snapshot after the host drops,
// so the screen can go on showing that source, marked stale. The reassembly
// fields are only touched from the NimBLE host task; the rest is shared with
// the UI task under model_mutex.
struct Slot {
    uint16_t conn_handle = NO_CONNECTION;
    char json[MAX_JSON + 1]{};
    size_t json_length = 0;
    uint8_t active_message = 0;
    uint8_t expected_chunk = 0;
    uint8_t chunk_count = 0;
    bool has_model = false;
    MonitorModel model{};
    std::string name;
    int64_t updated_at = 0;
    // Last shown or hidden state sent to this host; zero until it has been told.
    uint8_t announced = 0;
};

struct Announcement {
    uint16_t handle;
    uint8_t event;
};

uint16_t button_handle = 0;
std::atomic<uint8_t> button_sequence{0};
uint8_t own_addr_type;
SemaphoreHandle_t model_mutex;
Slot slots[kMaxSources];
std::string selected_name;
int shown_slot = -1;
bool update_pending = false;

int slot_for_connection(uint16_t handle) {
    for (int i = 0; i < SOURCES; ++i) {
        if (slots[i].conn_handle == handle) return i;
    }
    return -1;
}

int live_count() {
    int count = 0;
    for (const Slot &slot : slots) {
        if (slot.conn_handle != NO_CONNECTION) ++count;
    }
    return count;
}

// Mutex held. The saved choice wins whenever that source has data, even stale,
// so a dropped connection does not silently swap the screen to another host.
// Until the saved source appears, a live one stands in without replacing the
// choice, so whichever machine starts first does not overwrite it.
int resolve_shown() {
    if (!selected_name.empty()) {
        for (int i = 0; i < SOURCES; ++i) {
            if (slots[i].has_model && slots[i].name == selected_name) return i;
        }
    }
    for (int i = 0; i < SOURCES; ++i) {
        if (slots[i].has_model && slots[i].conn_handle != NO_CONNECTION) return i;
    }
    for (int i = 0; i < SOURCES; ++i) {
        if (slots[i].has_model) return i;
    }
    return -1;
}

// Mutex held. Hosts hear about the change later from monitor_ble_flush_events on
// the UI task. Sending a notification from inside a NimBLE callback runs the
// whole transmit path on the host task's small stack, which overflowed it.
void reselect_locked() {
    const int next = resolve_shown();
    if (next == shown_slot) return;
    shown_slot = next;
    update_pending = true;
}

void send_event(uint16_t handle, uint8_t event, uint8_t page) {
    if (handle == NO_CONNECTION || !button_handle) return;
    // Four bytes so the message always fits the minimum ATT MTU, unlike the
    // chunked telemetry going the other way.
    const uint8_t packet[] = {PROTOCOL_VERSION, ++button_sequence, event, page};
    os_mbuf *om = ble_hs_mbuf_from_flat(packet, sizeof(packet));
    if (!om) return;
    if (ble_gatts_notify_custom(handle, button_handle, om) != 0) ESP_LOGW(TAG, "Notify failed");
}

uint16_t shown_connection() {
    uint16_t handle = NO_CONNECTION;
    if (model_mutex && xSemaphoreTake(model_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (shown_slot >= 0) handle = slots[shown_slot].conn_handle;
        xSemaphoreGive(model_mutex);
    }
    return handle;
}

void save_selection(const std::string &name) {
    nvs_handle_t handle;
    if (nvs_open("rlcd", NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_str(handle, "source", name.c_str());
    nvs_commit(handle);
    nvs_close(handle);
}

std::string load_selection() {
    nvs_handle_t handle;
    if (nvs_open("rlcd", NVS_READONLY, &handle) != ESP_OK) return {};
    char value[32]{};
    size_t length = sizeof(value);
    std::string name;
    if (nvs_get_str(handle, "source", value, &length) == ESP_OK) name = value;
    nvs_close(handle);
    return name;
}

void copy_string(cJSON *root, const char *key, std::string &out) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring) out = item->valuestring;
}

void copy_int(cJSON *root, const char *key, uint8_t &out) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item)) out = static_cast<uint8_t>(std::clamp(item->valueint, 0, 100));
}

void copy_float(cJSON *root, const char *key, float &out) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item)) out = static_cast<float>(item->valuedouble);
}

void parse_agent(cJSON *root, const char *key, AgentUsage &agent) {
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsObject(obj)) return;
    cJSON *active = cJSON_GetObjectItemCaseSensitive(obj, "active");
    cJSON *tokens = cJSON_GetObjectItemCaseSensitive(obj, "tokens_k");
    cJSON *context = cJSON_GetObjectItemCaseSensitive(obj, "context");
    cJSON *sessions = cJSON_GetObjectItemCaseSensitive(obj, "sessions");
    cJSON *cost = cJSON_GetObjectItemCaseSensitive(obj, "cost");
    copy_string(obj, "five_hour", agent.five_hour_usage);
    copy_string(obj, "weekly", agent.weekly_usage);
    if (cJSON_IsBool(active)) agent.active = cJSON_IsTrue(active);
    if (cJSON_IsNumber(tokens)) agent.tokens_k = std::max(0, tokens->valueint);
    if (cJSON_IsNumber(context)) agent.context_percent = std::clamp(context->valueint, 0, 100);
    if (cJSON_IsNumber(sessions)) agent.sessions = std::clamp(sessions->valueint, 0, 255);
    if (cJSON_IsNumber(cost)) agent.cost_usd = static_cast<float>(cost->valuedouble);
}

bool parse_snapshot(const char *json, int slot) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    // A MonitorModel is over a kilobyte and this runs on the NimBLE host task, so
    // parse into reused storage instead of a local on its small stack.
    static const MonitorModel defaults = monitor_demo_model();
    static MonitorModel next;
    next = defaults;
    next.bluetooth_connected = true;
    copy_string(root, "source", next.source_name);
    copy_string(root, "time", next.time_text);
    copy_string(root, "date", next.date_text);
    copy_string(root, "weather", next.weather_text);
    copy_string(root, "region", next.region_text);
    copy_int(root, "battery", next.board_battery_percent);
    copy_int(root, "cpu", next.cpu_percent);
    copy_int(root, "ram", next.ram_percent);
    copy_int(root, "gpu", next.gpu_percent);
    copy_int(root, "swap", next.swap_percent);
    copy_float(root, "ram_used_gb", next.ram_used_gb);
    copy_float(root, "ram_total_gb", next.ram_total_gb);
    copy_float(root, "swap_used_gb", next.swap_used_gb);
    copy_float(root, "temp", next.cpu_temp_c);
    copy_float(root, "power", next.system_power_w);
    copy_float(root, "net_down", next.net_down_kbs);
    copy_float(root, "net_up", next.net_up_kbs);
    copy_float(root, "noise_offset", next.noise_offset_db);
    cJSON *process_count = cJSON_GetObjectItemCaseSensitive(root, "process_count");
    if (cJSON_IsNumber(process_count)) next.process_count = std::clamp(process_count->valueint, 0, 65535);
    cJSON *weather_c = cJSON_GetObjectItemCaseSensitive(root, "weather_c");
    if (cJSON_IsNumber(weather_c)) next.weather_c = weather_c->valueint;
    parse_agent(root, "codex", next.codex);
    parse_agent(root, "claude", next.claude);

    cJSON *processes = cJSON_GetObjectItemCaseSensitive(root, "processes");
    if (cJSON_IsArray(processes)) {
        next.process_rows = 0;
        for (size_t i = 0; i < next.processes.size(); ++i) {
            cJSON *obj = cJSON_GetArrayItem(processes, i);
            if (!cJSON_IsObject(obj)) break;
            copy_string(obj, "name", next.processes[i].name);
            copy_float(obj, "cpu", next.processes[i].cpu_percent);
            cJSON *ram = cJSON_GetObjectItemCaseSensitive(obj, "ram_mb");
            if (cJSON_IsNumber(ram)) next.processes[i].ram_mb = std::clamp(ram->valueint, 0, 65535);
            // An older host sends no impact, so it falls back to processor time
            // and the impact ordering simply matches the processor one.
            next.processes[i].impact = next.processes[i].cpu_percent;
            copy_float(obj, "impact", next.processes[i].impact);
            next.process_rows = i + 1;
        }
    }

    cJSON *headlines = cJSON_GetObjectItemCaseSensitive(root, "headlines");
    if (cJSON_IsArray(headlines)) {
        for (size_t i = 0; i < next.headlines.size(); ++i) {
            cJSON *item = cJSON_GetArrayItem(headlines, i);
            if (cJSON_IsString(item) && item->valuestring) next.headlines[i] = item->valuestring;
        }
    }
    cJSON *headline_ages = cJSON_GetObjectItemCaseSensitive(root, "headline_ages");
    if (cJSON_IsArray(headline_ages)) {
        for (size_t i = 0; i < next.headline_ages.size(); ++i) {
            cJSON *item = cJSON_GetArrayItem(headline_ages, i);
            if (cJSON_IsString(item) && item->valuestring) next.headline_ages[i] = item->valuestring;
        }
    }
    cJSON_Delete(root);
    if (next.source_name.empty()) next.source_name = "HOST " + std::to_string(slot + 1);

    xSemaphoreTake(model_mutex, portMAX_DELAY);
    Slot &target = slots[slot];
    // A host that reconnects lands in a fresh slot, so retire its stale copy.
    for (int i = 0; i < SOURCES; ++i) {
        if (i == slot || slots[i].conn_handle != NO_CONNECTION) continue;
        if (slots[i].has_model && slots[i].name == next.source_name) {
            slots[i].has_model = false;
            slots[i].name.clear();
        }
    }
    target.name = next.source_name;
    target.model = std::move(next);
    target.has_model = true;
    target.updated_at = esp_timer_get_time();
    reselect_locked();
    if (slot == shown_slot) update_pending = true;
    xSemaphoreGive(model_mutex);
    return true;
}

int telemetry_access(uint16_t conn_handle, uint16_t, ble_gatt_access_ctxt *ctxt, void *) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
    // Each host reassembles in its own slot, so two machines sending at once
    // cannot interleave their chunks.
    const int slot_index = slot_for_connection(conn_handle);
    if (slot_index < 0) return BLE_ATT_ERR_UNLIKELY;
    Slot &s = slots[slot_index];
    const uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 5 || len > 244) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t packet[244];
    uint16_t copied = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, packet, sizeof(packet), &copied) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const uint8_t version = packet[0];
    const uint8_t message = packet[1];
    const uint8_t index = packet[2];
    const uint8_t total = packet[3];
    if (version != PROTOCOL_VERSION || total == 0 || index >= total) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (index == 0) {
        s.active_message = message;
        s.expected_chunk = 0;
        s.chunk_count = total;
        s.json_length = 0;
    }
    if (message != s.active_message || index != s.expected_chunk || total != s.chunk_count) {
        s.json_length = 0;
        return BLE_ATT_ERR_UNLIKELY;
    }
    const size_t payload_len = copied - 4;
    if (s.json_length + payload_len > MAX_JSON) {
        s.json_length = 0;
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    memcpy(s.json + s.json_length, packet + 4, payload_len);
    s.json_length += payload_len;
    ++s.expected_chunk;

    if (s.expected_chunk == s.chunk_count) {
        s.json[s.json_length] = '\0';
        if (!parse_snapshot(s.json, slot_index)) {
            ESP_LOGW(TAG, "Rejected malformed telemetry JSON");
            s.json_length = 0;
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(TAG, "Telemetry updated (%u bytes)", static_cast<unsigned>(s.json_length));
        s.json_length = 0;
    }
    return 0;
}

int button_access(uint16_t, uint16_t, ble_gatt_access_ctxt *, void *) {
    // Buttons are only ever pushed as notifications; nothing to read back.
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

const ble_gatt_svc_def services[] = {{
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &SERVICE_UUID.u,
    .characteristics = (ble_gatt_chr_def[]){{
        .uuid = &TELEMETRY_UUID.u,
        .access_cb = telemetry_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    }, {
        .uuid = &BUTTON_UUID.u,
        .access_cb = button_access,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &button_handle,
    }, {0}},
}, {0}};

void advertise();

int gap_event(ble_gap_event *event, void *) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT: {
            if (event->connect.status != 0) {
                ESP_LOGI(TAG, "Host connection failed");
                advertise();
                return 0;
            }
            const uint16_t handle = event->connect.conn_handle;
            int index = -1;
            xSemaphoreTake(model_mutex, portMAX_DELAY);
            // Prefer an unused slot; otherwise evict the stale source that has
            // been gone the longest.
            for (int i = 0; i < SOURCES && index < 0; ++i) {
                if (slots[i].conn_handle == NO_CONNECTION && !slots[i].has_model) index = i;
            }
            if (index < 0) {
                for (int i = 0; i < SOURCES; ++i) {
                    if (slots[i].conn_handle != NO_CONNECTION) continue;
                    if (index < 0 || slots[i].updated_at < slots[index].updated_at) index = i;
                }
            }
            if (index >= 0) {
                Slot &slot = slots[index];
                slot.conn_handle = handle;
                slot.json_length = 0;
                slot.expected_chunk = 0;
                slot.chunk_count = 0;
                slot.has_model = false;
                slot.name.clear();
                slot.announced = 0;
                reselect_locked();
            }
            xSemaphoreGive(model_mutex);
            if (index < 0) {
                ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
                return 0;
            }
            // Apple requires a minimum interval that is a multiple of 15 ms, a
            // maximum at least 15 ms above it, and interval_max * (latency + 1)
            // under 2 seconds. macOS rejects the update outright otherwise.
            ble_gap_upd_params params{};
            params.itvl_min = 72;  // 90 ms
            params.itvl_max = 96;  // 120 ms
            params.latency = 4;
            params.supervision_timeout = 600;  // 6 seconds
            params.min_ce_len = 0;
            params.max_ce_len = 0;
            ble_gap_update_params(handle, &params);
            ESP_LOGI(TAG, "Host connected in slot %d", index);
            // Keep advertising so a second machine can still find the board.
            if (live_count() < SOURCES) advertise();
            return 0;
        }
        case BLE_GAP_EVENT_DISCONNECT: {
            xSemaphoreTake(model_mutex, portMAX_DELAY);
            const int index = slot_for_connection(event->disconnect.conn.conn_handle);
            if (index >= 0) {
                slots[index].conn_handle = NO_CONNECTION;
                slots[index].json_length = 0;
                // The shown source keeps its last snapshot on screen, marked stale.
                if (index == shown_slot) update_pending = true;
                reselect_locked();
            }
            xSemaphoreGive(model_mutex);
            ESP_LOGI(TAG, "Host in slot %d disconnected", index);
            advertise();
            return 0;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
            if (live_count() < SOURCES) advertise();
            return 0;
        default:
            return 0;
    }
}

void advertise() {
    if (ble_gap_adv_active()) return;
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name = ble_svc_gap_device_name();
    fields.name = reinterpret_cast<const uint8_t *>(name);
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising data failed: %d", rc);
        return;
    }
    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &params, gap_event, nullptr);
    if (rc != 0) ESP_LOGE(TAG, "Advertising start failed: %d", rc);
}

void on_sync() {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc == 0) advertise();
    else ESP_LOGE(TAG, "BLE address setup failed: %d", rc);
}

void host_task(void *) {
    ESP_LOGI(TAG, "Advertising as RLCD Monitor");
    nimble_port_run();
    nimble_port_freertos_deinit();
}
}  // namespace

void monitor_ble_init() {
    model_mutex = xSemaphoreCreateMutex();
    configASSERT(model_mutex);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    selected_name = load_selection();
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set("RLCD Monitor"));
    int rc = ble_gatts_count_cfg(services);
    configASSERT(rc == 0);
    rc = ble_gatts_add_svcs(services);
    configASSERT(rc == 0);
    nimble_port_freertos_init(host_task);
}

bool monitor_ble_connected() { return shown_connection() != NO_CONNECTION; }

int8_t monitor_ble_rssi() {
    const uint16_t handle = shown_connection();
    int8_t rssi = 0;
    if (handle == NO_CONNECTION || ble_gap_conn_rssi(handle, &rssi) != 0) return 0;
    return rssi;
}

void monitor_ble_notify_button(uint8_t event, uint8_t page) {
    // Only the shown host hears buttons, or a mapped command would run on every
    // connected machine at once.
    send_event(shown_connection(), event, page);
}

bool monitor_ble_show_next_source() {
    std::string chosen;
    if (!model_mutex || xSemaphoreTake(model_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    for (int step = 1; step <= SOURCES; ++step) {
        const int i = (std::max(shown_slot, 0) + step) % SOURCES;
        if (i != shown_slot && slots[i].has_model) {
            chosen = slots[i].name;
            break;
        }
    }
    if (!chosen.empty()) {
        selected_name = chosen;
        reselect_locked();
    }
    xSemaphoreGive(model_mutex);
    if (!chosen.empty()) save_selection(chosen);
    return !chosen.empty();
}

std::array<SourceStatus, kMaxSources> monitor_ble_sources() {
    std::array<SourceStatus, kMaxSources> list{};
    uint16_t handles[kMaxSources];
    if (!model_mutex || xSemaphoreTake(model_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return list;
    const int64_t now = esp_timer_get_time();
    for (int i = 0; i < SOURCES; ++i) {
        const Slot &slot = slots[i];
        list[i].connected = slot.conn_handle != NO_CONNECTION;
        list[i].shown = i == shown_slot;
        list[i].has_data = slot.has_model;
        list[i].name = slot.name;
        list[i].age_seconds = slot.has_model ? static_cast<uint32_t>((now - slot.updated_at) / 1000000) : 0;
        handles[i] = slot.conn_handle;
    }
    xSemaphoreGive(model_mutex);
    for (int i = 0; i < SOURCES; ++i) {
        int8_t rssi = 0;
        if (handles[i] != NO_CONNECTION && ble_gap_conn_rssi(handles[i], &rssi) == 0) list[i].rssi = rssi;
    }
    return list;
}

bool monitor_ble_take_update(MonitorModel &model) {
    if (!model_mutex || xSemaphoreTake(model_mutex, 0) != pdTRUE) return false;
    const bool available = update_pending && shown_slot >= 0 && slots[shown_slot].has_model;
    if (available) model = slots[shown_slot].model;
    update_pending = false;
    xSemaphoreGive(model_mutex);
    return available;
}

void monitor_ble_flush_events() {
    Announcement due[kMaxSources];
    int count = 0;
    if (!model_mutex || xSemaphoreTake(model_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
    for (int i = 0; i < SOURCES; ++i) {
        Slot &slot = slots[i];
        // A host is told once it has sent a snapshot; until then it assumes it is
        // shown and sends at full rate, so its first snapshot is never delayed.
        if (slot.conn_handle == NO_CONNECTION || !slot.has_model) continue;
        const uint8_t state = i == shown_slot ? EVENT_SHOWN : EVENT_HIDDEN;
        if (slot.announced == state) continue;
        slot.announced = state;
        due[count++] = {slot.conn_handle, state};
    }
    xSemaphoreGive(model_mutex);
    for (int i = 0; i < count; ++i) send_event(due[i].handle, due[i].event, 0);
}
