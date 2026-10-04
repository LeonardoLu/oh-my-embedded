// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Bound recursive parser stack use and reject noncanonical UTF-8 / embedded NUL.
 * cJSON remains responsible for the complete JSON grammar. */
static inline bool corallium_json_safe(const char *text) {
    const uint8_t *p = (const uint8_t *)text;
    unsigned depth = 0;
    bool quoted = false;
    while (*p) {
        uint8_t c = *p++;
        if (c >= 0x80) {
            unsigned extra;
            uint32_t value, minimum;
            if (c >= 0xc2 && c <= 0xdf) { extra = 1; value = c & 31; minimum = 0x80; }
            else if (c >= 0xe0 && c <= 0xef) { extra = 2; value = c & 15; minimum = 0x800; }
            else if (c >= 0xf0 && c <= 0xf4) { extra = 3; value = c & 7; minimum = 0x10000; }
            else return false;
            while (extra--) {
                if ((*p & 0xc0) != 0x80) return false;
                value = (value << 6) | (*p++ & 63);
            }
            if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
        } else if (quoted) {
            if (c == '"') quoted = false;
            else if (c == '\\') {
                if (!*p) return false;
                if (*p == 'u' && p[1] && p[2] && p[3] && p[4] &&
                    p[1] == '0' && p[2] == '0' && p[3] == '0' && p[4] == '0') return false;
                ++p;
            } else if (c < 0x20) return false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 16) return false; }
        else if (c == '}' || c == ']') { if (!depth) return false; --depth; }
    }
    return !quoted && !depth;
}
