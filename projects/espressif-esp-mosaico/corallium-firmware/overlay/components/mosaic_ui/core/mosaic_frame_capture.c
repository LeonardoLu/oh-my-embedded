/* SPDX-License-Identifier: Apache-2.0 */

#include "mosaic_frame_capture.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MOSAIC_CAPTURE_MAX_SIDE 480U
#define MOSAIC_CAPTURE_MAX_READ_BYTES 4096U

struct mosaic_ui_frame {
    esp_display_presenter_t *presenter;
    mosaic_ui_frame_info_t info;
    uint8_t *pixels;
    uint8_t *coverage;
    atomic_uint covered_pixels;
    atomic_uint tiles;
    uint32_t readers;
    atomic_int error;
    atomic_bool seen;
    atomic_bool full;
    atomic_bool active;
    atomic_bool committed;
    bool ready;
};

static portMUX_TYPE s_capture_lock = portMUX_INITIALIZER_UNLOCKED;
static _Atomic(mosaic_ui_frame_handle_t) s_capture;
static atomic_bool s_frame_outstanding;
static _Atomic(esp_display_presenter_t *) s_pause_presenter;
static atomic_uint s_pause_calls;
static atomic_uint s_pause_elapsed_ms;
static atomic_int s_pause_error;

void mosaic_frame_capture_pause_probe_begin(esp_display_presenter_t *presenter)
{
    atomic_store(&s_pause_calls, 0U);
    atomic_store(&s_pause_elapsed_ms, 0U);
    atomic_store(&s_pause_error, ESP_OK);
    atomic_store_explicit(&s_pause_presenter, presenter, memory_order_release);
}

void mosaic_frame_capture_pause_probe_end(
    mosaic_frame_capture_pause_progress_t *out)
{
    atomic_store_explicit(&s_pause_presenter, NULL, memory_order_release);
    if (out != NULL) {
        out->calls = atomic_load(&s_pause_calls);
        out->elapsed_ms = atomic_load(&s_pause_elapsed_ms);
        out->error = atomic_load(&s_pause_error);
    }
}

/* The pointer/ref claim is atomic with detach. Copies run outside the spinlock;
 * no wrapper keeps a reference while the real presenter waits for hardware. */
static mosaic_ui_frame_handle_t capture_acquire(esp_display_presenter_t *presenter)
{
    if (atomic_load_explicit(&s_capture, memory_order_acquire) == NULL) return NULL;
    portENTER_CRITICAL(&s_capture_lock);
    mosaic_ui_frame_handle_t frame = atomic_load_explicit(&s_capture, memory_order_relaxed);
    if (frame != NULL && frame->presenter == presenter) {
        ++frame->readers;
    } else {
        frame = NULL;
    }
    portEXIT_CRITICAL(&s_capture_lock);
    return frame;
}

static void capture_release(mosaic_ui_frame_handle_t frame)
{
    portENTER_CRITICAL(&s_capture_lock);
    --frame->readers;
    portEXIT_CRITICAL(&s_capture_lock);
}

esp_err_t mosaic_frame_capture_create(const esp_display_presenter_caps_t *caps,
    mosaic_ui_frame_handle_t *out_frame)
{
    if (caps == NULL || out_frame == NULL) return ESP_ERR_INVALID_ARG;
    *out_frame = NULL;
    if (caps->contract != ESP_DISPLAY_PRESENT_CONTRACT_PARTITION ||
            caps->pixel_format != ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 ||
            caps->width == 0 || caps->height == 0 ||
            caps->width > MOSAIC_CAPTURE_MAX_SIDE ||
            caps->height > MOSAIC_CAPTURE_MAX_SIDE) return ESP_ERR_NOT_SUPPORTED;
    bool outstanding = false;
    if (!atomic_compare_exchange_strong(&s_frame_outstanding,
            &outstanding, true)) return ESP_ERR_INVALID_STATE;
    mosaic_ui_frame_handle_t frame = calloc(1, sizeof(*frame));
    if (frame != NULL) {
        frame->info.width = caps->width;
        frame->info.height = caps->height;
        frame->info.stride_bytes = caps->width * 2U;
        frame->info.size_bytes = (uint32_t)frame->info.stride_bytes * caps->height;
        frame->pixels = heap_caps_malloc(frame->info.size_bytes,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        frame->coverage = heap_caps_calloc(
            ((size_t)caps->width * caps->height + 7U) / 8U, 1,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (frame == NULL || frame->pixels == NULL || frame->coverage == NULL) {
        if (frame != NULL) {
            free(frame->pixels);
            free(frame->coverage);
            free(frame);
        }
        atomic_store(&s_frame_outstanding, false);
        return ESP_ERR_NO_MEM;
    }
    *out_frame = frame;
    return ESP_OK;
}

esp_err_t mosaic_frame_capture_arm(mosaic_ui_frame_handle_t frame,
    esp_display_presenter_t *presenter)
{
    if (frame == NULL || presenter == NULL || frame->seen) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_capture_lock);
    esp_err_t error = atomic_load_explicit(&s_capture, memory_order_relaxed) == NULL
        ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (error == ESP_OK) {
        frame->presenter = presenter;
        atomic_store_explicit(&s_capture, frame, memory_order_release);
    }
    portEXIT_CRITICAL(&s_capture_lock);
    return error;
}

void mosaic_frame_capture_progress(mosaic_ui_frame_handle_t frame,
    mosaic_frame_capture_progress_t *out)
{
    if (out == NULL) return;
    *out = (mosaic_frame_capture_progress_t){0};
    if (frame != NULL) {
        out->seen = atomic_load(&frame->seen);
        out->full = atomic_load(&frame->full);
        out->committed = atomic_load(&frame->committed);
        out->tiles = atomic_load(&frame->tiles);
        out->covered_pixels = atomic_load(&frame->covered_pixels);
    }
}

void mosaic_frame_capture_detach(mosaic_ui_frame_handle_t frame)
{
    if (frame == NULL) return;
    portENTER_CRITICAL(&s_capture_lock);
    if (atomic_load_explicit(&s_capture, memory_order_relaxed) == frame) {
        atomic_store_explicit(&s_capture, NULL, memory_order_release);
    }
    uint32_t readers = frame->readers;
    portEXIT_CRITICAL(&s_capture_lock);
    /* Readers only validate/copy at most one display tile and never call USB,
     * acquire a UI mutex, or wait for the real presenter. */
    while (readers != 0U) {
        vTaskDelay(1);
        portENTER_CRITICAL(&s_capture_lock);
        readers = frame->readers;
        portEXIT_CRITICAL(&s_capture_lock);
    }
}

esp_err_t mosaic_frame_capture_complete(mosaic_ui_frame_handle_t frame,
    bool present_fenced)
{
    if (frame == NULL) return ESP_ERR_INVALID_ARG;
    if (!present_fenced) return ESP_ERR_TIMEOUT;
    if (frame->error != ESP_OK) return frame->error;
    if (!frame->committed || frame->active || frame->covered_pixels !=
            (uint32_t)frame->info.width * frame->info.height) {
        return ESP_ERR_INVALID_STATE;
    }
    frame->ready = true;
    return ESP_OK;
}

esp_err_t mosaic_frame_capture_info(mosaic_ui_frame_handle_t frame,
    mosaic_ui_frame_info_t *out)
{
    if (frame == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    if (!frame->ready) return ESP_ERR_INVALID_STATE;
    *out = frame->info;
    return ESP_OK;
}

esp_err_t mosaic_frame_capture_read(mosaic_ui_frame_handle_t frame,
    size_t offset, void *dst, size_t length)
{
    if (frame == NULL || dst == NULL || length == 0U ||
            length > MOSAIC_CAPTURE_MAX_READ_BYTES) return ESP_ERR_INVALID_ARG;
    if (!frame->ready) return ESP_ERR_INVALID_STATE;
    if (offset > frame->info.size_bytes ||
            length > frame->info.size_bytes - offset) return ESP_ERR_INVALID_SIZE;
    memcpy(dst, frame->pixels + offset, length);
    return ESP_OK;
}

void mosaic_frame_capture_delete(mosaic_ui_frame_handle_t frame)
{
    if (frame == NULL) return;
    mosaic_frame_capture_detach(frame);
    free(frame->pixels);
    free(frame->coverage);
    free(frame);
    atomic_store(&s_frame_outstanding, false);
}

/* Link-time wrappers preserve every real argument and return value. No frame
 * is allocated or copied until a diagnostic request explicitly arms one. */
esp_err_t __real_esp_display_presenter_begin_next_frame(
    esp_display_presenter_t *, const esp_display_present_surface_request_t *,
    esp_display_present_area_t *, size_t, size_t *, bool *);
esp_err_t __wrap_esp_display_presenter_begin_next_frame(
    esp_display_presenter_t *presenter,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *areas, size_t capacity,
    size_t *count, bool *full)
{
    esp_err_t error = __real_esp_display_presenter_begin_next_frame(
        presenter, request, areas, capacity, count, full);
    mosaic_ui_frame_handle_t frame = capture_acquire(presenter);
    if (frame != NULL) {
        if (!frame->seen) {
            frame->seen = true;
            frame->full = error == ESP_OK && full != NULL && *full;
            frame->active = frame->full;
            frame->error = error != ESP_OK ? error :
                frame->active ? ESP_OK : ESP_ERR_INVALID_STATE;
        }
        capture_release(frame);
    }
    return error;
}

static void capture_tile(mosaic_ui_frame_handle_t frame,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area, size_t stride_bytes)
{
    /* GSP synchronizes its accelerator before handing this CPU-readable tile
     * to the presenter (whose TE path may byte-swap/copy it on the CPU). Do not
     * invalidate the tile here: mixed software drawing can have dirty lines. */
    if (!frame->active || frame->error != ESP_OK) return;
    ++frame->tiles;
    if (buffer == NULL || area == NULL || buffer->surface.pixels == NULL ||
            buffer->surface.pixel_format != ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 ||
            area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 ||
            area->y2 < area->y1 || area->x2 >= frame->info.width ||
            area->y2 >= frame->info.height) {
        frame->error = ESP_ERR_INVALID_ARG;
        return;
    }
    const size_t width = (size_t)(area->x2 - area->x1 + 1);
    const size_t height = (size_t)(area->y2 - area->y1 + 1);
    if (stride_bytes < width * 2U || stride_bytes == 0U ||
            height > buffer->capacity_bytes / stride_bytes) {
        frame->error = ESP_ERR_INVALID_SIZE;
        return;
    }
    uint32_t covered = 0;
    for (size_t row = 0; row < height; ++row) {
        const size_t pixel = (area->y1 + row) * frame->info.width + area->x1;
        memcpy(frame->pixels + pixel * 2U,
            (const uint8_t *)buffer->surface.pixels + row * stride_bytes,
            width * 2U);
        for (size_t x = 0; x < width; ++x) {
            const size_t index = pixel + x;
            const uint8_t mask = (uint8_t)(1U << (index & 7U));
            if (!(frame->coverage[index / 8U] & mask)) {
                frame->coverage[index / 8U] |= mask;
                ++covered;
            }
        }
    }
    atomic_fetch_add(&frame->covered_pixels, covered);
}

esp_err_t __real_esp_display_presenter_submit_buffer(esp_display_presenter_t *,
    const esp_display_presenter_buffer_t *, const esp_display_present_area_t *, size_t);
esp_err_t __wrap_esp_display_presenter_submit_buffer(esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area, size_t stride_bytes)
{
    mosaic_ui_frame_handle_t frame = capture_acquire(presenter);
    if (frame != NULL) {
        capture_tile(frame, buffer, area, stride_bytes);
        capture_release(frame);
    }
    esp_err_t error = __real_esp_display_presenter_submit_buffer(
        presenter, buffer, area, stride_bytes);
    if (error != ESP_OK) {
        frame = capture_acquire(presenter);
        if (frame != NULL) {
            if (frame->active) frame->error = error;
            capture_release(frame);
        }
    }
    return error;
}

esp_err_t __real_esp_display_presenter_commit_frame(esp_display_presenter_t *,
    const esp_display_presenter_submit_t *);
esp_err_t __wrap_esp_display_presenter_commit_frame(esp_display_presenter_t *presenter,
    const esp_display_presenter_submit_t *submit)
{
    esp_err_t error = __real_esp_display_presenter_commit_frame(presenter, submit);
    mosaic_ui_frame_handle_t frame = capture_acquire(presenter);
    if (frame != NULL) {
        if (frame->active) {
            frame->active = false;
            frame->committed = error == ESP_OK;
            if (error != ESP_OK) frame->error = error;
        }
        capture_release(frame);
    }
    return error;
}

void __real_esp_display_presenter_cancel_frame(esp_display_presenter_t *);
void __wrap_esp_display_presenter_cancel_frame(esp_display_presenter_t *presenter)
{
    __real_esp_display_presenter_cancel_frame(presenter);
    mosaic_ui_frame_handle_t frame = capture_acquire(presenter);
    if (frame != NULL) {
        if (frame->active) {
            frame->active = false;
            frame->error = ESP_ERR_INVALID_STATE;
        }
        capture_release(frame);
    }
}

esp_err_t __real_esp_display_presenter_quiesce(
    esp_display_presenter_t *, uint32_t);
esp_err_t __wrap_esp_display_presenter_quiesce(
    esp_display_presenter_t *presenter, uint32_t timeout_ms)
{
    if (presenter == NULL || atomic_load_explicit(&s_pause_presenter,
            memory_order_acquire) != presenter) {
        return __real_esp_display_presenter_quiesce(presenter, timeout_ms);
    }
    atomic_fetch_add(&s_pause_calls, 1U);
    const int64_t start_us = esp_timer_get_time();
    /* Observe without retaining a frame copy, lock, or changing the fence. */
    const esp_err_t error = __real_esp_display_presenter_quiesce(
        presenter, timeout_ms);
    atomic_fetch_add(&s_pause_elapsed_ms,
        (uint32_t)((esp_timer_get_time() - start_us) / 1000));
    atomic_store(&s_pause_error, error);
    return error;
}
