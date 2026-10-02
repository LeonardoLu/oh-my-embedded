// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "corallium_framing.h"
#include "corallium_metrics.h"
#include "corallium_render_policy.h"
#include "corallium_json_guard.h"
#include "corallium_ble_session.h"
static bool saved_enabled;
static int save_error;
static int save_ble(bool enabled) {
    if (save_error) return save_error;
    saved_enabled = enabled;
    return 0;
}
static void test_ble_lifecycle(void) {
    corallium_ble_session_t ble = {0};
    assert(!ble.enabled && !corallium_ble_accept_connection(&ble));
    save_error = 7;
    assert(corallium_ble_set_enabled(&ble, true, save_ble) == 7);
    assert(!ble.enabled && !saved_enabled); // Failed NVS save must not report on.
    save_error = 0;
    assert(corallium_ble_set_enabled(&ble, true, save_ble) == 0);
    assert(saved_enabled && corallium_ble_accept_connection(&ble));
    assert(!corallium_ble_accept_connection(&ble)); // One active peer only.
    uint32_t first = ble.generation;
    assert(!corallium_ble_can_exchange(&ble));
    ble.notify_requested = true; // Plain CCCD write; no authentication event exists.
    assert(corallium_ble_can_exchange(&ble) && !corallium_ble_can_publish(&ble));
    ble.application_generation = first;
    assert(corallium_ble_can_publish(&ble));
    ble.notify_requested = false;
    assert(!corallium_ble_can_exchange(&ble) && !corallium_ble_can_publish(&ble));
    ble.notify_requested = true;
    save_error = 8;
    assert(corallium_ble_set_enabled(&ble, false, save_ble) == 8);
    assert(saved_enabled && ble.enabled && corallium_ble_can_exchange(&ble));
    save_error = 0;
    corallium_ble_reset_connection(&ble);
    assert(ble.enabled && !corallium_ble_can_exchange(&ble));
    assert(corallium_ble_accept_connection(&ble)); // No deadline or physical re-enable.
    assert(!corallium_ble_can_exchange(&ble)); // A prior subscription is not restored.
    ble.notify_requested = true;
    ble.application_generation = first; // Late completion from old connection.
    assert(corallium_ble_can_exchange(&ble) && !corallium_ble_can_publish(&ble));
    ble.application_generation = ble.generation;
    assert(corallium_ble_can_publish(&ble));
    corallium_ble_session_t rebooted = {0};
    rebooted.enabled = saved_enabled;
    assert(rebooted.enabled && corallium_ble_accept_connection(&rebooted));
    assert(!corallium_ble_can_publish(&rebooted)); // Saved on does not restore a session.
    assert(corallium_ble_set_enabled(&ble, false, save_ble) == 0);
    assert(!ble.enabled && !saved_enabled && !corallium_ble_can_exchange(&ble));
    ble.notify_requested = true; // A late subscription cannot reopen a disabled switch.
    assert(!corallium_ble_can_exchange(&ble) && !corallium_ble_can_publish(&ble));
    corallium_ble_reset_connection(&ble);
    assert(!corallium_ble_accept_connection(&ble));
    corallium_ble_session_t before_stack_init = {0};
    assert(corallium_ble_set_enabled(&before_stack_init, false, save_ble) == 0);
    assert(!saved_enabled); // Factory reset works without initializing Bluetooth.
}
static unsigned received;
static size_t received_length;
static char received_line[2049];
static bool line(const char *text, size_t size, void *context) {
    (void)context; ++received; received_length = size;
    memcpy(received_line, text, size + 1); return true;
}
int main(void) {
    test_ble_lifecycle();
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
    puts("BLE lifecycle/persistence, framing, UTF-8, overflow recovery, telemetry and render policy passed");
}
