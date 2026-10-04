/*
* Copyright 2023 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "custom.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**
 * Create a demo application
 */

void custom_init(lv_ui *ui)
{
    /* Add your codes here */
}

static void output_status_draw_cb(lv_event_t *e)
{
    lv_obj_t *badge = lv_event_get_target(e);
    lv_area_t area;
    lv_obj_get_coords(badge, &area);

    // Fill the whole corner cutout, including the old bitmap's black inset.
    const lv_point_t points[] = {
        {area.x1 + 2, area.y1}, {area.x2, area.y1},
        {area.x2, area.y1 + 24}, {area.x1 + 26, area.y1 + 24}
    };
    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = lv_obj_get_style_bg_color(badge, LV_PART_INDICATOR);
    lv_draw_polygon(lv_event_get_draw_ctx(e), &draw_dsc, points, 4);
}

lv_obj_t *pps_output_status_create(lv_obj_t *parent)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, 226, 0);
    lv_obj_set_size(badge, 94, 26);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xf3f3f3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x009933), LV_PART_INDICATOR);
    lv_obj_add_event_cb(badge, output_status_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *label = lv_label_create(badge);
    lv_label_set_text_static(label, "OFF");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 7, -1);
    return badge;
}

void pps_output_status_set(lv_obj_t *badge, bool enabled)
{
    lv_obj_t *label = lv_obj_get_child(badge, 0);
    const char *text = enabled ? "ON" : "OFF";
    if (strcmp(lv_label_get_text(label), text) == 0)
        return;

    lv_label_set_text_static(label, text);
    lv_obj_set_style_bg_color(badge,
        lv_color_hex(enabled ? 0xcc3300 : 0x009933), LV_PART_INDICATOR);
    lv_obj_invalidate(badge);
}
