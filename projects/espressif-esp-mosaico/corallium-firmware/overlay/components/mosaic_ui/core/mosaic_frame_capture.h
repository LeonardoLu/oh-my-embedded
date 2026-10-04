/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_display_present.h"
#include "mosaic_ui.h"

esp_err_t mosaic_frame_capture_create(const esp_display_presenter_caps_t *caps,
    mosaic_ui_frame_handle_t *out_frame);
/** Arm only while the real producer and presentation fences are paused. */
esp_err_t mosaic_frame_capture_arm(mosaic_ui_frame_handle_t frame,
    esp_display_presenter_t *presenter);
typedef struct {
    bool seen;
    bool full;
    bool committed;
    uint32_t tiles;
    uint32_t covered_pixels;
} mosaic_frame_capture_progress_t;
/** Numeric progress only; safe while the renderer is submitting tiles. */
void mosaic_frame_capture_progress(mosaic_ui_frame_handle_t frame,
    mosaic_frame_capture_progress_t *out);
typedef struct {
    uint32_t calls;
    uint32_t elapsed_ms;
    esp_err_t error;
} mosaic_frame_capture_pause_progress_t;
/** Pair under the caller's UI lifetime lock; observe public fence waits only. */
void mosaic_frame_capture_pause_probe_begin(esp_display_presenter_t *presenter);
void mosaic_frame_capture_pause_probe_end(
    mosaic_frame_capture_pause_progress_t *out);
/** Unpublish and wait for the short tile-copy readers before freeing. */
void mosaic_frame_capture_detach(mosaic_ui_frame_handle_t frame);
/** A copy is readable only after complete coverage, commit and present fence. */
esp_err_t mosaic_frame_capture_complete(mosaic_ui_frame_handle_t frame,
    bool present_fenced);
esp_err_t mosaic_frame_capture_info(mosaic_ui_frame_handle_t frame,
    mosaic_ui_frame_info_t *out);
esp_err_t mosaic_frame_capture_read(mosaic_ui_frame_handle_t frame,
    size_t offset, void *dst, size_t length);
void mosaic_frame_capture_delete(mosaic_ui_frame_handle_t frame);
