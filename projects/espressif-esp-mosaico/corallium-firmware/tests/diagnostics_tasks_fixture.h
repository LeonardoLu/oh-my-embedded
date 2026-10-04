/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include "freertos/task.h"

extern bool diagnostics_fixture_kernel_locked;
extern unsigned diagnostics_fixture_snapshots;
extern unsigned diagnostics_fixture_lookups;
extern UBaseType_t diagnostics_fixture_task_count;
extern UBaseType_t diagnostics_fixture_snapshot_count;
extern bool diagnostics_fixture_gsp_missing;
extern eTaskState diagnostics_fixture_gsp_state;
void diagnostics_tasks_fixture_reset(void);
