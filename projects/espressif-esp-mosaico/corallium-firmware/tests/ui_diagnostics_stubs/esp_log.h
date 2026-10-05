/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
void mosaic_test_log_warning(const char *format, ...);
#define ESP_LOGW(tag, ...) ((void)(tag), mosaic_test_log_warning(__VA_ARGS__))
