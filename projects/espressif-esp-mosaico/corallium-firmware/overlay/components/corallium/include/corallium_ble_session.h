// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

/* Saved admission policy and per-connection facts are independent. This
 * device uses ordinary GATT; CCCD subscription is required for exchanges. */
typedef struct {
    _Atomic bool enabled, connected, notify_requested;
    _Atomic uint32_t generation, application_generation;
} corallium_ble_session_t;

static inline void corallium_ble_reset_connection(corallium_ble_session_t *s) {
    s->connected = false;
    s->notify_requested = false;
    s->application_generation = 0;
    ++s->generation;
}
static inline int corallium_ble_set_enabled(corallium_ble_session_t *s, bool enabled,
                                           int (*persist)(bool)) {
    int error = persist(enabled);
    if (error) return error;
    s->enabled = enabled;
    if (!enabled) {
        s->notify_requested = false;
        s->application_generation = 0;
        ++s->generation;
    }
    return 0;
}
static inline bool corallium_ble_accept_connection(corallium_ble_session_t *s) {
    if (!s->enabled || s->connected) return false;
    corallium_ble_reset_connection(s);
    s->connected = true;
    return true;
}
static inline bool corallium_ble_can_exchange(const corallium_ble_session_t *s) {
    return s->enabled && s->connected && s->notify_requested;
}
static inline bool corallium_ble_can_publish(const corallium_ble_session_t *s) {
    return corallium_ble_can_exchange(s) && s->application_generation != 0 &&
           s->application_generation == s->generation;
}
