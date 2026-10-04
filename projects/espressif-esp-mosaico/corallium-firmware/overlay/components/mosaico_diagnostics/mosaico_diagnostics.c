/* SPDX-License-Identifier: Apache-2.0 */
#include "sdkconfig.h"
#include "mosaico_diagnostics.h"

#if CONFIG_MOSAICO_USB_DIAGNOSTICS

#include <unistd.h>

#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mosaico_diagnostics_core.h"
#include "tinyusb_cdc_acm.h"
#include "tusb.h"
#include "usb_console.h"

static mosaico_diagnostics_core_t s_core;
static TaskHandle_t s_task;

/* Keep raw CDC writes independent of libc FILE locking and its global mutex. */
static size_t send_packet(const mosaico_diagnostics_packet_t *packet, size_t offset)
{
    if (offset >= packet->len) {
        return 0;
    }
    size_t remaining = packet->len - offset;
    size_t queued = 0;
    /* Avoid deliberately fragmenting a line amongst ordinary console logs. */
    if (tud_cdc_n_write_available(TINYUSB_CDC_ACM_0) >= remaining) {
        queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                           packet->bytes + offset, remaining);
    }
    (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
    return queued;
}

static void diagnostics_task(void *arg)
{
    (void)arg;
    uint8_t input[64];
    size_t input_size = 0;
    size_t input_offset = 0;
    size_t output_offset = 0;
    uint32_t output_serial = 0;
    uint64_t output_started_ms = 0;
    for (;;) {
        uint64_t now_ms = (uint64_t)esp_timer_get_time() / 1000u;
        if (!tud_cdc_n_connected(TINYUSB_CDC_ACM_0)) {
            mosaico_diagnostics_core_disconnect(&s_core);
            input_size = input_offset = output_offset = 0;
            output_serial = 0;
            /* Do not replay a previous host's partial command on reconnect. */
            tud_cdc_n_read_flush(TINYUSB_CDC_ACM_0);
        } else {
            mosaico_diagnostics_core_tick(&s_core, now_ms);
            const mosaico_diagnostics_packet_t *packet = mosaico_diagnostics_core_packet(&s_core);
            if (packet) {
                if (packet->serial != output_serial) {
                    output_offset = 0;
                    output_serial = packet->serial;
                    output_started_ms = now_ms;
                }
                size_t queued = send_packet(packet, output_offset);
                output_offset += queued;
                if (output_offset == packet->len) {
                    mosaico_diagnostics_core_sent(&s_core, now_ms);
                    output_serial = 0;
                    output_offset = 0;
                } else if (now_ms < output_started_ms ||
                           now_ms - output_started_ms >= MOSAICO_DIAGNOSTICS_TX_STALL_MS) {
                    /* No indefinite response queue or retained capture on a stalled host. */
                    mosaico_diagnostics_core_disconnect(&s_core);
                    input_size = input_offset = output_offset = 0;
                    output_serial = 0;
                    tud_cdc_n_read_flush(TINYUSB_CDC_ACM_0);
                }
            }
            if (!mosaico_diagnostics_core_packet(&s_core)) {
                if (input_offset == input_size) {
                    ssize_t received = read(STDIN_FILENO, input, sizeof(input));
                    input_size = received > 0 ? (size_t)received : 0u;
                    input_offset = 0;
                }
                if (input_size > input_offset) {
                    input_offset += mosaico_diagnostics_core_feed(
                        &s_core, input + input_offset, input_size - input_offset, now_ms);
                }
            }
        }
        TickType_t delay = pdMS_TO_TICKS(s_core.capture_active || s_core.tasks_active || s_core.tx_count ? 2u : 20u);
        vTaskDelay(delay ? delay : 1u);
    }
}

esp_err_t mosaico_diagnostics_start(const mosaico_diagnostics_ops_t *ops)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_usb_console_is_initialized() ||
        !tinyusb_cdcacm_initialized(TINYUSB_CDC_ACM_0)) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t mac[6];
    /* S31's default getter emits EUI-64; this typed factory address is MAC-48. */
    esp_err_t err = esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
    if (err != ESP_OK) {
        return err;
    }
    if (!mosaico_diagnostics_core_init(&s_core, ops, mac)) {
        return ESP_ERR_INVALID_ARG;
    }
    s_core.tasks_snapshot = mosaico_diagnostics_tasks_snapshot;
    if (xTaskCreate(diagnostics_task, "usb_diagnostics", 6144u, NULL, 2u, &s_task) != pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

#else

esp_err_t mosaico_diagnostics_start(const mosaico_diagnostics_ops_t *ops)
{
    (void)ops;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
