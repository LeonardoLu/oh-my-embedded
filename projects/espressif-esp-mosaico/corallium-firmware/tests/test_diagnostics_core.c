/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "mosaico_diagnostics_core.h"
#include "diagnostics_tasks_fixture.h"

typedef struct {
    mosaico_diagnostics_status_t status;
    esp_err_t status_error;
    esp_err_t input_error;
    esp_err_t begin_error;
    esp_err_t read_error;
    unsigned status_calls;
    unsigned input_calls;
    unsigned open_calls;
    unsigned begins;
    unsigned reads;
    unsigned ends;
    uint32_t last_args[5];
    mosaico_diagnostics_frame_t frame;
    bool acquired;
} provider_t;

static esp_err_t get_status(void *ctx, mosaico_diagnostics_status_t *out)
{
    provider_t *p = ctx;
    ++p->status_calls;
    *out = p->status;
    return p->status_error;
}

static esp_err_t tap(void *ctx, uint16_t x, uint16_t y)
{
    provider_t *p = ctx;
    ++p->input_calls;
    p->last_args[0] = x;
    p->last_args[1] = y;
    return p->input_error;
}

static esp_err_t drag(void *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                       uint32_t duration)
{
    provider_t *p = ctx;
    ++p->input_calls;
    p->last_args[0] = x0;
    p->last_args[1] = y0;
    p->last_args[2] = x1;
    p->last_args[3] = y1;
    p->last_args[4] = duration;
    return p->input_error;
}

static esp_err_t back(void *ctx)
{
    provider_t *p = ctx;
    ++p->input_calls;
    return p->input_error;
}

static esp_err_t open_app(void *ctx, mosaico_diagnostics_app_t app)
{
    provider_t *p = ctx;
    assert(app >= MOSAICO_DIAGNOSTICS_APP_SETTINGS && app <= MOSAICO_DIAGNOSTICS_APP_WEATHER);
    ++p->open_calls;
    p->last_args[0] = (uint32_t)app;
    return p->input_error;
}

static esp_err_t begin(void *ctx, mosaico_diagnostics_frame_t *out, void **handle)
{
    provider_t *p = ctx;
    assert(!p->acquired);
    ++p->begins;
    *out = p->frame;
    *handle = p;
    p->acquired = true;
    return p->begin_error;
}

static esp_err_t read_frame(void *ctx, void *handle, uint32_t offset, uint8_t *dst, size_t len)
{
    provider_t *p = ctx;
    assert(handle == p && p->acquired);
    assert(len > 0 && len <= MOSAICO_DIAGNOSTICS_FRAME_CHUNK);
    assert(offset <= p->frame.size_bytes && len <= p->frame.size_bytes - offset);
    ++p->reads;
    if (p->read_error != ESP_OK) return p->read_error;
    for (size_t i = 0; i < len; ++i) dst[i] = (uint8_t)((offset + i) * 29u + 7u);
    return ESP_OK;
}

static void end(void *ctx, void *handle)
{
    provider_t *p = ctx;
    assert(handle == p && p->acquired);
    p->acquired = false;
    ++p->ends;
}

static void init(mosaico_diagnostics_core_t *core, provider_t *p, bool capture)
{
    memset(p, 0, sizeof(*p));
    p->status = (mosaico_diagnostics_status_t){
        .app_name = "Settings", .app_id = 3, .display_owner = MOSAICO_DIAGNOSTICS_OWNER_GSP,
        .width = 480, .height = 480, .started = true, .panel_enabled = true,
        .capture_supported = true, .brightness_percent = 80, .panel_brightness_percent = 15,
        .dim_timeout_ms = 10000, .sleep_timeout_ms = 30000,
    };
    p->frame = (mosaico_diagnostics_frame_t){
        .width = 8, .height = 2, .stride_bytes = 17, .size_bytes = 34,
    };
    mosaico_diagnostics_ops_t ops = {
        .ctx = p, .status = get_status, .tap = tap, .drag = drag, .back = back,
        .open_app = open_app,
        .capture_begin = capture ? begin : NULL,
        .capture_read = capture ? read_frame : NULL,
        .capture_end = capture ? end : NULL,
    };
    const uint8_t mac[] = {0x1c, 0x29, 0x04, 0xd0, 0x90, 0x36};
    assert(mosaico_diagnostics_core_init(core, &ops, mac));
    core->tasks_snapshot = mosaico_diagnostics_tasks_snapshot;
}

static void command(mosaico_diagnostics_core_t *core, const char *body, uint64_t now_ms)
{
    char line[256];
    int len = snprintf(line, sizeof(line), "@MOSAICO %s\n", body);
    assert(len > 0 && (size_t)len < sizeof(line));
    assert(mosaico_diagnostics_core_feed(core, (const uint8_t *)line, (size_t)len, now_ms) == (size_t)len);
}

static void packet(mosaico_diagnostics_core_t *core, char out[513], uint64_t now_ms)
{
    const mosaico_diagnostics_packet_t *pending = mosaico_diagnostics_core_packet(core);
    assert(pending && pending->len <= MOSAICO_DIAGNOSTICS_PACKET_BYTES);
    assert(pending->len >= 4u && memcmp(pending->bytes, "\r\n@MOSAICO ", 11u) == 0);
    assert(pending->bytes[pending->len - 2u] == '\r' && pending->bytes[pending->len - 1u] == '\n');
    memcpy(out, pending->bytes, pending->len);
    out[pending->len] = '\0';
    mosaico_diagnostics_core_sent(core, now_ms);
}

static void expect(mosaico_diagnostics_core_t *core, const char *text, uint64_t now_ms)
{
    char out[513];
    packet(core, out, now_ms);
    if (!strstr(out, text)) fprintf(stderr, "Expected %s; got %s", text, out);
    assert(strstr(out, text));
}

static void test_input_and_identity(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    const uint8_t first[] = "@MOSAI";
    const uint8_t second[] = "CO 4294967295 ping\r\n";
    assert(mosaico_diagnostics_core_feed(&core, first, sizeof(first) - 1u, 0) == sizeof(first) - 1u);
    assert(!mosaico_diagnostics_core_packet(&core));
    size_t consumed = mosaico_diagnostics_core_feed(&core, second, sizeof(second) - 1u, 0);
    assert(consumed == sizeof(second) - 2u);
    expect(&core, "4294967295 OK mac=1c:29:04:d0:90:36 protocol=1", 0);
    assert(mosaico_diagnostics_core_feed(&core, second + consumed, 1u, 0) == 1u);
    assert(p.input_calls == 0 && p.status_calls == 0 && p.begins == 0);

    command(&core, "2 tap 479 0", 100);
    expect(&core, "2 OK queued", 100);
    assert(p.input_calls == 1 && p.last_args[0] == 479 && p.last_args[1] == 0);
    command(&core, "3 drag 0 479 479 0 2000", 200);
    expect(&core, "3 OK queued", 200);
    assert(p.input_calls == 2 && p.last_args[1] == 479 && p.last_args[4] == 2000);
    command(&core, "4 back", 300);
    expect(&core, "4 OK queued", 300);
    assert(p.input_calls == 3);
    p.input_error = ESP_ERR_TIMEOUT;
    command(&core, "5 back", 400);
    expect(&core, "5 ERR ui 0x107", 400);
    p.input_error = ESP_ERR_NOT_SUPPORTED;
    command(&core, "6 tap 0 0", 500);
    expect(&core, "6 ERR unsupported 0x106", 500);

    unsigned calls = p.input_calls;
    const char *bad[] = {
        "7 tap -1 4", "8 tap 480 0", "9 tap 0 480", "10 tap 4.0 4", "11 tap +4 4",
        "12 drag 0 0 479 480 50", "13 drag 0 0 1 1 49", "14 drag 0 0 1 1 2001",
        "15 tap 1 1 extra", "16 tap 1", "17 back extra", "18 drag 0 0 1 1 4294967296",
        "19 configuration password=sensitive", "0 ping", "4294967296 ping", "-1 ping",
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        uint64_t now = 600u + i * 100u;
        command(&core, bad[i], now);
        char out[513];
        packet(&core, out, now);
        assert(strstr(out, " ERR ") && !strstr(out, "sensitive") && !strstr(out, "password"));
    }
    assert(p.input_calls == calls);
    command(&core, "20 tap 1 1", 2101);
    expect(&core, "20 ERR rate", 2101);
    assert(p.input_calls == calls);

    mosaico_diagnostics_core_disconnect(&core);
    const char *ignored = "password=sensitive\n";
    assert(mosaico_diagnostics_core_feed(&core, (const uint8_t *)ignored, strlen(ignored), 0) == strlen(ignored));
    assert(!mosaico_diagnostics_core_packet(&core) && p.input_calls == calls);
}

static void test_invalid_lines_and_pressure(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    char large[512];
    memcpy(large, "@MOSAICO 1 tap 1 1 ", 19u);
    memset(large + 19u, 'x', sizeof(large) - 20u);
    large[sizeof(large) - 1u] = '\n';
    assert(mosaico_diagnostics_core_feed(&core, (const uint8_t *)large, sizeof(large), 0) == sizeof(large));
    expect(&core, "0 ERR line", 0);
    assert(p.input_calls == 0);
    uint8_t nul[] = {'@','M','O','S','A','I','C','O',' ','2',' ','t','a','p',' ','1',0,' ','1','\n'};
    assert(mosaico_diagnostics_core_feed(&core, nul, sizeof(nul), 100) == sizeof(nul));
    expect(&core, "0 ERR line", 100);
    command(&core, "3 status", 200);
    assert(core.tx_count == MOSAICO_DIAGNOSTICS_TX_DEPTH);
    const uint8_t next[] = "@MOSAICO 4 tap 0 0\n";
    assert(mosaico_diagnostics_core_feed(&core, next, sizeof(next) - 1u, 300) == 0);
    assert(p.input_calls == 0);
    while (core.tx_count) expect(&core, "@MOSAICO 3", 200);
    assert(mosaico_diagnostics_core_feed(&core, next, sizeof(next) - 1u, 300) == sizeof(next) - 1u);
    expect(&core, "4 OK queued", 300);
    assert(p.input_calls == 1);

    /* Arbitrary hostile bytes cannot grow storage or dispatch UI operations. */
    uint32_t seed = 0x97f3beu;
    for (unsigned sample = 0; sample < 2000u; ++sample) {
        uint8_t data[257];
        for (size_t i = 0; i < sizeof(data); ++i) {
            seed = seed * 1664525u + 1013904223u;
            data[i] = (uint8_t)(seed >> 24);
        }
        size_t offset = 0;
        while (offset < sizeof(data)) {
            size_t n = mosaico_diagnostics_core_feed(&core, data + offset, sizeof(data) - offset, 500u + sample * 100u);
            offset += n;
            while (core.tx_count) expect(&core, "ERR", 500u + sample * 100u);
        }
        assert(core.line_len <= MOSAICO_DIAGNOSTICS_MAX_LINE && p.input_calls == 1);
    }
}

static void test_read_only_status(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    memset(p.status.app_name, 'z', sizeof(p.status.app_name));
    p.status.app_id = UINT32_MAX;
    p.status.display_owner = MOSAICO_DIAGNOSTICS_OWNER_EXCLUSIVE;
    p.status.width = p.status.height = p.status.rotation = UINT16_MAX;
    p.status.queue_depth = UINT16_MAX;
    p.status.render_frames = p.status.render_busy_us = UINT64_MAX;
    p.status.render_errors = p.status.runtime_errors = UINT32_MAX;
    p.status.last_render_error = p.status.last_runtime_error = p.status.last_input_error = INT32_MIN;
    p.status.dim_timeout_ms = p.status.sleep_timeout_ms = p.status.poweroff_timeout_ms = UINT32_MAX;
    p.status.volume_percent = p.status.brightness_percent = p.status.panel_brightness_percent = UINT8_MAX;
    command(&core, "4294967295 status", UINT64_MAX);
    assert(core.tx_count == 4 && p.status_calls == 1 && p.input_calls == 0 && p.begins == 0);
    expect(&core, "\"device\":{\"chip\":\"esp32s31\",\"mac\":\"1c:29:04:d0:90:36\"}", UINT64_MAX);
    expect(&core, "\"owner\":\"exclusive\"", UINT64_MAX);
    expect(&core, "\"last_runtime_error\":-2147483648", UINT64_MAX);
    expect(&core, "\"panel_brightness_percent\":255", UINT64_MAX);
    assert(!core.tx_count);

    mosaico_diagnostics_core_disconnect(&core);
    memcpy(p.status.app_name, "bad\"name", sizeof("bad\"name"));
    command(&core, "1 status", 0);
    expect(&core, "STATUS", 0);
    expect(&core, "\"app_name\":\"unknown\"", 0);
    expect(&core, "STATUS", 0);
    expect(&core, "OK", 0);
    p.status_error = ESP_ERR_TIMEOUT;
    command(&core, "2 status", 100);
    expect(&core, "2 ERR status 0x107", 100);
    assert(p.input_calls == 0 && p.begins == 0);
}

static void test_capture_lifecycle(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, false);
    command(&core, "1 capture", 0);
    expect(&core, "1 ERR unsupported 0x106", 0);
    assert(p.begins == 0);
    init(&core, &p, true);
    p.status.asleep = true;
    command(&core, "1 capture", 0);
    expect(&core, "1 ERR asleep", 0);
    assert(p.begins == 0 && p.input_calls == 0);
    p.status.asleep = false;
    p.status.display_owner = MOSAICO_DIAGNOSTICS_OWNER_EXCLUSIVE;
    command(&core, "2 capture", 100);
    expect(&core, "2 ERR owner", 100);
    assert(p.begins == 0);
    p.status.display_owner = MOSAICO_DIAGNOSTICS_OWNER_GSP;
    p.status.capture_supported = false;
    command(&core, "20 capture", 200);
    expect(&core, "20 ERR unsupported 0x106", 200);
    assert(p.begins == 0);
    mosaico_diagnostics_core_disconnect(&core);
    p.status.capture_supported = true;
    p.begin_error = ESP_ERR_NOT_SUPPORTED;
    command(&core, "3 capture", 200);
    expect(&core, "3 ERR unsupported 0x106", 200);
    assert(p.begins == 1 && p.ends == 1 && !p.acquired);
    p.begin_error = ESP_OK;
    p.frame.size_bytes = 35;
    command(&core, "4 capture", 300);
    expect(&core, "4 ERR frame 0x104", 300);
    assert(p.begins == 2 && p.ends == 2);
    p.frame.size_bytes = 34;
    command(&core, "5 capture", 400);
    expect(&core, "5 FRAME 8 2 17 RGB565LE 34", 400);
    assert(core.capture_active && p.acquired);
    command(&core, "6 capture", 500);
    expect(&core, "6 ERR busy", 500);
    command(&core, "7 abort", 600);
    expect(&core, "5 ERR cancelled", 600);
    expect(&core, "7 OK aborted", 600);
    assert(p.ends == 3 && !core.capture_active && !p.acquired);

    command(&core, "8 capture", 700);
    expect(&core, "FRAME", 700);
    p.read_error = ESP_ERR_TIMEOUT;
    mosaico_diagnostics_core_tick(&core, 701);
    expect(&core, "8 ERR frame 0x107", 701);
    assert(p.ends == 4 && !core.capture_active);
    p.read_error = ESP_OK;
    command(&core, "9 capture", 800);
    expect(&core, "FRAME", 800);
    mosaico_diagnostics_core_tick(&core, 801);
    expect(&core, "9 DATA 0 34", 801);
    mosaico_diagnostics_core_tick(&core, 802);
    expect(&core, "9 END 34", 802);
    assert(p.ends == 5 && !core.capture_active && p.input_calls == 0);

    command(&core, "10 capture", 900);
    mosaico_diagnostics_core_disconnect(&core);
    assert(p.ends == 6 && !core.capture_active && !core.tx_count && !p.acquired);
    command(&core, "11 capture", 1000);
    mosaico_diagnostics_core_tick(&core, 1000);
    mosaico_diagnostics_core_tick(&core, 3000);
    expect(&core, "11 ERR timeout", 3000);
    assert(p.ends == 7);
    command(&core, "12 capture", 3100);
    expect(&core, "FRAME", 3100);
    /* Other successful responses cannot bypass the total capture deadline. */
    mosaico_diagnostics_core_sent(&core, 32099);
    core.last_tx_progress_ms = 32099;
    mosaico_diagnostics_core_tick(&core, 33100);
    expect(&core, "12 ERR timeout", 33100);
    assert(p.ends == 8 && !p.acquired);
    mosaico_diagnostics_core_disconnect(&core);
    assert(p.ends == 8);
}

static void transcript(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    const mosaico_diagnostics_frame_t cases[] = {
        {.width = 480, .height = 480, .stride_bytes = 1024, .size_bytes = MOSAICO_DIAGNOSTICS_MAX_FRAME_BYTES},
        {.width = 8, .height = 2, .stride_bytes = 17, .size_bytes = 34},
        {.width = 8, .height = 2, .stride_bytes = 16, .size_bytes = 32},
        {.width = 5, .height = 3, .stride_bytes = 11, .size_bytes = 33},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        init(&core, &p, true);
        p.frame = cases[i];
        char body[32];
        snprintf(body, sizeof(body), "%u capture", 77u + i);
        command(&core, body, 100);
        unsigned count = 0;
        uint64_t now = 100;
        do {
            mosaico_diagnostics_core_tick(&core, now);
            const mosaico_diagnostics_packet_t *pending = mosaico_diagnostics_core_packet(&core);
            assert(pending && pending->len <= MOSAICO_DIAGNOSTICS_PACKET_BYTES);
            assert(fwrite(pending->bytes, 1u, pending->len, stdout) == pending->len);
            mosaico_diagnostics_core_sent(&core, now);
            now += 2u;
            ++count;
        } while (core.capture_active || core.tx_count);
        unsigned expected = 2u + (p.frame.size_bytes + 191u) / 192u;
        assert(count == expected && p.ends == 1 && !p.acquired && p.input_calls == 0);
    }
}

static void status_transcript(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    command(&core, "4294967295 status", UINT64_MAX);
    while (core.tx_count) {
        const mosaico_diagnostics_packet_t *pending = mosaico_diagnostics_core_packet(&core);
        assert(fwrite(pending->bytes, 1u, pending->len, stdout) == pending->len);
        mosaico_diagnostics_core_sent(&core, UINT64_MAX);
    }
}

static void test_open_whitelist(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    const char *names[] = {"settings", "works", "album", "weather"};
    for (unsigned i = 0; i < 4; ++i) {
        char body[64];
        snprintf(body, sizeof(body), "1 open %s", names[i]);
        command(&core, body, i * 100u);
        expect(&core, "1 OK admitted", i * 100u);
        assert(p.open_calls == i + 1 && p.last_args[0] == i);
    }
    const char *bad[] = {"2 open", "2 open Settings", "2 open settings extra",
        "2 open /apps/settings", "2 open scripts/game.lua", "2 open unknown"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        command(&core, bad[i], 400u + i * 100u);
        expect(&core, "2 ERR argument", 400u + i * 100u);
    }
    assert(p.open_calls == 4 && p.status_calls == 0 && p.input_calls == 0);
    p.input_error = ESP_ERR_TIMEOUT;
    command(&core, "3 open settings", 1000);
    expect(&core, "3 ERR ui 0x107", 1000);
    core.ops.open_app = NULL;
    command(&core, "4 open works", 1100);
    expect(&core, "4 ERR unsupported 0x106", 1100);
    command(&core, "5 capture", 1200);
    expect(&core, "5 FRAME", 1200);
    command(&core, "6 open album", 1300);
    expect(&core, "6 ERR busy", 1300);
    command(&core, "7 tasks", 1400);
    expect(&core, "7 ERR busy", 1400);
    mosaico_diagnostics_core_disconnect(&core);
    assert(!p.acquired && p.ends == 1);
}

static void drain_tasks(mosaico_diagnostics_core_t *core, uint64_t now_ms, bool emit)
{
    char out[513];
    unsigned lines = 0;
    while (core->tasks_active || core->tx_count) {
        if (!core->tx_count) mosaico_diagnostics_core_tick(core, now_ms + lines);
        packet(core, out, now_ms + lines);
        assert(!diagnostics_fixture_kernel_locked);
        if (emit) fputs(out, stdout);
        if (lines == 0) assert(strstr(out, " TASKS "));
        else if (lines <= MOSAICO_DIAGNOSTICS_LISTED_TASKS) {
            assert(strstr(out, " TASK ") && strstr(out, mosaico_diagnostics_task_name(lines - 1)));
        } else assert(strstr(out, " OK {\"tasks\":14}"));
        ++lines;
    }
    assert(lines == MOSAICO_DIAGNOSTICS_LISTED_TASKS + 2);
}

static void test_task_stream(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    diagnostics_tasks_fixture_reset();
    p.status.asleep = true;
    p.status_error = ESP_ERR_TIMEOUT; /* Tasks never takes the UI status path. */
    command(&core, "1 tasks", 0);
    drain_tasks(&core, 0, false);
    assert(diagnostics_fixture_snapshots == 1 && p.status_calls == 0 && p.input_calls == 0);
    command(&core, "2 tasks", 100);
    expect(&core, "2 ERR rate", 100);
    assert(diagnostics_fixture_snapshots == 1);
    command(&core, "3 tasks", 1000);
    expect(&core, "3 TASKS", 1000);
    command(&core, "4 back", 1100);
    expect(&core, "4 ERR busy", 1100);
    assert(p.input_calls == 0);
    command(&core, "5 abort", 1200);
    expect(&core, "3 ERR cancelled", 1200);
    expect(&core, "5 OK aborted", 1200);
    assert(!core.tasks_active);
    command(&core, "6 tasks", 2000);
    expect(&core, "6 TASKS", 2000);
    mosaico_diagnostics_core_tick(&core, 2001);
    assert(core.tx_count == 1);
    mosaico_diagnostics_core_tick(&core, 4000);
    expect(&core, "6 ERR timeout", 4000);
    assert(!core.tasks_active && !core.tx_count);
    command(&core, "7 tasks", 5000);
    mosaico_diagnostics_core_disconnect(&core);
    assert(!core.tasks_active && !core.tx_count);
    command(&core, "8 tasks", 5001);
    drain_tasks(&core, 5001, false);
    diagnostics_fixture_task_count = MOSAICO_DIAGNOSTICS_TASK_SLOTS + 1;
    command(&core, "9 tasks", 6100);
    expect(&core, "9 ERR tasks 0x104", 6100);
    diagnostics_fixture_task_count = 4;
    diagnostics_fixture_snapshot_count = 0;
    command(&core, "10 tasks", 7100);
    expect(&core, "10 ERR tasks 0x103", 7100);
    core.tasks_snapshot = NULL;
    command(&core, "11 tasks", 8100);
    expect(&core, "11 ERR tasks 0x106", 8100);
    assert(p.status_calls == 0 && p.input_calls == 0 && p.open_calls == 0 && p.begins == 0);
    init(&core, &p, true);
    diagnostics_tasks_fixture_reset();
    command(&core, "12 tasks", 0);
    expect(&core, "12 TASKS", 0);
    for (uint64_t now = 1000; now < 5000; now += 1000) {
        mosaico_diagnostics_core_tick(&core, now);
        expect(&core, "12 TASK", now); /* Progress avoids 2 s stall; total limit still applies. */
    }
    mosaico_diagnostics_core_tick(&core, 5000);
    expect(&core, "12 ERR timeout", 5000);
    assert(!core.tasks_active);
    init(&core, &p, true);
    command(&core, "13 tasks", UINT64_MAX);
    expect(&core, "13 TASKS", UINT64_MAX);
    mosaico_diagnostics_core_tick(&core, 0);
    expect(&core, "13 ERR timeout", 0);
    assert(!core.tasks_active);
}

static void tasks_transcript(void)
{
    mosaico_diagnostics_core_t core;
    provider_t p;
    init(&core, &p, true);
    diagnostics_tasks_fixture_reset();
    command(&core, "81 tasks", 9000);
    drain_tasks(&core, 9000, true);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--transcript") == 0) {
        transcript();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--status-transcript") == 0) {
        status_transcript();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--tasks-transcript") == 0) {
        tasks_transcript();
        return 0;
    }
    assert(~mosaico_diagnostics_crc32_update(UINT32_MAX, (const uint8_t *)"123456789", 9u) == 0xcbf43926u);
    test_input_and_identity();
    test_invalid_lines_and_pressure();
    test_read_only_status();
    test_capture_lifecycle();
    test_open_whitelist();
    test_task_stream();
    puts("USB diagnostic core: parsing, readonly status, bounds and capture lifecycle passed");
    return 0;
}
