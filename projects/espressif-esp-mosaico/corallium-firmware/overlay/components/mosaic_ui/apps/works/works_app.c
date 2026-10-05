/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mosaic_app_catalog.h"
#include "mosaic_hub_actions.h"
#include "mosaic_liquid.h"
#include "mosaic_runtime.h"
#include "works_actions.h"
#include "works_binds.h"
#include "works_objects.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORKS_APP_ID    7U
#define WORKS_ROW_COUNT 4U
#define WORKS_NATIVE_COUNT 3U

static const char *const s_native_ids[] = {"liquid_pixel", "liquid_gradient", "liquid_water"};
static const char *const s_native_names[] = {"Pixel", "Gradient", "Water"};
static const uint16_t s_native_visible_binds[] = {
    GSP_BIND_WORKS_NATIVE_PIXEL_VISIBLE,
    GSP_BIND_WORKS_NATIVE_GRADIENT_VISIBLE,
    GSP_BIND_WORKS_NATIVE_WATER_VISIBLE,
};
static const uint16_t s_row_lua_visible_binds[] = {
    GSP_BIND_WORKS_ROW_0_LUA_VISIBLE, GSP_BIND_WORKS_ROW_1_LUA_VISIBLE,
    GSP_BIND_WORKS_ROW_2_LUA_VISIBLE, GSP_BIND_WORKS_ROW_3_LUA_VISIBLE,
};

static const mosaic_app_route_t s_native_routes[] = {
    {GSP_ACT_ID_WORKS_PIXEL, "liquid"},
    {GSP_ACT_ID_WORKS_GRADIENT, "liquid"},
    {GSP_ACT_ID_WORKS_WATER, "liquid"},
};

static const uint16_t s_row_visible_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_VISIBLE,
    GSP_BIND_WORKS_ROW_1_VISIBLE,
    GSP_BIND_WORKS_ROW_2_VISIBLE,
    GSP_BIND_WORKS_ROW_3_VISIBLE,
};
static const uint16_t s_row_active_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_ACTIVE_VISIBLE,
    GSP_BIND_WORKS_ROW_1_ACTIVE_VISIBLE,
    GSP_BIND_WORKS_ROW_2_ACTIVE_VISIBLE,
    GSP_BIND_WORKS_ROW_3_ACTIVE_VISIBLE,
};
static const uint16_t s_row_stopped_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_STOPPED_VISIBLE,
    GSP_BIND_WORKS_ROW_1_STOPPED_VISIBLE,
    GSP_BIND_WORKS_ROW_2_STOPPED_VISIBLE,
    GSP_BIND_WORKS_ROW_3_STOPPED_VISIBLE,
};
static const uint16_t s_row_failed_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_FAILED_VISIBLE,
    GSP_BIND_WORKS_ROW_1_FAILED_VISIBLE,
    GSP_BIND_WORKS_ROW_2_FAILED_VISIBLE,
    GSP_BIND_WORKS_ROW_3_FAILED_VISIBLE,
};

static void works_clear_rows(esp_gsp_handle_t ui)
{
    for (size_t row = 0; row < WORKS_ROW_COUNT; ++row) {
        (void)esp_gsp_set_visible(ui, s_row_visible_binds[row], false);
        (void)esp_gsp_set_visible(ui, s_row_active_binds[row], false);
        (void)esp_gsp_set_visible(ui, s_row_stopped_binds[row], false);
        (void)esp_gsp_set_visible(ui, s_row_failed_binds[row], false);
        (void)esp_gsp_set_visible(ui, s_row_lua_visible_binds[row], false);
        if (row < WORKS_NATIVE_COUNT) (void)esp_gsp_set_visible(ui, s_native_visible_binds[row], false);
    }
}

#if defined(ESP_PLATFORM)

#include "esp_log.h"
#include "mosaic_loader.h"
#include "works_runtime.h"

typedef enum {
    WORKS_TAB_RECENT = 0,
    WORKS_TAB_INSTALLED,
    WORKS_TAB_LAB,
} works_tab_t;

static const uint16_t s_row_name_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_NAME,
    GSP_BIND_WORKS_ROW_1_NAME,
    GSP_BIND_WORKS_ROW_2_NAME,
    GSP_BIND_WORKS_ROW_3_NAME,
};
static const uint16_t s_row_meta_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_META,
    GSP_BIND_WORKS_ROW_1_META,
    GSP_BIND_WORKS_ROW_2_META,
    GSP_BIND_WORKS_ROW_3_META,
};
static const uint16_t s_row_active_text_binds[WORKS_ROW_COUNT] = {
    GSP_BIND_WORKS_ROW_0_ACTIVE_TEXT,
    GSP_BIND_WORKS_ROW_1_ACTIVE_TEXT,
    GSP_BIND_WORKS_ROW_2_ACTIVE_TEXT,
    GSP_BIND_WORKS_ROW_3_ACTIVE_TEXT,
};
static const gsp_component_key_t s_row_visibility_keys[WORKS_ROW_COUNT][4] = {
    {GSP_OBJ_KEY_WORKS_ROW_0, GSP_OBJ_KEY_WORKS_ROW_0_ACTIVE, GSP_OBJ_KEY_WORKS_ROW_0_STOPPED, GSP_OBJ_KEY_WORKS_ROW_0_FAILED},
    {GSP_OBJ_KEY_WORKS_ROW_1, GSP_OBJ_KEY_WORKS_ROW_1_ACTIVE, GSP_OBJ_KEY_WORKS_ROW_1_STOPPED, GSP_OBJ_KEY_WORKS_ROW_1_FAILED},
    {GSP_OBJ_KEY_WORKS_ROW_2, GSP_OBJ_KEY_WORKS_ROW_2_ACTIVE, GSP_OBJ_KEY_WORKS_ROW_2_STOPPED, GSP_OBJ_KEY_WORKS_ROW_2_FAILED},
    {GSP_OBJ_KEY_WORKS_ROW_3, GSP_OBJ_KEY_WORKS_ROW_3_ACTIVE, GSP_OBJ_KEY_WORKS_ROW_3_STOPPED, GSP_OBJ_KEY_WORKS_ROW_3_FAILED},
};

static const char *TAG = "works_app";
static works_tab_t s_selected_tab = WORKS_TAB_LAB;
static size_t s_installed_page;

static bool works_is_lab_item(const works_runtime_item_snapshot_t *item)
{
    return strcmp(item->app_id, "dino") == 0 ||
           strcmp(item->app_id, "flappybird") == 0;
}

static size_t works_page_item_count(void)
{
    size_t count = 0;
    if (works_runtime_get_count(&count) != ESP_OK) {
        count = 0;
    }
    if (s_selected_tab != WORKS_TAB_LAB) {
        return count + (s_selected_tab == WORKS_TAB_INSTALLED ? WORKS_NATIVE_COUNT : 0);
    }
    size_t lab_count = 0;
    for (size_t index = 0; index < count; ++index) {
        works_runtime_item_snapshot_t item;
        if (works_runtime_get_item(index, &item) == ESP_OK && works_is_lab_item(&item)) {
            ++lab_count;
        }
    }
    return lab_count + WORKS_NATIVE_COUNT;
}

static esp_err_t works_first_error(esp_err_t current, esp_err_t next)
{
    return current == ESP_OK ? next : current;
}

static esp_err_t works_set_row_visibility(esp_gsp_handle_t ui, size_t row, bool visible, bool active, bool stopped, bool failed)
{
    const bool values[] = {visible, active, stopped, failed};
    gsp_component_update_t updates[4];
    for (size_t i = 0; i < 4U; ++i) {
        updates[i] = (gsp_component_update_t) {
            .key = s_row_visibility_keys[row][i],
            .prop = GSP_COMPONENT_PROP_VISIBLE,
            .value = {.type = GSP_VALUE_BOOL, .data.boolean = values[i]},
        };
    }
    return esp_gsp_component_set_many(ui, updates, 4U);
}

static bool works_state_is_active(works_runtime_state_t state)
{
    return state == WORKS_RUNTIME_QUEUED ||
           state == WORKS_RUNTIME_RUNNING ||
           state == WORKS_RUNTIME_STOPPING;
}

static const char *works_active_label(works_runtime_state_t state)
{
    switch (state) {
    case WORKS_RUNTIME_QUEUED:
        return "Starting";
    case WORKS_RUNTIME_STOPPING:
        return "Stopping";
    case WORKS_RUNTIME_RUNNING:
    default:
        return "Running";
    }
}

static void works_runtime_changed(uint32_t revision, void *user_ctx)
{
    (void)user_ctx;
    (void)mosaic_loader_invalidate_app(WORKS_APP_ID, revision);
}

static esp_err_t works_item_for_row(
    size_t row, works_runtime_item_snapshot_t *out_item)
{
    if (row >= WORKS_ROW_COUNT || !out_item) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_selected_tab == WORKS_TAB_RECENT) {
        return works_runtime_get_recent(row, out_item);
    }
    size_t wanted = s_installed_page * WORKS_ROW_COUNT + row;
    if (wanted < WORKS_NATIVE_COUNT) {
        memset(out_item, 0, sizeof(*out_item));
        snprintf(out_item->app_id, sizeof(out_item->app_id), "%s", s_native_ids[wanted]);
        snprintf(out_item->display_name, sizeof(out_item->display_name), "%s", s_native_names[wanted]);
        out_item->builtin = true;
        return ESP_OK;
    }
    wanted -= WORKS_NATIVE_COUNT;
    if (s_selected_tab == WORKS_TAB_LAB) {
        size_t count = 0;
        size_t matched = 0;
        esp_err_t err = works_runtime_get_count(&count);
        if (err != ESP_OK) {
            return err;
        }
        for (size_t index = 0; index < count; ++index) {
            err = works_runtime_get_item(index, out_item);
            if (err != ESP_OK) {
                return err;
            }
            if (works_is_lab_item(out_item) && matched++ == wanted) {
                return ESP_OK;
            }
        }
        return ESP_ERR_NOT_FOUND;
    }
    return works_runtime_get_item(wanted, out_item);
}

static bool works_is_native_row(size_t row)
{
    return s_selected_tab != WORKS_TAB_RECENT &&
           s_installed_page == 0 && row < WORKS_NATIVE_COUNT;
}

static void works_render(esp_gsp_handle_t ui)
{
    size_t item_count = 0;
    item_count = works_page_item_count();
    const size_t page_count =
        (item_count + WORKS_ROW_COUNT - 1U) / WORKS_ROW_COUNT;
    if (page_count == 0) {
        s_installed_page = 0;
    } else if (s_installed_page >= page_count) {
        s_installed_page = page_count - 1U;
    }

    char text[24];
    snprintf(text, sizeof(text), "%u ITEMS", (unsigned)item_count);
    (void)esp_gsp_set_text(ui, GSP_BIND_WORKS_COUNT, text);
    (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_0_SELECTED,
                              s_selected_tab == WORKS_TAB_RECENT);
    (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_1_SELECTED,
                              s_selected_tab == WORKS_TAB_INSTALLED);
    (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_2_SELECTED,
                              s_selected_tab == WORKS_TAB_LAB);
    (void)esp_gsp_set_visible(
        ui, GSP_BIND_WORKS_PAGER_VISIBLE,
        s_selected_tab != WORKS_TAB_RECENT && page_count > 1U);
    snprintf(text, sizeof(text), "%u / %u",
             (unsigned)(page_count ? s_installed_page + 1U : 0U),
             (unsigned)page_count);
    (void)esp_gsp_set_text(ui, GSP_BIND_WORKS_PAGE_TEXT, text);

    for (size_t row = 0; row < WORKS_ROW_COUNT; ++row) {
        works_runtime_item_snapshot_t item;
        bool visible = works_item_for_row(row, &item) == ESP_OK;
        bool native = visible && works_is_native_row(row);
        (void)esp_gsp_set_visible(ui, s_row_lua_visible_binds[row], visible && !native);
        if (row < WORKS_NATIVE_COUNT) (void)esp_gsp_set_visible(ui, s_native_visible_binds[row], native);
        if (!visible) {
            esp_err_t err = works_set_row_visibility(ui, row, false, false, false, false);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "hide work row %u failed: %s", (unsigned)row, esp_err_to_name(err));
            }
            continue;
        }
        bool active = works_state_is_active(item.state);
        esp_err_t err = esp_gsp_set_text(ui, s_row_name_binds[row], item.display_name);
        err = works_first_error(err, esp_gsp_set_text(ui, s_row_meta_binds[row], native ? "Liquid · Tilt to move" : (item.builtin ? "Lua · System" : "Lua · Local")));
        if (active) {
            err = works_first_error(err, esp_gsp_set_text(ui, s_row_active_text_binds[row], works_active_label(item.state)));
        }
        err = works_first_error(err, works_set_row_visibility(ui, row, true, active, !native && item.state == WORKS_RUNTIME_STOPPED, item.state == WORKS_RUNTIME_FAILED));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "render work row %u failed: %s", (unsigned)row, esp_err_to_name(err));
        }
    }
}

static void works_toggle_row(size_t row)
{
    works_runtime_item_snapshot_t item;
    esp_err_t err = works_item_for_row(row, &item);
    if (err == ESP_OK && !works_is_native_row(row)) {
        err = works_runtime_request_toggle(item.app_id);
    }
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "toggle work failed: %s", esp_err_to_name(err));
    }
}

static void works_dispatch_call(
    esp_gsp_handle_t ui, const mosaic_event_t *event)
{
    switch (event->data.call.action_id) {
    case GSP_ACT_ID_WORKS_PIXEL:
        (void)mosaic_liquid_select_style(LIQUID_PIXEL);
        break;
    case GSP_ACT_ID_WORKS_GRADIENT:
        (void)mosaic_liquid_select_style(LIQUID_GRADIENT);
        break;
    case GSP_ACT_ID_WORKS_WATER:
        (void)mosaic_liquid_select_style(LIQUID_WATER);
        break;
    case GSP_ACT_ID_WORKS_SELECT_TAB:
        if (event->data.call.arg <= WORKS_TAB_LAB) {
            s_selected_tab = (works_tab_t)event->data.call.arg;
            s_installed_page = 0;
            works_render(ui);
        }
        break;
    case GSP_ACT_ID_WORKS_TOGGLE:
        works_toggle_row(event->data.call.arg);
        works_render(ui);
        break;
    case GSP_ACT_ID_WORKS_PAGE_PREV:
        if (s_installed_page > 0) {
            --s_installed_page;
        }
        works_render(ui);
        break;
    case GSP_ACT_ID_WORKS_PAGE_NEXT: {
        size_t count = works_page_item_count();
        if ((s_installed_page + 1U) * WORKS_ROW_COUNT < count) {
            ++s_installed_page;
        }
        works_render(ui);
        break;
    }
    default:
        break;
    }
}

static void works_event(
    esp_gsp_handle_t ui, const struct mosaic_event *raw_event)
{
    const mosaic_event_t *event = raw_event;
    if (!event) {
        return;
    }
    switch (event->type) {
    case MOSAIC_EVENT_START: {
        works_clear_rows(ui);
        works_runtime_config_t config = {
            .on_changed = works_runtime_changed,
        };
        esp_err_t err = works_runtime_init(&config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "initialize works runtime failed: %s",
                     esp_err_to_name(err));
        }
        works_render(ui);
        break;
    }
    case MOSAIC_EVENT_UI_CALL:
        works_dispatch_call(ui, event);
        break;
    case MOSAIC_EVENT_TIMER:
        (void)works_runtime_refresh();
        (void)works_runtime_flush();
        break;
    case MOSAIC_EVENT_MODEL_CHANGED:
    case MOSAIC_EVENT_SCENE_CHANGED:
        works_render(ui);
        break;
    case MOSAIC_EVENT_STOP:
        (void)works_runtime_flush();
        break;
    case MOSAIC_EVENT_POINTER:
    default:
        break;
    }
}

#else

static void works_event(
    esp_gsp_handle_t ui, const struct mosaic_event *raw_event)
{
    const mosaic_event_t *event = raw_event;
    if (event && event->type == MOSAIC_EVENT_START) {
        (void)esp_gsp_set_text(ui, GSP_BIND_WORKS_COUNT, "0 ITEMS");
        (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_0_SELECTED, false);
        (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_1_SELECTED, false);
        (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_TAB_2_SELECTED, true);
        (void)esp_gsp_set_visible(ui, GSP_BIND_WORKS_PAGER_VISIBLE, false);
        works_clear_rows(ui);
    }
}

#endif

const mosaic_app_descriptor_t mosaic_works_app = {
    .id = WORKS_APP_ID,
    .launch_action = GSP_ACT_ID_APP_WORKS,
    .back_action = MOSAIC_APP_SHELL_BACK_ACTION,
    .routes = s_native_routes,
    .route_count = sizeof(s_native_routes) / sizeof(s_native_routes[0]),
    .name = "works",
    .title = "Works",
    .directory = &gsp_obj_directory_works,
    .disable_swipe = true,
    .on_event = works_event,
};
