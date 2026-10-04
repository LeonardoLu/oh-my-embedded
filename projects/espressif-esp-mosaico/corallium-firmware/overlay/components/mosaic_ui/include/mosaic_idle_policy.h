/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t dim_timeout_ms;
    uint32_t screen_timeout_ms;
    uint32_t poweroff_timeout_ms;
    bool dim_while_charging;
    bool sleep_while_charging;
} mosaic_idle_power_policy_t;

typedef struct {
    bool dimmed;
    bool asleep;
    bool poweroff_due;
    /** Remaining time until the next stage; 0 means no pending stage. */
    uint32_t next_timeout_ms;
} mosaic_idle_power_decision_t;

static inline void mosaic_idle_policy_next_deadline(
    uint32_t timeout_ms, uint64_t idle_ms, uint32_t *next_ms)
{
    if (timeout_ms == 0U || idle_ms >= timeout_ms) {
        return;
    }
    const uint32_t remaining = timeout_ms - (uint32_t)idle_ms;
    if (*next_ms == 0U || remaining < *next_ms) {
        *next_ms = remaining;
    }
}

/** All stages use the same inactivity origin, including while the panel is off.
 * A foreign presenter must return its renderer before these actions apply. */
static inline mosaic_idle_power_decision_t mosaic_idle_policy_evaluate(
    const mosaic_idle_power_policy_t *policy, uint64_t idle_ms,
    bool battery_available, bool charging, bool presenter_active)
{
    mosaic_idle_power_decision_t result = {0};
    if (policy == NULL || !presenter_active) {
        return result;
    }
    const bool allow_dim = !charging || policy->dim_while_charging;
    const bool allow_sleep = !charging || policy->sleep_while_charging;
    /* An unavailable gauge never permits unattended whole-device shutdown. */
    const bool allow_poweroff = battery_available && !charging;
    if (allow_dim && policy->dim_timeout_ms != 0U) {
        result.dimmed = idle_ms >= policy->dim_timeout_ms;
        mosaic_idle_policy_next_deadline(
            policy->dim_timeout_ms, idle_ms, &result.next_timeout_ms);
    }
    if (allow_sleep && policy->screen_timeout_ms != 0U) {
        result.asleep = idle_ms >= policy->screen_timeout_ms;
        mosaic_idle_policy_next_deadline(
            policy->screen_timeout_ms, idle_ms, &result.next_timeout_ms);
    }
    if (allow_poweroff && policy->poweroff_timeout_ms != 0U) {
        result.poweroff_due = idle_ms >= policy->poweroff_timeout_ms;
        mosaic_idle_policy_next_deadline(
            policy->poweroff_timeout_ms, idle_ms, &result.next_timeout_ms);
    }
    return result;
}
