/* SPDX-License-Identifier: Apache-2.0 */
#include "mosaico_diagnostics_core.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    COMMAND_PING,
    COMMAND_STATUS,
    COMMAND_TASKS,
    COMMAND_OPEN,
    COMMAND_TAP,
    COMMAND_DRAG,
    COMMAND_BACK,
    COMMAND_CAPTURE,
    COMMAND_ABORT,
} command_kind_t;

typedef struct {
    uint32_t id;
    command_kind_t kind;
    uint32_t args[5];
} command_t;

static bool queue_line(mosaico_diagnostics_core_t *core, uint32_t id,
                       const char *format, ...)
{
    if (core->tx_count == MOSAICO_DIAGNOSTICS_TX_DEPTH) {
        return false;
    }
    mosaico_diagnostics_packet_t *packet =
        &core->tx[(core->tx_head + core->tx_count) % MOSAICO_DIAGNOSTICS_TX_DEPTH];
    int prefix = snprintf((char *)packet->bytes, sizeof(packet->bytes),
                          "\r\n@MOSAICO %" PRIu32 " ", id);
    if (prefix < 0 || (size_t)prefix >= sizeof(packet->bytes) - 2u) {
        return false;
    }
    va_list args;
    va_start(args, format);
    int body = vsnprintf((char *)packet->bytes + prefix,
                        sizeof(packet->bytes) - (size_t)prefix - 2u, format, args);
    va_end(args);
    if (body < 0 || (size_t)body >= sizeof(packet->bytes) - (size_t)prefix - 2u) {
        return false;
    }
    packet->len = (size_t)prefix + (size_t)body;
    packet->bytes[packet->len++] = '\r';
    packet->bytes[packet->len++] = '\n';
    if (++core->packet_serial == 0u) ++core->packet_serial;
    packet->serial = core->packet_serial;
    ++core->tx_count;
    return true;
}

static void queue_error(mosaico_diagnostics_core_t *core, uint32_t id,
                        const char *code, esp_err_t error)
{
    if (error == ESP_OK) {
        (void)queue_line(core, id, "ERR %s", code);
    } else {
        (void)queue_line(core, id, "ERR %s 0x%" PRIx32, code, (uint32_t)error);
    }
}

static bool decimal(const char *word, uint32_t *value)
{
    uint32_t result = 0;
    if (!word || !*word) {
        return false;
    }
    for (; *word; ++word) {
        if (*word < '0' || *word > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(*word - '0');
        if (result > (UINT32_MAX - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

static const char *parse_line(char *line, command_t *out)
{
    char *words[8];
    unsigned count = 0;
    char *cursor = line + strlen("@MOSAICO ");
    while (*cursor) {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (!*cursor) {
            break;
        }
        if (count == sizeof(words) / sizeof(words[0])) {
            return "argument";
        }
        words[count++] = cursor;
        while (*cursor && *cursor != ' ') {
            ++cursor;
        }
        if (*cursor) {
            *cursor++ = '\0';
        }
    }
    if (!count || !decimal(words[0], &out->id) || out->id == 0u) {
        out->id = 0;
        return "syntax";
    }
    if (count < 2u) {
        return "syntax";
    }
    unsigned args = 0;
    if (strcmp(words[1], "ping") == 0) {
        out->kind = COMMAND_PING;
    } else if (strcmp(words[1], "status") == 0) {
        out->kind = COMMAND_STATUS;
    } else if (strcmp(words[1], "tasks") == 0) {
        out->kind = COMMAND_TASKS;
    } else if (strcmp(words[1], "open") == 0) {
        out->kind = COMMAND_OPEN;
        if (count != 3u) return "argument";
        if (strcmp(words[2], "settings") == 0) out->args[0] = MOSAICO_DIAGNOSTICS_APP_SETTINGS;
        else if (strcmp(words[2], "works") == 0) out->args[0] = MOSAICO_DIAGNOSTICS_APP_WORKS;
        else if (strcmp(words[2], "album") == 0) out->args[0] = MOSAICO_DIAGNOSTICS_APP_ALBUM;
        else if (strcmp(words[2], "weather") == 0) out->args[0] = MOSAICO_DIAGNOSTICS_APP_WEATHER;
        else return "argument";
        return NULL;
    } else if (strcmp(words[1], "tap") == 0) {
        out->kind = COMMAND_TAP;
        args = 2;
    } else if (strcmp(words[1], "drag") == 0) {
        out->kind = COMMAND_DRAG;
        args = 5;
    } else if (strcmp(words[1], "back") == 0) {
        out->kind = COMMAND_BACK;
    } else if (strcmp(words[1], "capture") == 0) {
        out->kind = COMMAND_CAPTURE;
    } else if (strcmp(words[1], "abort") == 0) {
        out->kind = COMMAND_ABORT;
    } else {
        return "command";
    }
    if (count != args + 2u) {
        return "argument";
    }
    for (unsigned i = 0; i < args; ++i) {
        if (!decimal(words[i + 2u], &out->args[i])) {
            return "argument";
        }
    }
    if (args >= 2u && (out->args[0] >= 480u || out->args[1] >= 480u)) {
        return "argument";
    }
    if (args == 5u && (out->args[2] >= 480u || out->args[3] >= 480u ||
                       out->args[4] < 50u || out->args[4] > 2000u)) {
        return "argument";
    }
    return NULL;
}

static const char *owner_name(mosaico_diagnostics_owner_t owner)
{
    switch (owner) {
    case MOSAICO_DIAGNOSTICS_OWNER_GSP: return "gsp";
    case MOSAICO_DIAGNOSTICS_OWNER_EXCLUSIVE: return "exclusive";
    default: return "none";
    }
}

static void public_app_name(char out[32], const char in[32])
{
    size_t len = 0;
    while (len < 31u && in[len]) {
        unsigned char c = (unsigned char)in[len];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-' || c == '.')) {
            memcpy(out, "unknown", sizeof("unknown"));
            return;
        }
        out[len] = (char)c;
        ++len;
    }
    out[len] = '\0';
    if (len == 0u) {
        memcpy(out, "none", sizeof("none"));
    }
}

static void release_capture(mosaico_diagnostics_core_t *core)
{
    if (core->capture_active) {
        core->ops.capture_end(core->ops.ctx, core->capture_handle);
    }
    core->capture_active = false;
    core->capture_handle = NULL;
}

static void status_reply(mosaico_diagnostics_core_t *core, uint32_t id, uint64_t now_ms)
{
    mosaico_diagnostics_status_t status = {0};
    esp_err_t err = core->ops.status(core->ops.ctx, &status);
    if (err != ESP_OK) {
        queue_error(core, id, "status", err);
        return;
    }
    char app_name[32];
    public_app_name(app_name, status.app_name);
    bool ok = queue_line(core, id,
        "STATUS {\"device\":{\"chip\":\"esp32s31\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"},"
        "\"uptime_ms\":%" PRIu64 "}",
        core->mac[0], core->mac[1], core->mac[2], core->mac[3], core->mac[4], core->mac[5], now_ms);
    ok = ok && queue_line(core, id,
        "STATUS {\"ui\":{\"app_name\":\"%s\",\"app_id\":%" PRIu32 ",\"owner\":\"%s\","
        "\"width\":%u,\"height\":%u,\"rotation\":%u,\"started\":%u,\"asleep\":%u,"
        "\"dimmed\":%u,\"panel\":%u,\"hub\":%u,\"runtime\":%u,\"quiesced\":%u,"
        "\"paused\":%u,\"drawer\":%u,\"gesture\":%u,\"queue\":%u,"
        "\"capture_supported\":%u}}",
        app_name, status.app_id, owner_name(status.display_owner),
        (unsigned)status.width, (unsigned)status.height, (unsigned)status.rotation,
        (unsigned)status.started, (unsigned)status.asleep, (unsigned)status.dimmed,
        (unsigned)status.panel_enabled, (unsigned)status.hub_presenter_active,
        (unsigned)status.runtime_started, (unsigned)status.quiesced,
        (unsigned)status.screen_paused, (unsigned)status.drawer_open,
        (unsigned)status.gesture_active, (unsigned)status.queue_depth,
        (unsigned)status.capture_supported);
    ok = ok && queue_line(core, id,
        "STATUS {\"render\":{\"frames\":%" PRIu64 ",\"busy_us\":%" PRIu64 ","
        "\"errors\":%" PRIu32 ",\"last_error\":%" PRId32 ","
        "\"runtime_errors\":%" PRIu32 ",\"last_runtime_error\":%" PRId32 ","
        "\"last_input_error\":%" PRId32 "}}",
        status.render_frames, status.render_busy_us, status.render_errors,
        status.last_render_error, status.runtime_errors, status.last_runtime_error,
        status.last_input_error);
    ok = ok && queue_line(core, id,
        "OK {\"controls\":{\"volume\":%u,\"brightness\":%u,\"panel_brightness_percent\":%u,"
        "\"dim_ms\":%" PRIu32 ","
        "\"sleep_ms\":%" PRIu32 ",\"poweroff_ms\":%" PRIu32 ",\"dim_charge\":%u,"
        "\"sleep_charge\":%u,\"wifi_enabled\":%u,\"wifi_connected\":%u,"
        "\"bluetooth_enabled\":%u},\"battery\":{\"available\":%u,\"charging\":%u}}",
        (unsigned)status.volume_percent, (unsigned)status.brightness_percent,
        (unsigned)status.panel_brightness_percent,
        status.dim_timeout_ms, status.sleep_timeout_ms, status.poweroff_timeout_ms,
        (unsigned)status.dim_while_charging, (unsigned)status.sleep_while_charging,
        (unsigned)status.wifi_enabled, (unsigned)status.wifi_connected,
        (unsigned)status.bluetooth_enabled, (unsigned)status.battery_available,
        (unsigned)status.charging);
    if (!ok) {
        queue_error(core, id, "overflow", ESP_OK);
    }
}

static void begin_capture(mosaico_diagnostics_core_t *core, uint32_t id, uint64_t now_ms)
{
    if (core->capture_active) {
        queue_error(core, id, "busy", ESP_OK);
        return;
    }
    if (!core->ops.capture_begin) {
        queue_error(core, id, "unsupported", ESP_ERR_NOT_SUPPORTED);
        return;
    }
    mosaico_diagnostics_status_t status = {0};
    esp_err_t err = core->ops.status(core->ops.ctx, &status);
    if (err != ESP_OK) {
        queue_error(core, id, "status", err);
        return;
    }
    if (!status.started || status.asleep || !status.panel_enabled) {
        queue_error(core, id, "asleep", ESP_OK);
        return;
    }
    if (status.display_owner != MOSAICO_DIAGNOSTICS_OWNER_GSP) {
        queue_error(core, id, "owner", ESP_OK);
        return;
    }
    if (!status.capture_supported) {
        queue_error(core, id, "unsupported", ESP_ERR_NOT_SUPPORTED);
        return;
    }
    mosaico_diagnostics_frame_t frame = {0};
    void *handle = NULL;
    err = core->ops.capture_begin(core->ops.ctx, &frame, &handle);
    bool valid = frame.width > 0u && frame.width <= 480u &&
                 frame.height > 0u && frame.height <= 480u &&
                 frame.stride_bytes >= frame.width * 2u && frame.stride_bytes <= 1024u &&
                 frame.size_bytes == (uint32_t)frame.stride_bytes * frame.height &&
                 frame.size_bytes <= MOSAICO_DIAGNOSTICS_MAX_FRAME_BYTES && handle;
    if (err != ESP_OK || !valid) {
        if (handle) {
            core->ops.capture_end(core->ops.ctx, handle);
        }
        queue_error(core, id, err == ESP_ERR_NOT_SUPPORTED ? "unsupported" : "frame",
                    err != ESP_OK ? err : ESP_ERR_INVALID_SIZE);
        return;
    }
    core->capture_active = true;
    core->capture_timer_armed = false;
    core->capture_handle = handle;
    core->capture_frame = frame;
    core->capture_id = id;
    core->capture_offset = 0;
    core->capture_crc = UINT32_MAX;
    core->capture_started_ms = now_ms;
    core->last_tx_progress_ms = now_ms;
    (void)queue_line(core, id, "FRAME %u %u %u RGB565LE %" PRIu32,
                     (unsigned)frame.width, (unsigned)frame.height,
                     (unsigned)frame.stride_bytes, frame.size_bytes);
}

static void begin_tasks(mosaico_diagnostics_core_t *core, uint32_t id, uint64_t now_ms)
{
    if (core->capture_active) {
        queue_error(core, id, "busy", ESP_OK);
        return;
    }
    if (core->tasks_seen && (now_ms < core->last_tasks_ms ||
        now_ms - core->last_tasks_ms < MOSAICO_DIAGNOSTICS_TASK_INTERVAL_MS)) {
        queue_error(core, id, "rate", ESP_OK);
        return;
    }
    core->tasks_seen = true;
    core->last_tasks_ms = now_ms;
    esp_err_t err = core->tasks_snapshot ? core->tasks_snapshot(&core->tasks) : ESP_ERR_NOT_SUPPORTED;
    if (err != ESP_OK) {
        queue_error(core, id, "tasks", err);
        return;
    }
    if (core->tasks.total_tasks == 0u || core->tasks.total_tasks > MOSAICO_DIAGNOSTICS_TASK_SLOTS ||
        (core->tasks.counter_bits != 32u && core->tasks.counter_bits != 64u)) {
        queue_error(core, id, "tasks", ESP_ERR_INVALID_SIZE);
        return;
    }
    (void)queue_line(core, id,
        "TASKS {\"snapshot_ms\":%" PRIu64 ",\"total_tasks\":%u,\"listed_tasks\":%u,"
        "\"counter_bits\":%u,\"counter_total_us\":%" PRIu64 ",\"pc_supported\":false}",
        now_ms, (unsigned)core->tasks.total_tasks, MOSAICO_DIAGNOSTICS_LISTED_TASKS,
        (unsigned)core->tasks.counter_bits, core->tasks.counter_total_us);
    core->tasks_active = true;
    core->tasks_id = id;
    core->tasks_index = 0;
    core->tasks_started_ms = now_ms;
    core->last_tx_progress_ms = now_ms;
}

static const char *task_state_name(mosaico_diagnostics_task_state_t state)
{
    switch (state) {
    case MOSAICO_DIAGNOSTICS_TASK_MISSING: return "missing";
    case MOSAICO_DIAGNOSTICS_TASK_RUNNING: return "running";
    case MOSAICO_DIAGNOSTICS_TASK_READY: return "ready";
    case MOSAICO_DIAGNOSTICS_TASK_BLOCKED: return "blocked";
    case MOSAICO_DIAGNOSTICS_TASK_SUSPENDED: return "suspended";
    case MOSAICO_DIAGNOSTICS_TASK_DELETED: return "deleted";
    default: return "invalid";
    }
}

static void tick_tasks(mosaico_diagnostics_core_t *core, uint64_t now_ms)
{
    if (now_ms < core->tasks_started_ms || now_ms < core->last_tx_progress_ms ||
        now_ms - core->tasks_started_ms >= 5000u ||
        now_ms - core->last_tx_progress_ms >= MOSAICO_DIAGNOSTICS_TX_STALL_MS) {
        core->tasks_active = false;
        core->tx_head = 0;
        core->tx_count = 0;
        queue_error(core, core->tasks_id, "timeout", ESP_OK);
        return;
    }
    if (core->tx_count != 0u) return;
    if (core->tasks_index == MOSAICO_DIAGNOSTICS_LISTED_TASKS) {
        core->tasks_active = false;
        (void)queue_line(core, core->tasks_id, "OK {\"tasks\":%u}", MOSAICO_DIAGNOSTICS_LISTED_TASKS);
        return;
    }
    const mosaico_diagnostics_task_t *entry = &core->tasks.entries[core->tasks_index];
    bool ok = queue_line(core, core->tasks_id,
        "TASK {\"name\":\"%s\",\"present\":%s,\"number\":%" PRIu32 ",\"state\":\"%s\","
        "\"priority\":%" PRIu32 ",\"base_priority\":%" PRIu32 ",\"stack_min_bytes\":%" PRIu32 ","
        "\"runtime_us\":%" PRIu64 ",\"affinity\":%d}",
        mosaico_diagnostics_task_name(core->tasks_index), entry->present ? "true" : "false",
        entry->number, task_state_name(entry->state), entry->priority, entry->base_priority,
        entry->stack_min_bytes, entry->runtime_us, (int)entry->affinity);
    if (ok) ++core->tasks_index;
}

static void execute(mosaico_diagnostics_core_t *core, const command_t *cmd, uint64_t now_ms)
{
    if (core->tasks_active && cmd->kind != COMMAND_ABORT) {
        queue_error(core, cmd->id, "busy", ESP_OK);
        return;
    }
    esp_err_t err = ESP_ERR_NOT_SUPPORTED;
    switch (cmd->kind) {
    case COMMAND_PING:
        (void)queue_line(core, cmd->id, "OK mac=%02x:%02x:%02x:%02x:%02x:%02x protocol=1",
                         core->mac[0], core->mac[1], core->mac[2], core->mac[3], core->mac[4], core->mac[5]);
        return;
    case COMMAND_STATUS:
        status_reply(core, cmd->id, now_ms);
        return;
    case COMMAND_TASKS:
        begin_tasks(core, cmd->id, now_ms);
        return;
    case COMMAND_OPEN:
        if (core->capture_active) {
            queue_error(core, cmd->id, "busy", ESP_OK);
            return;
        }
        if (core->ops.open_app) {
            err = core->ops.open_app(core->ops.ctx, (mosaico_diagnostics_app_t)cmd->args[0]);
        }
        if (err == ESP_OK) (void)queue_line(core, cmd->id, "OK admitted");
        else queue_error(core, cmd->id, err == ESP_ERR_NOT_SUPPORTED ? "unsupported" : "ui", err);
        return;
    case COMMAND_TAP:
        if (core->ops.tap) {
            err = core->ops.tap(core->ops.ctx, (uint16_t)cmd->args[0], (uint16_t)cmd->args[1]);
        }
        break;
    case COMMAND_DRAG:
        if (core->ops.drag) {
            err = core->ops.drag(core->ops.ctx, (uint16_t)cmd->args[0], (uint16_t)cmd->args[1],
                                 (uint16_t)cmd->args[2], (uint16_t)cmd->args[3], cmd->args[4]);
        }
        break;
    case COMMAND_BACK:
        if (core->ops.back) {
            err = core->ops.back(core->ops.ctx);
        }
        break;
    case COMMAND_CAPTURE:
        begin_capture(core, cmd->id, now_ms);
        return;
    case COMMAND_ABORT:
        if (core->tasks_active) {
            core->tasks_active = false;
            queue_error(core, core->tasks_id, "cancelled", ESP_OK);
        }
        if (core->capture_active) {
            uint32_t capture_id = core->capture_id;
            release_capture(core);
            queue_error(core, capture_id, "cancelled", ESP_OK);
        }
        (void)queue_line(core, cmd->id, "OK aborted");
        return;
    }
    if (err == ESP_OK) {
        (void)queue_line(core, cmd->id, "OK queued");
    } else {
        queue_error(core, cmd->id, err == ESP_ERR_NOT_SUPPORTED ? "unsupported" : "ui", err);
    }
}

bool mosaico_diagnostics_core_init(mosaico_diagnostics_core_t *core,
                                  const mosaico_diagnostics_ops_t *ops,
                                  const uint8_t mac[6])
{
    if (!core || !ops || !ops->status || !mac) {
        return false;
    }
    bool capture_any = ops->capture_begin || ops->capture_read || ops->capture_end;
    bool capture_all = ops->capture_begin && ops->capture_read && ops->capture_end;
    if (capture_any && !capture_all) {
        return false;
    }
    memset(core, 0, sizeof(*core));
    core->ops = *ops;
    memcpy(core->mac, mac, sizeof(core->mac));
    return true;
}

size_t mosaico_diagnostics_core_feed(mosaico_diagnostics_core_t *core,
                                    const uint8_t *data, size_t len, uint64_t now_ms)
{
    if (!core || !data || core->tx_count != 0u) {
        return 0;
    }
    for (size_t i = 0; i < len; ++i) {
        uint8_t ch = data[i];
        if (ch == '\n' || ch == '\r') {
            core->line[core->line_len] = '\0';
            if (core->line_len >= strlen("@MOSAICO ") &&
                memcmp(core->line, "@MOSAICO ", strlen("@MOSAICO ")) == 0) {
                command_t cmd = {0};
                const char *error = core->line_invalid ? "line" : parse_line(core->line, &cmd);
                if (core->command_seen && (now_ms < core->last_command_ms ||
                    now_ms - core->last_command_ms < MOSAICO_DIAGNOSTICS_COMMAND_INTERVAL_MS)) {
                    queue_error(core, cmd.id, "rate", ESP_OK);
                } else {
                    core->command_seen = true;
                    core->last_command_ms = now_ms;
                    if (error) {
                        queue_error(core, cmd.id, error, ESP_OK);
                    } else {
                        execute(core, &cmd, now_ms);
                    }
                }
            }
            core->line_len = 0;
            core->line_invalid = false;
            return i + 1u;
        }
        if (ch < 32u || ch > 126u || core->line_len == MOSAICO_DIAGNOSTICS_MAX_LINE) {
            core->line_invalid = true;
        } else if (!core->line_invalid) {
            core->line[core->line_len++] = (char)ch;
        }
    }
    return len;
}

uint32_t mosaico_diagnostics_crc32_update(uint32_t state, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        state ^= data[i];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            state = (state >> 1) ^ (0xedb88320u & (0u - (state & 1u)));
        }
    }
    return state;
}

static void base64(char *out, const uint8_t *data, size_t len)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    for (size_t i = 0; i < len; i += 3u) {
        uint32_t word = (uint32_t)data[i] << 16;
        if (i + 1u < len) word |= (uint32_t)data[i + 1u] << 8;
        if (i + 2u < len) word |= data[i + 2u];
        out[pos++] = alphabet[(word >> 18) & 63u];
        out[pos++] = alphabet[(word >> 12) & 63u];
        out[pos++] = i + 1u < len ? alphabet[(word >> 6) & 63u] : '=';
        out[pos++] = i + 2u < len ? alphabet[word & 63u] : '=';
    }
    out[pos] = '\0';
}

void mosaico_diagnostics_core_tick(mosaico_diagnostics_core_t *core, uint64_t now_ms)
{
    if (core && core->tasks_active) {
        tick_tasks(core, now_ms);
        return;
    }
    if (!core || !core->capture_active) {
        return;
    }
    if (!core->capture_timer_armed) {
        /* A finite UI copy/fence may outlast the USB stall deadline. */
        core->last_tx_progress_ms = now_ms;
        core->capture_timer_armed = true;
    }
    if (now_ms < core->capture_started_ms || now_ms < core->last_tx_progress_ms ||
        now_ms - core->capture_started_ms >= MOSAICO_DIAGNOSTICS_CAPTURE_LIMIT_MS ||
        now_ms - core->last_tx_progress_ms >= MOSAICO_DIAGNOSTICS_TX_STALL_MS) {
        uint32_t id = core->capture_id;
        release_capture(core);
        /* A blocked host must not keep a snapshot or a partial DATA packet. */
        core->tx_head = 0;
        core->tx_count = 0;
        queue_error(core, id, "timeout", ESP_OK);
        return;
    }
    if (core->tx_count != 0u) {
        return;
    }
    if (core->capture_offset == core->capture_frame.size_bytes) {
        (void)queue_line(core, core->capture_id, "END %" PRIu32 " %08" PRIx32,
                         core->capture_offset, ~core->capture_crc);
        release_capture(core);
        return;
    }
    uint8_t chunk[MOSAICO_DIAGNOSTICS_FRAME_CHUNK];
    uint32_t remaining = core->capture_frame.size_bytes - core->capture_offset;
    size_t len = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    esp_err_t err = core->ops.capture_read(core->ops.ctx, core->capture_handle,
                                          core->capture_offset, chunk, len);
    if (err != ESP_OK) {
        uint32_t id = core->capture_id;
        release_capture(core);
        queue_error(core, id, "frame", err);
        return;
    }
    char encoded[4u * ((MOSAICO_DIAGNOSTICS_FRAME_CHUNK + 2u) / 3u) + 1u];
    base64(encoded, chunk, len);
    uint32_t crc = ~mosaico_diagnostics_crc32_update(UINT32_MAX, chunk, len);
    if (queue_line(core, core->capture_id, "DATA %" PRIu32 " %u %08" PRIx32 " %s",
                    core->capture_offset, (unsigned)len, crc, encoded)) {
        core->capture_crc = mosaico_diagnostics_crc32_update(core->capture_crc, chunk, len);
        core->capture_offset += (uint32_t)len;
    }
}

const mosaico_diagnostics_packet_t *mosaico_diagnostics_core_packet(
    const mosaico_diagnostics_core_t *core)
{
    return core && core->tx_count ? &core->tx[core->tx_head] : NULL;
}

void mosaico_diagnostics_core_sent(mosaico_diagnostics_core_t *core, uint64_t now_ms)
{
    if (core && core->tx_count) {
        core->tx[core->tx_head].len = 0;
        core->tx_head = (core->tx_head + 1u) % MOSAICO_DIAGNOSTICS_TX_DEPTH;
        --core->tx_count;
        if (core->capture_active || core->tasks_active) {
            core->last_tx_progress_ms = now_ms;
            core->capture_timer_armed = true;
        }
    }
}

void mosaico_diagnostics_core_disconnect(mosaico_diagnostics_core_t *core)
{
    if (!core) {
        return;
    }
    release_capture(core);
    core->tasks_active = false;
    core->tasks_seen = false;
    core->line_len = 0;
    core->line_invalid = false;
    core->tx_head = 0;
    core->tx_count = 0;
    core->command_seen = false;
}
