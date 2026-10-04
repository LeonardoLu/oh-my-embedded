/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void vTaskDelay(TickType_t ticks);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xTaskCreate(void (*task)(void *), const char *name, uint32_t stack,
    void *arg, UBaseType_t priority, TaskHandle_t *out);
