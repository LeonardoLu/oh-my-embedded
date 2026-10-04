// SPDX-License-Identifier: MIT
#include "corallium.h"
#include "corallium_framing.h"
#include "corallium_json_guard.h"
#include "corallium_ble_session.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define LINE_LIMIT 2048
#define TAG "device_ble"
static const uint8_t service_uuid[16] = {0x10,0x90,0x8f,0x5e,0x6d,0x6b,0xd2,0xa8,0x55,0x4c,0x6d,0x7e,0x01,0x00,0x2a,0x7d};
static const uint8_t rx_uuid[16] = {0x10,0x90,0x8f,0x5e,0x6d,0x6b,0xd2,0xa8,0x55,0x4c,0x6d,0x7e,0x02,0x00,0x2a,0x7d};
static const uint8_t tx_uuid[16] = {0x10,0x90,0x8f,0x5e,0x6d,0x6b,0xd2,0xa8,0x55,0x4c,0x6d,0x7e,0x03,0x00,0x2a,0x7d};
static const uint16_t primary_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t characteristic_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t rx_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t tx_property = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t initial_value[1];
static uint8_t cccd_value[2];
enum { SERVICE, RX_DECL, RX, TX_DECL, TX, CCCD, ATTR_COUNT };
static uint16_t handles[ATTR_COUNT];
static esp_gatt_if_t gatts_if = ESP_GATT_IF_NONE;
static _Atomic uint16_t connection_id;
static corallium_ble_session_t session;
static _Atomic bool ready, advertising_pending;
static bool adv_ready, scan_ready, service_ready;
static bool switch_restored;
static QueueHandle_t requests;
static corallium_stream_t incoming;
static esp_bd_addr_t peer;
typedef struct { uint32_t generation; char line[LINE_LIMIT + 1]; } request_t;
static const esp_gatts_attr_db_t database[ATTR_COUNT] = {
    [SERVICE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_uuid, ESP_GATT_PERM_READ, 16, 16, (uint8_t *)service_uuid}},
    [RX_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_uuid, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&rx_property}},
    [RX] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)rx_uuid, ESP_GATT_PERM_WRITE, 512, 0, initial_value}},
    [TX_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_uuid, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&tx_property}},
    [TX] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)tx_uuid, ESP_GATT_PERM_READ, 512, 0, initial_value}},
    [CCCD] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, cccd_value}},
};
static esp_ble_adv_params_t advertising = {
    .adv_int_min = 0x320, .adv_int_max = 0x640,
    .adv_type = ADV_TYPE_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
static esp_err_t save_enabled(bool enabled) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("corallium", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(handle, "ble_enabled", enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
void corallium_restore_switch(void) {
    if (switch_restored) return;
    switch_restored = true;
    nvs_handle_t handle;
    uint8_t enabled = 0;
    esp_err_t err = nvs_open("corallium", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "ble_enabled", &enabled);
        nvs_close(handle);
    }
    session.enabled = err == ESP_OK && enabled == 1;
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "BLE preference unavailable: %s", esp_err_to_name(err));
}
static void advertise(void) {
    if (!ready || !session.enabled || session.connected || atomic_exchange(&advertising_pending, true)) return;
    esp_err_t err = esp_ble_gap_start_advertising(&advertising);
    if (err != ESP_OK) {
        advertising_pending = false;
        ESP_LOGW(TAG, "Advertising start rejected: %s", esp_err_to_name(err));
    }
}
bool corallium_pairing_active(void) { return session.enabled; }
bool corallium_connected(void) { return session.enabled && session.connected; }
esp_err_t corallium_close_pairing(void) {
    esp_err_t err = corallium_ble_set_enabled(&session, false, save_enabled);
    if (err != ESP_OK) return err;
    if (ready) (void)esp_ble_gap_stop_advertising();
    if (session.connected) (void)esp_ble_gap_disconnect(peer);
    return ESP_OK;
}
esp_err_t corallium_open_pairing(void) {
    esp_err_t err = corallium_ble_set_enabled(&session, true, save_enabled);
    if (err != ESP_OK) return err;
    advertise();
    return ESP_OK;
}
static void gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        adv_ready = param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS;
        ready = adv_ready && scan_ready && service_ready;
        advertise();
    } else if (event == ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT) {
        scan_ready = param->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS;
        ready = adv_ready && scan_ready && service_ready;
        advertise();
    } else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            advertising_pending = false;
            ESP_LOGW(TAG, "Advertising start failed: status=%d", param->adv_start_cmpl.status);
        } else if (!session.enabled || session.connected) {
            (void)esp_ble_gap_stop_advertising();
        }
    } else if (event == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT) {
        advertising_pending = false;
        advertise();
    } else if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        /* This device profile uses ordinary, unpaired GATT. Do not accept
         * an unsolicited pairing flow from a central. */
        (void)esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, false);
    }
}
static bool queue_line(const char *line, size_t length, void *context) {
    (void)context;
    static request_t message;
    memset(&message, 0, sizeof(message));
    message.generation = session.generation;
    memcpy(message.line, line, length);
    const bool accepted = xQueueSend(requests, &message, 0) == pdTRUE;
    memset(&message, 0, sizeof(message));
    return accepted;
}
static void receive(const uint8_t *bytes, size_t length) {
    if (!corallium_ble_can_exchange(&session)) return;
    if (!corallium_stream_feed(&incoming, bytes, length, esp_timer_get_time(), queue_line, NULL))
        (void)esp_ble_gap_disconnect(peer);
}
static void gatt_event(esp_gatts_cb_event_t event, esp_gatt_if_t iface, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status != ESP_GATT_OK) break;
        gatts_if = iface;
        (void)esp_ble_gap_set_device_name("ESP-Mosaico");
        /* Service UUID fits primary advertising; full name in scan response. */
        esp_ble_adv_data_t data = {.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
            .service_uuid_len = 16, .p_service_uuid = (uint8_t *)service_uuid};
        (void)esp_ble_gap_config_adv_data(&data);
        esp_ble_adv_data_t scan = {.set_scan_rsp = true, .include_name = true};
        (void)esp_ble_gap_config_adv_data(&scan);
        (void)esp_ble_gatts_create_attr_tab(database, iface, ATTR_COUNT, 0);
        break;
    }
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == ATTR_COUNT) {
            memcpy(handles, param->add_attr_tab.handles, sizeof(handles));
            (void)esp_ble_gatts_start_service(handles[SERVICE]);
        }
        break;
    case ESP_GATTS_START_EVT:
        service_ready = param->start.status == ESP_GATT_OK;
        ready = adv_ready && scan_ready && service_ready;
        advertise();
        break;
    case ESP_GATTS_CONNECT_EVT: {
        if (!corallium_ble_accept_connection(&session)) { (void)esp_ble_gap_disconnect(param->connect.remote_bda); break; }
        connection_id = param->connect.conn_id;
        memcpy(peer, param->connect.remote_bda, sizeof(peer));
        advertising_pending = false;
        corallium_stream_reset(&incoming);
        xQueueReset(requests);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        if (!session.connected || param->disconnect.conn_id != connection_id) break;
        ESP_LOGI(TAG, "Disconnected: reason=%u", param->disconnect.reason);
        corallium_ble_reset_connection(&session);
        advertising_pending = false;
        corallium_stream_reset(&incoming);
        xQueueReset(requests);
        advertise();
        break;
    case ESP_GATTS_WRITE_EVT:
        if (!session.connected || param->write.conn_id != connection_id || !session.enabled) break;
        if (param->write.is_prep) { (void)esp_ble_gap_disconnect(peer); break; }
        if (param->write.handle == handles[CCCD] && param->write.len == 2 && param->write.offset == 0) {
            session.notify_requested = param->write.value[0] == 1 && param->write.value[1] == 0;
            ESP_LOGI(TAG, "Notification subscription: enabled=%d", session.notify_requested);
        } else if (param->write.handle == handles[RX]) receive(param->write.value, param->write.len);
        break;
    default: break;
    }
}
static bool send_json(cJSON *message, uint32_t session_generation) {
    char *text = cJSON_PrintUnformatted(message);
    cJSON_Delete(message);
    if (!text) return false;
    size_t length = strlen(text), pos = 0;
    bool failed = false;
    if (length <= LINE_LIMIT) {
        /* Include LF in the final ATT packet without growing cJSON's buffer. */
        for (; pos <= length && corallium_ble_can_exchange(&session) && session_generation == session.generation; ) {
            uint8_t packet[20]; size_t count = 0;
            while (count < sizeof(packet) && pos <= length) {
                packet[count++] = pos == length ? '\n' : text[pos];
                ++pos;
            }
            esp_err_t err = esp_ble_gatts_send_indicate(gatts_if, connection_id, handles[TX], count, packet, false);
            if (err != ESP_OK) { failed = true; (void)esp_ble_gap_disconnect(peer); break; }
            vTaskDelay(pdMS_TO_TICKS(12));
        }
    }
    memset(text, 0, length); free(text);
    return !failed && pos == length + 1 && corallium_ble_can_exchange(&session) && session_generation == session.generation;
}
static void worker(void *arg) {
    (void)arg;
    request_t *message = calloc(1, sizeof(*message));
    if (!message) { vTaskDelete(NULL); return; }
    int64_t last_status = 0;
    while (true) {
        if (xQueueReceive(requests, message, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (message->generation == session.generation && corallium_ble_can_exchange(&session)) {
                const char *end = NULL;
                cJSON *request = corallium_json_safe(message->line) ? cJSON_ParseWithOpts(message->line, &end, true) : NULL;
                cJSON *response = corallium_dispatch(request);
                const cJSON *op = cJSON_GetObjectItemCaseSensitive(response, "op");
                bool handshake = cJSON_IsString(op) && !strcmp(op->valuestring, "device.info") &&
                                 cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(response, "ok"));
                bool sent = send_json(response, message->generation);
                if (handshake && sent && message->generation == session.generation) {
                    session.application_generation = message->generation;
                    last_status = esp_timer_get_time();
                }
                cJSON_Delete(request);
            }
            memset(message, 0, sizeof(*message));
        } else if (corallium_ble_can_publish(&session) && esp_timer_get_time() - last_status > 5000000) {
            cJSON *event = cJSON_CreateObject();
            cJSON_AddNumberToObject(event, "v", 1);
            cJSON_AddStringToObject(event, "op", "device.status");
            cJSON_AddItemToObject(event, "payload", corallium_status());
            send_json(event, session.generation);
            last_status = esp_timer_get_time();
        }
    }
}
esp_err_t corallium_start(void) {
    corallium_restore_switch();
    requests = xQueueCreate(1, sizeof(request_t));
    if (!requests) return ESP_ERR_NO_MEM;
    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&config);
    if (err != ESP_OK) return err;
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) return err;
    if ((err = esp_bluedroid_init()) != ESP_OK) return err;
    if ((err = esp_bluedroid_enable()) != ESP_OK) return err;
    if ((err = esp_ble_gap_register_callback(gap_event)) != ESP_OK) return err;
    if ((err = esp_ble_gatts_register_callback(gatt_event)) != ESP_OK) return err;
    if (xTaskCreate(worker, "mosaico_ble", 6144, NULL, 4, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return esp_ble_gatts_app_register(0xC0);
}
