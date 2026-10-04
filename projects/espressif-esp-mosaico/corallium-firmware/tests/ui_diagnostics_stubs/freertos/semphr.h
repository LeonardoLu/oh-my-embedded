/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "task.h"
typedef struct test_semaphore *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t lock, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t lock);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t lock);
void vSemaphoreDelete(SemaphoreHandle_t lock);
