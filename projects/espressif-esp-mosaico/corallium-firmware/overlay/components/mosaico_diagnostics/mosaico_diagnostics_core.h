/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "mosaico_diagnostics.h"
#include "mosaico_diagnostics_tasks.h"

#define MOSAICO_DIAGNOSTICS_PACKET_BYTES 512u
#define MOSAICO_DIAGNOSTICS_TX_DEPTH 4u
#define MOSAICO_DIAGNOSTICS_COMMAND_INTERVAL_MS 100u
#define MOSAICO_DIAGNOSTICS_CAPTURE_LIMIT_MS 30000u
#define MOSAICO_DIAGNOSTICS_TX_STALL_MS 2000u
#define MOSAICO_DIAGNOSTICS_TASK_INTERVAL_MS 1000u

typedef struct {
    uint8_t bytes[MOSAICO_DIAGNOSTICS_PACKET_BYTES];
    size_t len;
    uint32_t serial;
} mosaico_diagnostics_packet_t;

typedef struct {
    mosaico_diagnostics_ops_t ops;
    uint8_t mac[6];
    char line[MOSAICO_DIAGNOSTICS_MAX_LINE + 1u];
    size_t line_len;
    bool line_invalid;
    mosaico_diagnostics_packet_t tx[MOSAICO_DIAGNOSTICS_TX_DEPTH];
    unsigned tx_head;
    unsigned tx_count;
    uint32_t packet_serial;
    bool command_seen;
    uint64_t last_command_ms;
    bool capture_active;
    bool capture_timer_armed;
    void *capture_handle;
    mosaico_diagnostics_frame_t capture_frame;
    uint32_t capture_id;
    uint32_t capture_offset;
    uint32_t capture_crc;
    uint64_t capture_started_ms;
    uint64_t last_tx_progress_ms;
    esp_err_t (*tasks_snapshot)(mosaico_diagnostics_tasks_t *out);
    mosaico_diagnostics_tasks_t tasks;
    bool tasks_seen;
    bool tasks_active;
    uint32_t tasks_id;
    unsigned tasks_index;
    uint64_t tasks_started_ms;
    uint64_t last_tasks_ms;
} mosaico_diagnostics_core_t;

bool mosaico_diagnostics_core_init(mosaico_diagnostics_core_t *core,
                                  const mosaico_diagnostics_ops_t *ops,
                                  const uint8_t mac[6]);
/* Returns consumed bytes, stopping after a complete line or for TX capacity. */
size_t mosaico_diagnostics_core_feed(mosaico_diagnostics_core_t *core,
                                    const uint8_t *data, size_t len, uint64_t now_ms);
void mosaico_diagnostics_core_tick(mosaico_diagnostics_core_t *core, uint64_t now_ms);
const mosaico_diagnostics_packet_t *mosaico_diagnostics_core_packet(
    const mosaico_diagnostics_core_t *core);
void mosaico_diagnostics_core_sent(mosaico_diagnostics_core_t *core, uint64_t now_ms);
/* Also releases a live snapshot. Disconnected commands/responses are discarded. */
void mosaico_diagnostics_core_disconnect(mosaico_diagnostics_core_t *core);
uint32_t mosaico_diagnostics_crc32_update(uint32_t state, const uint8_t *data, size_t len);
