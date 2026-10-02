// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "corallium_framing.h"
#include "corallium_metrics.h"
#include "corallium_render_policy.h"
#include "corallium_json_guard.h"
static unsigned received;
static size_t received_length;
static char received_line[2049];
static bool line(const char *text, size_t size, void *context) {
    (void)context; ++received; received_length = size;
    memcpy(received_line, text, size + 1); return true;
}
int main(void) {
    assert(corallium_json_safe("{\"ssid\":\"珊瑚\"}"));
    assert(corallium_json_safe("{\"text\":\"\\\\u0000\"}"));
    assert(!corallium_json_safe("{\"text\":\"\\u0000\"}"));
    assert(!corallium_json_safe("{\"text\":\"\xc0\x80\"}"));
    assert(!corallium_json_safe("\"\xed\xa0\x80\""));
    assert(!corallium_json_safe("\"\xf4\x90\x80\x80\""));
    assert(!corallium_json_safe("\"\xf0"));
    assert(corallium_json_safe("[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]"));
    assert(!corallium_json_safe("[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]"));
    corallium_stream_t stream = {0};
    const char *utf8 = "{\"ssid\":\"珊瑚\"}\n";
    for (size_t i = 0; i < strlen(utf8); ++i)
        assert(corallium_stream_feed(&stream, (const uint8_t *)utf8 + i, 1, 100 + i, line, NULL));
    assert(received == 1 && received_length == strlen(utf8) - 1);
    assert(!strncmp(received_line, utf8, received_length));
    unsigned char frame[2050]; memset(frame, 'x', sizeof(frame)); frame[2048] = '\n';
    corallium_stream_feed(&stream, frame, 2049, 200, line, NULL);
    assert(received == 2 && received_length == 2048);
    frame[2048] = 'x'; frame[2049] = '\n';
    corallium_stream_feed(&stream, frame, 2050, 300, line, NULL);
    assert(received == 2);
    const uint8_t nul[] = {'a',0,'b','\n','o','k','\n'};
    corallium_stream_feed(&stream, nul, sizeof(nul), 400, line, NULL);
    assert(received == 3 && !strcmp(received_line, "ok"));
    corallium_stream_feed(&stream, (const uint8_t *)"old", 3, 500, line, NULL);
    corallium_stream_feed(&stream, (const uint8_t *)"tail\nnew\n", 9, 11000500, line, NULL);
    assert(received == 4 && !strcmp(received_line, "new"));
    corallium_stream_reset(&stream);
    for (size_t i=0; i<sizeof(stream.bytes); ++i) assert(stream.bytes[i] == 0);
    assert(corallium_cell_power_mw(3700, -20) == 74);
    assert(corallium_cell_power_mw(4000, 10) == -40);
    assert(corallium_runtime_available(false, -20, 100));
    assert(!corallium_runtime_available(true, -20, 100));
    assert(!corallium_runtime_available(false, -1, 100));
    assert(!corallium_runtime_available(false, -20, UINT16_MAX));
    assert(corallium_dispatch_interval_ms(true, false, 2000000) == 50);
    assert(corallium_dispatch_interval_ms(true, false, 1999999) == 16);
    assert(corallium_dispatch_interval_ms(false, false, 6000000) == 16);
    assert(corallium_dispatch_interval_ms(true, true, 6000000) == 100);
    puts("framing, UTF-8, overflow recovery, telemetry and render policy passed");
}
