/* SPDX-License-Identifier: Apache-2.0 */
#include "mosaico_diagnostics_tasks.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *const s_names[MOSAICO_DIAGNOSTICS_LISTED_TASKS] = {
    "esp_gsp", "gsp_decode", "mosaic_runtime", "screen_power", "works_runtime",
    "usb_diagnostics", "esp_timer", "Tmr Svc", "IDLE0", "IDLE1", "wifi_manager",
    "weather", "main", "sys_evt",
};

const char *mosaico_diagnostics_task_name(unsigned index)
{
    return index < MOSAICO_DIAGNOSTICS_LISTED_TASKS ? s_names[index] : NULL;
}

#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS && INCLUDE_xTaskGetHandle

/* Avoid placing all SDK task records on the 6 KiB USB task stack. */
static TaskStatus_t s_status[MOSAICO_DIAGNOSTICS_TASK_SLOTS];

static mosaico_diagnostics_task_state_t task_state(eTaskState state)
{
    switch (state) {
    case eRunning: return MOSAICO_DIAGNOSTICS_TASK_RUNNING;
    case eReady: return MOSAICO_DIAGNOSTICS_TASK_READY;
    case eBlocked: return MOSAICO_DIAGNOSTICS_TASK_BLOCKED;
    case eSuspended: return MOSAICO_DIAGNOSTICS_TASK_SUSPENDED;
    case eDeleted: return MOSAICO_DIAGNOSTICS_TASK_DELETED;
    default: return MOSAICO_DIAGNOSTICS_TASK_INVALID;
    }
}

esp_err_t mosaico_diagnostics_tasks_snapshot(mosaico_diagnostics_tasks_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    for (unsigned i = 0; i < MOSAICO_DIAGNOSTICS_LISTED_TASKS; ++i) {
        out->entries[i].affinity = -1;
    }
    if (uxTaskGetNumberOfTasks() > MOSAICO_DIAGNOSTICS_TASK_SLOTS) {
        return ESP_ERR_INVALID_SIZE;
    }
    configRUN_TIME_COUNTER_TYPE total = 0;
    UBaseType_t count = uxTaskGetSystemState(s_status, MOSAICO_DIAGNOSTICS_TASK_SLOTS, &total);
    if (count == 0u || count > MOSAICO_DIAGNOSTICS_TASK_SLOTS) {
        return ESP_ERR_INVALID_STATE;
    }
    out->total_tasks = (uint16_t)count;
    out->counter_bits = (uint8_t)(sizeof(total) * 8u);
    out->counter_total_us = total;
    for (unsigned i = 0; i < MOSAICO_DIAGNOSTICS_LISTED_TASKS; ++i) {
        /*
         * The SDK snapshot's name/stack pointers can expire after its kernel
         * lock is released. Only compare opaque handle values with a fresh
         * public name lookup; never dereference a captured name, TCB or stack.
         * A task deleted between the two samples is simply absent.
         */
        TaskHandle_t handle = xTaskGetHandle(s_names[i]);
        if (!handle) continue;
        for (UBaseType_t n = 0; n < count; ++n) {
            const TaskStatus_t *status = &s_status[n];
            if (status->xHandle != handle) continue;
            uint64_t stack_bytes = (uint64_t)status->usStackHighWaterMark * sizeof(StackType_t);
            if (stack_bytes > UINT32_MAX) return ESP_ERR_INVALID_SIZE;
            mosaico_diagnostics_task_t *entry = &out->entries[i];
            entry->present = true;
            entry->number = (uint32_t)status->xTaskNumber;
            entry->state = task_state(status->eCurrentState);
            entry->priority = (uint32_t)status->uxCurrentPriority;
            entry->base_priority = (uint32_t)status->uxBasePriority;
            entry->stack_min_bytes = (uint32_t)stack_bytes;
            entry->runtime_us = status->ulRunTimeCounter;
#if configTASKLIST_INCLUDE_COREID == 1
            if (status->xCoreID == 0 || status->xCoreID == 1) {
                entry->affinity = (int8_t)status->xCoreID;
            }
#endif
            break;
        }
    }
    return ESP_OK;
}

#else

esp_err_t mosaico_diagnostics_tasks_snapshot(mosaico_diagnostics_tasks_t *out)
{
    (void)out;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
