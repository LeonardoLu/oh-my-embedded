/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "mosaico_diagnostics.h"

#define MOSAICO_DIAGNOSTICS_TASK_SLOTS 64u
#define MOSAICO_DIAGNOSTICS_LISTED_TASKS 14u

typedef enum {
    MOSAICO_DIAGNOSTICS_TASK_MISSING,
    MOSAICO_DIAGNOSTICS_TASK_RUNNING,
    MOSAICO_DIAGNOSTICS_TASK_READY,
    MOSAICO_DIAGNOSTICS_TASK_BLOCKED,
    MOSAICO_DIAGNOSTICS_TASK_SUSPENDED,
    MOSAICO_DIAGNOSTICS_TASK_DELETED,
    MOSAICO_DIAGNOSTICS_TASK_INVALID,
} mosaico_diagnostics_task_state_t;

typedef struct {
    bool present;
    uint32_t number;
    mosaico_diagnostics_task_state_t state;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t stack_min_bytes;
    uint64_t runtime_us;
    int8_t affinity;
} mosaico_diagnostics_task_t;

typedef struct {
    uint16_t total_tasks;
    uint8_t counter_bits;
    uint64_t counter_total_us;
    mosaico_diagnostics_task_t entries[MOSAICO_DIAGNOSTICS_LISTED_TASKS];
} mosaico_diagnostics_tasks_t;

const char *mosaico_diagnostics_task_name(unsigned index);
/* Called only by the USB service task; never formats or writes USB in kernel locks. */
esp_err_t mosaico_diagnostics_tasks_snapshot(mosaico_diagnostics_tasks_t *out);
