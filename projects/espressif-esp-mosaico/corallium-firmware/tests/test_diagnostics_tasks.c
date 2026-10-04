/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diagnostics_tasks_fixture.h"
#include "mosaico_diagnostics_tasks.h"

int main(void)
{
    mosaico_diagnostics_tasks_t snapshot;
    diagnostics_tasks_fixture_reset();
    assert(mosaico_diagnostics_tasks_snapshot(NULL) == ESP_ERR_INVALID_ARG);
    assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_OK);
    assert(snapshot.total_tasks == 4 && snapshot.counter_bits == sizeof(configRUN_TIME_COUNTER_TYPE) * 8u);
    assert(snapshot.counter_total_us == (configRUN_TIME_COUNTER_TYPE)~(configRUN_TIME_COUNTER_TYPE)0);
    assert(diagnostics_fixture_snapshots == 1 && diagnostics_fixture_lookups == MOSAICO_DIAGNOSTICS_LISTED_TASKS);
    assert(!diagnostics_fixture_kernel_locked);
    const mosaico_diagnostics_task_t *gsp = &snapshot.entries[0];
    assert(gsp->present && gsp->number == 17 && gsp->state == MOSAICO_DIAGNOSTICS_TASK_BLOCKED);
    assert(gsp->priority == 5 && gsp->base_priority == 4 && gsp->stack_min_bytes == 768);
    assert(gsp->runtime_us == (configRUN_TIME_COUNTER_TYPE)~(configRUN_TIME_COUNTER_TYPE)0 && gsp->affinity == -1);
    assert(snapshot.entries[2].present && snapshot.entries[2].affinity == 1);
    assert(!snapshot.entries[1].present && snapshot.entries[1].affinity == -1);
    assert(mosaico_diagnostics_task_name(MOSAICO_DIAGNOSTICS_LISTED_TASKS) == NULL);
    for (unsigned i = 0; i < MOSAICO_DIAGNOSTICS_LISTED_TASKS; ++i) assert(strlen(mosaico_diagnostics_task_name(i)) < 16);
    const mosaico_diagnostics_task_state_t states[] = {
        MOSAICO_DIAGNOSTICS_TASK_RUNNING, MOSAICO_DIAGNOSTICS_TASK_READY,
        MOSAICO_DIAGNOSTICS_TASK_BLOCKED, MOSAICO_DIAGNOSTICS_TASK_SUSPENDED,
        MOSAICO_DIAGNOSTICS_TASK_DELETED, MOSAICO_DIAGNOSTICS_TASK_INVALID,
    };
    for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        diagnostics_fixture_gsp_state = (eTaskState)i;
        assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_OK);
        assert(snapshot.entries[0].state == states[i]);
    }
    diagnostics_fixture_gsp_missing = true; /* Deleted after snapshot: no captured pointer dereference. */
    assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_OK);
    assert(!snapshot.entries[0].present && snapshot.entries[0].state == MOSAICO_DIAGNOSTICS_TASK_MISSING);
    diagnostics_fixture_task_count = MOSAICO_DIAGNOSTICS_TASK_SLOTS + 1;
    unsigned calls = diagnostics_fixture_snapshots;
    assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_ERR_INVALID_SIZE);
    assert(diagnostics_fixture_snapshots == calls);
    diagnostics_fixture_task_count = 4;
    diagnostics_fixture_snapshot_count = 0; /* Grew beyond fixed capacity inside SDK. */
    assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_ERR_INVALID_STATE);
    diagnostics_fixture_snapshot_count = MOSAICO_DIAGNOSTICS_TASK_SLOTS + 1;
    assert(mosaico_diagnostics_tasks_snapshot(&snapshot) == ESP_ERR_INVALID_STATE);
    puts("Task statistics: public snapshot, fixed names, expired pointers, counter widths and capacity passed");
    return 0;
}
