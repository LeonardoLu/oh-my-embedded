/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAICO_DIAGNOSTICS_MAX_LINE 128u
#define MOSAICO_DIAGNOSTICS_MAX_FRAME_BYTES (480u * 1024u)
#define MOSAICO_DIAGNOSTICS_FRAME_CHUNK 192u

typedef enum {
    MOSAICO_DIAGNOSTICS_OWNER_NONE,
    MOSAICO_DIAGNOSTICS_OWNER_GSP,
    MOSAICO_DIAGNOSTICS_OWNER_EXCLUSIVE,
} mosaico_diagnostics_owner_t;

typedef enum {
    MOSAICO_DIAGNOSTICS_APP_SETTINGS,
    MOSAICO_DIAGNOSTICS_APP_WORKS,
    MOSAICO_DIAGNOSTICS_APP_ALBUM,
    MOSAICO_DIAGNOSTICS_APP_WEATHER,
} mosaico_diagnostics_app_t;

/* Public diagnostic values only: no scene text or network credentials. */
typedef struct {
    char app_name[32];
    uint32_t app_id;
    mosaico_diagnostics_owner_t display_owner;
    uint16_t width;
    uint16_t height;
    uint16_t rotation;
    bool started;
    bool asleep;
    bool dimmed;
    bool panel_enabled;
    bool hub_presenter_active;
    bool runtime_started;
    bool quiesced;
    bool screen_paused;
    bool drawer_open;
    bool gesture_active;
    uint16_t queue_depth;
    uint64_t render_frames;
    uint64_t render_busy_us;
    uint32_t render_errors;
    int32_t last_render_error;
    uint32_t runtime_errors;
    int32_t last_runtime_error;
    int32_t last_input_error;
    bool capture_supported;
    uint8_t volume_percent;
    uint8_t brightness_percent;
    uint8_t panel_brightness_percent;
    uint32_t dim_timeout_ms;
    uint32_t sleep_timeout_ms;
    uint32_t poweroff_timeout_ms;
    bool dim_while_charging;
    bool sleep_while_charging;
    bool wifi_enabled;
    bool wifi_connected;
    bool bluetooth_enabled;
    bool battery_available;
    bool charging;
} mosaico_diagnostics_status_t;

/* A stable copy of a device frame, never a host-rendered substitute. */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t stride_bytes;
    uint32_t size_bytes;
} mosaico_diagnostics_frame_t;

typedef struct {
    void *ctx;
    /* Must not wake the screen, reset idle time, or start network work. */
    esp_err_t (*status)(void *ctx, mosaico_diagnostics_status_t *out);
    /* These callbacks submit through the UI/loader queue, not the renderer. */
    esp_err_t (*tap)(void *ctx, uint16_t x, uint16_t y);
    esp_err_t (*drag)(void *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                      uint32_t duration_ms);
    esp_err_t (*back)(void *ctx);
    /* Fixed local apps only. Success means asynchronous queue admission. */
    esp_err_t (*open_app)(void *ctx, mosaico_diagnostics_app_t app);
    /*
     * begin copies/fences a real awake GSP frame with a finite UI timeout,
     * then releases all UI/presenter locks before returning. RGB565 bytes
     * are little-endian; size must equal stride * height. The returned handle
     * owns an immutable snapshot until end, including on USB timeout/disconnect.
     * Unsupported presenters/capture paths return ESP_ERR_NOT_SUPPORTED.
     */
    esp_err_t (*capture_begin)(void *ctx, mosaico_diagnostics_frame_t *out, void **handle);
    /* Exactly len bytes, len <= FRAME_CHUNK; no UI lock or indefinite wait. */
    esp_err_t (*capture_read)(void *ctx, void *handle, uint32_t offset,
                              uint8_t *dst, size_t len);
    void (*capture_end)(void *ctx, void *handle);
} mosaico_diagnostics_ops_t;

/*
 * Call once after bsp_usb_console_init and UI startup. This reuses CDC 0 and
 * stdin; it never installs USB, changes DTR/RTS callbacks, or starts Claw CLI.
 * Callback pointers/context must remain valid for the firmware lifetime.
 * Disabled configuration returns ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t mosaico_diagnostics_start(const mosaico_diagnostics_ops_t *ops);

#ifdef __cplusplus
}
#endif
