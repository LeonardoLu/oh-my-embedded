// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x104
#define ESP_ERR_NOT_FINISHED 0x10c
#define ESP_CODEC_DEV_OK 0
#define ESP_CODEC_DEV_DRV_ERR -1
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(n) (n)
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef void *esp_codec_dev_handle_t;
typedef void *i2s_chan_handle_t;
typedef struct { bool is_enabled; uint32_t total_dma_buf_size; } i2s_chan_info_t;
typedef struct { int sample_rate, channel, bits_per_sample; } esp_codec_dev_sample_info_t;
typedef struct { esp_codec_dev_handle_t codec_dev; } dev_audio_codec_handles_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t, uint32_t);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
int xTaskCreate(void (*fn)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
uint32_t ulTaskNotifyTake(int, uint32_t);
void xTaskNotifyGive(TaskHandle_t);
int esp_codec_dev_open(esp_codec_dev_handle_t, esp_codec_dev_sample_info_t *);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t, int);
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t, bool);
int esp_codec_dev_write(esp_codec_dev_handle_t, void *, int);
int esp_codec_dev_close(esp_codec_dev_handle_t);
int esp_codec_set_disable_when_closed(esp_codec_dev_handle_t, bool);
esp_err_t esp_board_manager_init_device_by_name(const char *);
esp_err_t esp_board_manager_get_device_handle(const char *, void **);
esp_err_t esp_board_manager_get_periph_handle(const char *, void **);
esp_err_t i2s_channel_get_info(i2s_chan_handle_t, i2s_chan_info_t *);
