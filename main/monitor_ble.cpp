#include "monitor_ble.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
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

std::atomic_bool connected{false};
std::atomic<uint16_t> conn_handle{0};
uint16_t button_handle = 0;
uint8_t button_sequence = 0;
uint8_t own_addr_type;
SemaphoreHandle_t model_mutex;
MonitorModel pending_model = monitor_demo_model();
bool update_pending = false;
char json_buffer[MAX_JSON + 1];
size_t json_length = 0;
uint8_t active_message = 0;
uint8_t expected_chunk = 0;
uint8_t chunk_count = 0;

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

bool parse_snapshot(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    MonitorModel next = monitor_demo_model();
    next.bluetooth_connected = true;
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
    cJSON *process_count = cJSON_GetObjectItemCaseSensitive(root, "process_count");
    if (cJSON_IsNumber(process_count)) next.process_count = std::clamp(process_count->valueint, 0, 65535);
    cJSON *weather_c = cJSON_GetObjectItemCaseSensitive(root, "weather_c");
    if (cJSON_IsNumber(weather_c)) next.weather_c = weather_c->valueint;
    parse_agent(root, "codex", next.codex);
    parse_agent(root, "claude", next.claude);

    cJSON *processes = cJSON_GetObjectItemCaseSensitive(root, "processes");
    if (cJSON_IsArray(processes)) {
        for (size_t i = 0; i < next.processes.size(); ++i) {
            cJSON *obj = cJSON_GetArrayItem(processes, i);
            if (!cJSON_IsObject(obj)) break;
            copy_string(obj, "name", next.processes[i].name);
            copy_float(obj, "cpu", next.processes[i].cpu_percent);
            cJSON *ram = cJSON_GetObjectItemCaseSensitive(obj, "ram_mb");
            if (cJSON_IsNumber(ram)) next.processes[i].ram_mb = std::clamp(ram->valueint, 0, 65535);
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

    xSemaphoreTake(model_mutex, portMAX_DELAY);
    pending_model = std::move(next);
    update_pending = true;
    xSemaphoreGive(model_mutex);
    return true;
}

int telemetry_access(uint16_t, uint16_t, ble_gatt_access_ctxt *ctxt, void *) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_READ_NOT_PERMITTED;
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
        active_message = message;
        expected_chunk = 0;
        chunk_count = total;
        json_length = 0;
    }
    if (message != active_message || index != expected_chunk || total != chunk_count) {
        json_length = 0;
        return BLE_ATT_ERR_UNLIKELY;
    }
    const size_t payload_len = copied - 4;
    if (json_length + payload_len > MAX_JSON) {
        json_length = 0;
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    memcpy(json_buffer + json_length, packet + 4, payload_len);
    json_length += payload_len;
    ++expected_chunk;

    if (expected_chunk == chunk_count) {
        json_buffer[json_length] = '\0';
        if (!parse_snapshot(json_buffer)) {
            ESP_LOGW(TAG, "Rejected malformed telemetry JSON");
            json_length = 0;
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(TAG, "Telemetry updated (%u bytes)", static_cast<unsigned>(json_length));
        json_length = 0;
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
        case BLE_GAP_EVENT_CONNECT:
            connected.store(event->connect.status == 0);
            if (event->connect.status != 0) advertise();
            if (event->connect.status == 0) {
                conn_handle.store(event->connect.conn_handle);
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
                ble_gap_update_params(event->connect.conn_handle, &params);
            }
            ESP_LOGI(TAG, "Host %s", connected.load() ? "connected" : "connection failed");
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            connected.store(false);
            conn_handle.store(0);
            ESP_LOGI(TAG, "Host disconnected; advertising again");
            advertise();
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            return 0;
        default:
            return 0;
    }
}

void advertise() {
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

bool monitor_ble_connected() { return connected.load(); }

void monitor_ble_notify_button(uint8_t event, uint8_t page) {
    if (!connected.load() || !button_handle) return;
    // Four bytes so the message always fits the minimum ATT MTU, unlike the
    // chunked telemetry going the other way.
    const uint8_t packet[] = {PROTOCOL_VERSION, ++button_sequence, event, page};
    os_mbuf *om = ble_hs_mbuf_from_flat(packet, sizeof(packet));
    if (!om) return;
    if (ble_gatts_notify_custom(conn_handle.load(), button_handle, om) != 0) {
        ESP_LOGW(TAG, "Button notify failed");
    }
}

int8_t monitor_ble_rssi() {
    int8_t rssi = 0;
    if (!connected.load() || ble_gap_conn_rssi(conn_handle.load(), &rssi) != 0) return 0;
    return rssi;
}

bool monitor_ble_take_update(MonitorModel &model) {
    if (!model_mutex || xSemaphoreTake(model_mutex, 0) != pdTRUE) return false;
    const bool available = update_pending;
    if (available) {
        model = pending_model;
        update_pending = false;
    }
    xSemaphoreGive(model_mutex);
    return available;
}
