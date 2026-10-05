/* The native bridge omits these APIs. Navigation and common chrome are outside
 * this regression; scene geometry, List recycling and image decoding are real. */
#include "esp_gsp.h"
#include "mosaic_app_shell.h"
#include "mosaic_top_notice.h"

static uint16_t s_page;

esp_gsp_err_t esp_gsp_stack_view_push(esp_gsp_handle_t ui,
    gsp_component_key_t key, uint16_t page, bool animated)
{
    (void)ui; (void)key; (void)animated; s_page = page;
    return ESP_GSP_OK;
}

esp_gsp_err_t esp_gsp_stack_view_pop(esp_gsp_handle_t ui,
    gsp_component_key_t key, bool animated)
{
    (void)ui; (void)key; (void)animated; s_page = 0;
    return ESP_GSP_OK;
}

esp_gsp_err_t esp_gsp_stack_view_get_top(esp_gsp_handle_t ui,
    gsp_component_key_t key, uint16_t *out)
{
    (void)ui; (void)key; *out = s_page;
    return ESP_GSP_OK;
}

esp_gsp_err_t esp_gsp_stack_view_is_animating(esp_gsp_handle_t ui,
    gsp_component_key_t key, bool *out)
{
    (void)ui; (void)key; *out = false;
    return ESP_GSP_OK;
}

esp_gsp_err_t esp_gsp_keyboard_text(esp_gsp_handle_t ui, char *out, size_t size)
{
    (void)ui; if (size) out[0] = 0;
    return ESP_GSP_OK;
}

void mosaic_app_shell_attach(esp_gsp_handle_t ui, uint32_t key,
    const char *title, bool enabled)
{ (void)ui; (void)key; (void)title; (void)enabled; }
void mosaic_app_shell_sync(esp_gsp_handle_t ui) { (void)ui; }
void mosaic_app_shell_set_root_visible(esp_gsp_handle_t ui, bool visible)
{ (void)ui; (void)visible; }
void mosaic_app_shell_set_bottom_enabled(esp_gsp_handle_t ui, bool enabled)
{ (void)ui; (void)enabled; }
void mosaic_app_shell_detach(esp_gsp_handle_t ui) { (void)ui; }

esp_gsp_err_t mosaic_top_notice_show(esp_gsp_handle_t ui,
    const mosaic_top_notice_config_t *cfg, const char *title,
    const char *message, uint32_t timeout)
{
    (void)ui; (void)cfg; (void)title; (void)message; (void)timeout;
    return ESP_GSP_OK;
}
void mosaic_top_notice_hide(esp_gsp_handle_t ui) { (void)ui; }
void mosaic_top_notice_detach(esp_gsp_handle_t ui) { (void)ui; }
