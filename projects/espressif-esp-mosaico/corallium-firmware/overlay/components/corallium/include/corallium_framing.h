// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define CORALLIUM_LINE_LIMIT 2048

typedef struct { char bytes[CORALLIUM_LINE_LIMIT + 1]; size_t count; bool dropping; int64_t last_us; } corallium_stream_t;
typedef bool (*corallium_line_cb)(const char *line, size_t length, void *context);
static inline void corallium_stream_reset(corallium_stream_t *stream) { memset(stream, 0, sizeof(*stream)); }
static inline bool corallium_stream_feed(corallium_stream_t *s, const uint8_t *bytes, size_t size, int64_t now_us, corallium_line_cb callback, void *context) {
    if (s->last_us && now_us - s->last_us > 10000000) {
        corallium_stream_reset(s); s->dropping = true;
    }
    s->last_us = now_us;
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] == '\n') {
            bool ok = true;
            if (!s->dropping && s->count) ok = callback(s->bytes, s->count, context);
            corallium_stream_reset(s);
            if (!ok) return false;
        } else if (!s->dropping) {
            if (!bytes[i] || s->count == CORALLIUM_LINE_LIMIT) {
                corallium_stream_reset(s); s->dropping = true;
            } else s->bytes[s->count++] = (char)bytes[i];
        }
    }
    if (s->count || s->dropping) s->last_us = now_us;
    return true;
}
