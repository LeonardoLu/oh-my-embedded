/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "freertos/FreeRTOS.h"
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef enum { eRunning, eReady, eBlocked, eSuspended, eDeleted, eInvalid } eTaskState;
typedef struct {
    TaskHandle_t xHandle;
    const char *pcTaskName;
    UBaseType_t xTaskNumber;
    eTaskState eCurrentState;
    UBaseType_t uxCurrentPriority;
    UBaseType_t uxBasePriority;
    configRUN_TIME_COUNTER_TYPE ulRunTimeCounter;
    StackType_t *pxStackBase;
    configSTACK_DEPTH_TYPE usStackHighWaterMark;
    BaseType_t xCoreID;
} TaskStatus_t;
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetSystemState(TaskStatus_t *out, UBaseType_t count,
                                configRUN_TIME_COUNTER_TYPE *total);
TaskHandle_t xTaskGetHandle(const char *name);
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                       unsigned priority, TaskHandle_t *handle);
void vTaskDelay(TickType_t ticks);
