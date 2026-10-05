/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "diagnostics_tasks_fixture.h"
#include "mosaico_diagnostics_tasks.h"

bool diagnostics_fixture_kernel_locked;
unsigned diagnostics_fixture_snapshots;
unsigned diagnostics_fixture_lookups;
UBaseType_t diagnostics_fixture_task_count = 4;
UBaseType_t diagnostics_fixture_snapshot_count = 4;
bool diagnostics_fixture_gsp_missing;
eTaskState diagnostics_fixture_gsp_state = eBlocked;
static unsigned s_handle[4];

void diagnostics_tasks_fixture_reset(void)
{
    diagnostics_fixture_kernel_locked = false;
    diagnostics_fixture_snapshots = diagnostics_fixture_lookups = 0;
    diagnostics_fixture_task_count = diagnostics_fixture_snapshot_count = 4;
    diagnostics_fixture_gsp_missing = false;
    diagnostics_fixture_gsp_state = eBlocked;
}

UBaseType_t uxTaskGetNumberOfTasks(void)
{
    assert(!diagnostics_fixture_kernel_locked);
    return diagnostics_fixture_task_count;
}

UBaseType_t uxTaskGetSystemState(TaskStatus_t *out, UBaseType_t count,
                                configRUN_TIME_COUNTER_TYPE *total)
{
    assert(!diagnostics_fixture_kernel_locked && count == MOSAICO_DIAGNOSTICS_TASK_SLOTS);
    ++diagnostics_fixture_snapshots;
    diagnostics_fixture_kernel_locked = true;
    /* Deliberately unusable name/stack pointers: a live snapshot must not read them. */
    for (unsigned i = 0; i < 4; ++i) {
        out[i] = (TaskStatus_t){
            .xHandle = &s_handle[i], .pcTaskName = (const char *)(uintptr_t)1,
            .pxStackBase = (StackType_t *)(uintptr_t)1,
            .xTaskNumber = 17u + i, .eCurrentState = i == 0 ? diagnostics_fixture_gsp_state : eReady,
            .uxCurrentPriority = i == 0 ? 5 : 2, .uxBasePriority = i == 0 ? 4 : 2,
            .ulRunTimeCounter = (configRUN_TIME_COUNTER_TYPE)~(configRUN_TIME_COUNTER_TYPE)0 - i,
            .usStackHighWaterMark = 768u + i, .xCoreID = i == 1 ? 1 : 0x7fffffff,
        };
    }
    *total = (configRUN_TIME_COUNTER_TYPE)~(configRUN_TIME_COUNTER_TYPE)0;
    diagnostics_fixture_kernel_locked = false;
    return diagnostics_fixture_snapshot_count;
}

TaskHandle_t xTaskGetHandle(const char *name)
{
    assert(!diagnostics_fixture_kernel_locked);
    bool listed = false;
    for (unsigned i = 0; i < MOSAICO_DIAGNOSTICS_LISTED_TASKS; ++i) {
        if (strcmp(name, mosaico_diagnostics_task_name(i)) == 0) listed = true;
    }
    assert(listed); /* No task name or configuration input comes from the host. */
    ++diagnostics_fixture_lookups;
    if (strcmp(name, "esp_gsp") == 0) return diagnostics_fixture_gsp_missing ? NULL : &s_handle[0];
    if (strcmp(name, "mosaic_runtime") == 0) return &s_handle[1];
    if (strcmp(name, "usb_diagnostics") == 0) return &s_handle[2];
    return NULL;
}
