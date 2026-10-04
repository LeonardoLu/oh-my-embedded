/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include "diagnostics_tasks_fixture.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mosaico_diagnostics_core.h"
#include "freertos/task.h"

static bool s_usb_initialized;
static bool s_cdc_initialized;
static bool s_connected;
static esp_err_t s_mac_error;
static unsigned s_factory_mac_calls;
static unsigned s_legacy_mac_calls;
static BaseType_t s_create_result;
static TaskFunction_t s_created_task;
static uint64_t s_clock_ms;
static unsigned s_available;
static size_t s_short_write;
static uint8_t s_input[1024];
static size_t s_input_len;
static size_t s_input_pos;
static uint8_t s_output[32768];
static size_t s_output_len;
static unsigned s_queue_calls;
static unsigned s_read_calls;
static unsigned s_flush_calls;
static unsigned s_read_flush_calls;
static unsigned s_locks;
static unsigned s_unlocks;
static unsigned s_libc_global_depth;
static unsigned s_status_calls;
static unsigned s_input_calls;
static unsigned s_begin_calls;
static unsigned s_end_calls;
static bool s_frame_acquired;
static uint64_t s_begin_cost_ms;
static uint64_t s_stop_ms;
static unsigned s_phase;
static void (*s_script)(void);
static jmp_buf s_task_done;

static int __attribute__((unused)) test_try_lock(FILE *file)
{
    assert(file == stdout || file == stderr);
#if MOSAICO_DIAGNOSTICS_PICOLIBC_NEGATIVE
    /* Actual linked Picolibc ignores FILE and uses blocking global acquire. */
    ++s_locks;
    ++s_libc_global_depth;
    return 0;
#else
    assert(!"Raw diagnostics must never call libc FILE locking");
    return 1;
#endif
}

static void __attribute__((unused)) test_unlock(FILE *file)
{
    assert(file == stdout || file == stderr);
#if MOSAICO_DIAGNOSTICS_PICOLIBC_NEGATIVE
    /* FILE->_lock is independent; this does not balance the global acquire. */
    ++s_unlocks;
    assert(s_unlocks <= s_locks);
#else
    assert(!"Raw diagnostics must never call libc FILE unlocking");
#endif
}

static ssize_t test_read(int fd, void *dst, size_t len)
{
    assert(fd == STDIN_FILENO && len <= 64u);
    ++s_read_calls;
    size_t remaining = s_input_len - s_input_pos;
    if (!remaining) {
        errno = EWOULDBLOCK;
        return -1;
    }
    size_t n = remaining < len ? remaining : len;
    memcpy(dst, s_input + s_input_pos, n);
    s_input_pos += n;
    return (ssize_t)n;
}

/* Compile the actual transport/task, replacing only host boundary operations. */
#define ftrylockfile test_try_lock
#define funlockfile test_unlock
#define read test_read
#include "mosaico_diagnostics.c"
#undef ftrylockfile
#undef funlockfile
#undef read

bool bsp_usb_console_is_initialized(void) { return s_usb_initialized; }
bool tinyusb_cdcacm_initialized(int itf) { assert(itf == 0); return s_cdc_initialized; }
bool tud_cdc_n_connected(uint8_t itf) { assert(itf == 0); return s_connected; }
uint32_t tud_cdc_n_write_available(uint8_t itf) { assert(itf == 0); return s_available; }
int64_t esp_timer_get_time(void) { return (int64_t)(s_clock_ms * 1000u); }

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type)
{
    assert(type == ESP_MAC_EFUSE_FACTORY);
    ++s_factory_mac_calls;
    if (s_mac_error != ESP_OK) return s_mac_error;
    const uint8_t fixed[6] = {0x1c, 0x29, 0x04, 0xd0, 0x90, 0x36};
    memcpy(mac, fixed, sizeof(fixed));
    return ESP_OK;
}

esp_err_t esp_efuse_mac_get_default(uint8_t *mac)
{
    ++s_legacy_mac_calls;
    if (s_mac_error != ESP_OK) return s_mac_error;
    /* Real S31 IEEE802154 behavior, including the formerly missed 8-byte write. */
    const uint8_t eui64[8] = {0x1c, 0x29, 0x04, 0xff, 0xfe, 0xd0, 0x90, 0x36};
    memcpy(mac, eui64, sizeof(eui64));
    return ESP_OK;
}

void tud_cdc_n_read_flush(uint8_t itf)
{
    assert(itf == 0);
    ++s_read_flush_calls;
    s_input_len = s_input_pos = 0;
}

size_t tinyusb_cdcacm_write_queue(int itf, const uint8_t *data, size_t len)
{
    assert(!diagnostics_fixture_kernel_locked);
    assert(itf == 0 && len <= 512u);
#if MOSAICO_DIAGNOSTICS_PICOLIBC_NEGATIVE
    assert(s_locks > s_unlocks);
#else
    assert(s_locks == 0 && s_unlocks == 0 && s_libc_global_depth == 0);
#endif
    ++s_queue_calls;
    if (len > s_available) len = s_available;
    if (s_short_write && len > s_short_write) len = s_short_write;
    assert(s_output_len + len < sizeof(s_output));
    memcpy(s_output + s_output_len, data, len);
    s_output_len += len;
    s_output[s_output_len] = '\0';
    s_available -= (unsigned)len;
    return len;
}

esp_err_t tinyusb_cdcacm_write_flush(int itf, uint32_t timeout_ticks)
{
    assert(itf == 0 && timeout_ticks == 0);
    ++s_flush_calls;
    return ESP_OK;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                       unsigned priority, TaskHandle_t *handle)
{
    assert(fn && strcmp(name, "usb_diagnostics") == 0 && stack == 6144u && !arg && priority == 2u);
    s_created_task = fn;
    if (s_create_result == pdPASS) *handle = (void *)(uintptr_t)1u;
    return s_create_result;
}

void vTaskDelay(TickType_t ticks)
{
    assert(!diagnostics_fixture_kernel_locked);
    assert(ticks > 0u && ticks <= 20u);
    assert(s_locks == 0 && s_unlocks == 0 && s_libc_global_depth == 0);
    s_clock_ms += ticks;
    s_available = 512u;
    if (s_script) s_script();
    if (s_clock_ms >= s_stop_ms) longjmp(s_task_done, 1);
}

static void input(const char *line)
{
    size_t len = strlen(line);
    assert(s_input_len + len < sizeof(s_input));
    memcpy(s_input + s_input_len, line, len);
    s_input_len += len;
}

static esp_err_t status(void *ctx, mosaico_diagnostics_status_t *out)
{
    assert(!ctx);
    ++s_status_calls;
    *out = (mosaico_diagnostics_status_t){
        .app_name = "Settings", .app_id = 3, .display_owner = MOSAICO_DIAGNOSTICS_OWNER_GSP,
        .width = 480, .height = 480, .started = true, .panel_enabled = true,
        .capture_supported = true, .brightness_percent = 80, .panel_brightness_percent = 15,
    };
    return ESP_OK;
}

static esp_err_t tap(void *ctx, uint16_t x, uint16_t y)
{
    assert(!ctx && x == 0 && y == 479);
    ++s_input_calls;
    return ESP_OK;
}

static esp_err_t drag(void *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t duration)
{
    assert(!ctx && x0 == 0 && y0 == 479 && x1 == 479 && y1 == 0 && duration == 50);
    ++s_input_calls;
    return ESP_OK;
}

static esp_err_t back(void *ctx)
{
    assert(!ctx);
    ++s_input_calls;
    return ESP_OK;
}

static esp_err_t begin(void *ctx, mosaico_diagnostics_frame_t *out, void **handle)
{
    assert(!ctx && !s_frame_acquired);
    ++s_begin_calls;
    s_frame_acquired = true;
    s_clock_ms += s_begin_cost_ms;
    *out = (mosaico_diagnostics_frame_t){
        .width = 480, .height = 480, .stride_bytes = 960, .size_bytes = 480u * 960u,
    };
    *handle = (void *)(uintptr_t)1u;
    return ESP_OK;
}

static esp_err_t read_frame(void *ctx, void *handle, uint32_t offset, uint8_t *dst, size_t len)
{
    assert(!ctx && handle == (void *)(uintptr_t)1u && s_frame_acquired);
    assert(len <= 192u && offset + len <= 480u * 960u);
    for (size_t i = 0; i < len; ++i) dst[i] = (uint8_t)(offset + i);
    return ESP_OK;
}

static void end(void *ctx, void *handle)
{
    assert(!ctx && handle == (void *)(uintptr_t)1u && s_frame_acquired);
    s_frame_acquired = false;
    ++s_end_calls;
}

static mosaico_diagnostics_ops_t ops(void)
{
    return (mosaico_diagnostics_ops_t){
        .status = status, .tap = tap, .drag = drag, .back = back,
        .capture_begin = begin, .capture_read = read_frame, .capture_end = end,
    };
}

static void reset(void)
{
    diagnostics_tasks_fixture_reset();
    mosaico_diagnostics_core_disconnect(&s_core);
    s_task = NULL;
    s_usb_initialized = s_cdc_initialized = s_connected = true;
    s_mac_error = ESP_OK;
    s_factory_mac_calls = s_legacy_mac_calls = 0;
    s_create_result = pdPASS;
    s_created_task = NULL;
    s_clock_ms = 0;
    s_available = 512;
    s_short_write = 0;
    s_input_len = s_input_pos = s_output_len = 0;
    s_queue_calls = s_read_calls = s_flush_calls = s_read_flush_calls = 0;
    s_locks = s_unlocks = 0;
    s_libc_global_depth = 0;
    s_status_calls = s_input_calls = s_begin_calls = s_end_calls = 0;
    s_frame_acquired = false;
    s_begin_cost_ms = 0;
    s_phase = 0;
    s_script = NULL;
    s_stop_ms = 1000;
    memset(s_output, 0, sizeof(s_output));
}

static void run_task(void)
{
    mosaico_diagnostics_ops_t callbacks = ops();
    assert(mosaico_diagnostics_start(&callbacks) == ESP_OK);
    assert(s_factory_mac_calls == 1u && s_legacy_mac_calls == 0u);
    if (setjmp(s_task_done) == 0) s_created_task(NULL);
    assert(s_locks == 0 && s_unlocks == 0 && s_libc_global_depth == 0);
}

static void test_start_and_nonblocking_packet(void)
{
    reset();
    mosaico_diagnostics_ops_t callbacks = ops();
    s_usb_initialized = false;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_ERR_INVALID_STATE);
    s_usb_initialized = true;
    s_cdc_initialized = false;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_ERR_INVALID_STATE);
    s_cdc_initialized = true;
    s_mac_error = ESP_FAIL;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_FAIL);
    assert(s_factory_mac_calls == 1u && s_legacy_mac_calls == 0u && !s_created_task);
    s_mac_error = ESP_OK;
    assert(mosaico_diagnostics_start(NULL) == ESP_ERR_INVALID_ARG);
    callbacks.capture_read = NULL;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_ERR_INVALID_ARG);
    callbacks = ops();
    s_create_result = 0;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_ERR_NO_MEM);
    s_create_result = pdPASS;
    assert(mosaico_diagnostics_start(&callbacks) == ESP_OK);
    assert(s_factory_mac_calls == 5u && s_legacy_mac_calls == 0u);
    assert(mosaico_diagnostics_start(&callbacks) == ESP_ERR_INVALID_STATE);

    const char text[] = "\r\n@MOSAICO 1 OK protocol=1\r\n";
    mosaico_diagnostics_packet_t packet = {.len = sizeof(text) - 1u};
    memcpy(packet.bytes, text, packet.len);
    s_available = 10;
    assert(send_packet(&packet, 0) == 0 && s_queue_calls == 0 && s_flush_calls == 1);
    s_available = 512;
    s_short_write = 7;
    assert(send_packet(&packet, 0) == 7);
    s_short_write = 0;
    assert(send_packet(&packet, 7) == packet.len - 7u);
    assert(s_output_len == packet.len && memcmp(s_output, text, packet.len) == 0);
    assert(s_locks == 0 && s_unlocks == 0 && s_libc_global_depth == 0);
    assert(send_packet(&packet, packet.len) == 0);
}

static void input_script(void)
{
    static const char *commands[] = {
        "@MOSAICO 2 status\n", "@MOSAICO 3 tap 0 479\n",
        "@MOSAICO 4 drag 0 479 479 0 50\n", "@MOSAICO 5 back\n",
        "@MOSAICO 6 status\n", "arbitrary input is discarded\n",
    };
    if (s_phase < sizeof(commands) / sizeof(commands[0]) &&
        s_clock_ms >= (s_phase + 1u) * 120u) input(commands[s_phase++]);
}

static void test_task_input_and_status(void)
{
    reset();
    input("@MOSAICO 1 ping\n");
    s_script = input_script;
    run_task();
    assert(s_status_calls == 2 && s_input_calls == 3 && s_begin_calls == 0);
    assert(strstr((char *)s_output, "1 OK mac=1c:29:04:d0:90:36 protocol=1"));
    assert(strstr((char *)s_output, "\"brightness\":80,\"panel_brightness_percent\":15"));
    assert(strstr((char *)s_output, "5 OK queued") && !strstr((char *)s_output, "arbitrary"));
    assert(s_read_calls > 0 && s_read_flush_calls == 0);
}

static void abort_script(void)
{
    if (s_phase == 0u && s_clock_ms >= 120u) {
        input("@MOSAICO 2 capture\n");
        ++s_phase;
    } else if (s_phase == 1u && s_clock_ms >= 240u) {
        input("@MOSAICO 3 abort\n");
        ++s_phase;
    }
}

static void test_short_writes_and_abort(void)
{
    reset();
    s_short_write = 7;
    input("@MOSAICO 1 ping\n");
    s_script = abort_script;
    run_task();
    assert(s_begin_calls == 1 && s_end_calls == 1 && !s_frame_acquired);
    assert(strstr((char *)s_output, "2 FRAME 480 480 960 RGB565LE 460800"));
    assert(strstr((char *)s_output, "2 DATA 0 192"));
    assert(strstr((char *)s_output, "2 ERR cancelled") && strstr((char *)s_output, "3 OK aborted"));
    assert(!strstr((char *)s_output, " ERR timeout") && s_input_calls == 0);
}

static void disconnect_script(void)
{
    if (s_phase == 0u && s_clock_ms >= 100u) {
        input("@MOSAICO 2 tap 0");
        s_connected = false;
        ++s_phase;
    } else if (s_phase == 1u && s_clock_ms >= 300u) {
        s_connected = true;
        input("@MOSAICO 3 ping\n");
        ++s_phase;
    }
}

static void test_disconnect_and_slow_ui_copy(void)
{
    reset();
    input("@MOSAICO 1 capture\n");
    s_script = disconnect_script;
    run_task();
    assert(s_begin_calls == 1 && s_end_calls == 1 && !s_frame_acquired && s_input_calls == 0);
    assert(s_read_flush_calls > 0 && strstr((char *)s_output, "3 OK mac="));
    assert(!strstr((char *)s_output, "1 END"));

    reset();
    input("@MOSAICO 1 capture\n");
    s_begin_cost_ms = 3000;
    s_stop_ms = 3100;
    run_task();
    assert(s_frame_acquired && strstr((char *)s_output, "1 DATA"));
    assert(!strstr((char *)s_output, "ERR timeout"));
    mosaico_diagnostics_core_disconnect(&s_core);
    assert(s_end_calls == 1 && !s_frame_acquired);
}

static void stalled_script(void)
{
    s_available = 0;
}

static void test_stalled_host(void)
{
    reset();
    input("@MOSAICO 1 capture\n");
    s_available = 0;
    s_script = stalled_script;
    s_stop_ms = 2200;
    run_task();
    assert(s_begin_calls == 1 && s_end_calls == 1 && !s_frame_acquired && s_input_calls == 0);
    assert(s_output_len == 0 && s_core.tx_count <= MOSAICO_DIAGNOSTICS_TX_DEPTH);

    reset();
    s_connected = false;
    input("@MOSAICO 1 tap 0 479\n");
    run_task();
    assert(s_read_calls == 0 && s_input_calls == 0 && s_status_calls == 0 && s_read_flush_calls > 0);
}

static void tasks_abort_script(void)
{
    if (s_phase == 0u && s_clock_ms >= 120u) {
        input("@MOSAICO 2 abort\n");
        ++s_phase;
    }
}

static void test_task_statistics_transport(void)
{
    reset();
    input("@MOSAICO 1 tasks\n");
    run_task();
    assert(diagnostics_fixture_snapshots == 1 && !diagnostics_fixture_kernel_locked);
    assert(s_status_calls == 0 && s_input_calls == 0 && s_begin_calls == 0);
    assert(strstr((char *)s_output, "1 TASKS {\"snapshot_ms\":0"));
    assert(strstr((char *)s_output, "\"name\":\"esp_gsp\",\"present\":true"));
    assert(strstr((char *)s_output, "1 OK {\"tasks\":14}"));
    assert(!s_core.tasks_active && !s_core.tx_count);

    reset();
    s_short_write = 7;
    input("@MOSAICO 1 tasks\n");
    s_script = tasks_abort_script;
    run_task();
    assert(strstr((char *)s_output, "1 TASKS") && strstr((char *)s_output, "1 ERR cancelled"));
    assert(strstr((char *)s_output, "2 OK aborted") && !strstr((char *)s_output, "1 OK {\"tasks\""));
    assert(!s_core.tasks_active && s_status_calls == 0 && s_input_calls == 0);

    reset();
    s_short_write = 7;
    input("@MOSAICO 1 tasks\n");
    s_script = disconnect_script;
    run_task();
    assert(s_read_flush_calls > 0 && strstr((char *)s_output, "3 OK mac="));
    assert(!strstr((char *)s_output, "1 OK {\"tasks\"") && !s_core.tasks_active);
    assert(s_status_calls == 0 && s_input_calls == 0);

    reset();
    input("@MOSAICO 1 tasks\n");
    s_available = 0;
    s_script = stalled_script;
    s_stop_ms = 2200;
    run_task();
    assert(!s_core.tasks_active && s_output_len == 0 && s_status_calls == 0);
    assert(s_core.tx_count <= MOSAICO_DIAGNOSTICS_TX_DEPTH);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--picolibc-negative") == 0) {
        reset();
        const char text[] = "\r\n@MOSAICO 1 OK fixture\r\n";
        mosaico_diagnostics_packet_t packet = {.len = sizeof(text) - 1u};
        memcpy(packet.bytes, text, packet.len);
        for (unsigned i = 0; i < 3; ++i) {
            s_available = 512;
            assert(send_packet(&packet, 0) == packet.len);
        }
        assert(s_locks == 6 && s_unlocks == 6 && s_libc_global_depth == 6);
        assert(s_libc_global_depth == 0 && "PICOLIBC_GLOBAL_LOCK_LEAK");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--legacy-negative") == 0) {
        reset();
        mosaico_diagnostics_ops_t callbacks = ops();
        (void)mosaico_diagnostics_start(&callbacks);
        assert(!"Expected the old eight-byte API to fail ASan before returning");
    }
    test_start_and_nonblocking_packet();
    test_task_input_and_status();
    test_short_writes_and_abort();
    test_disconnect_and_slow_ui_copy();
    test_stalled_host();
    test_task_statistics_transport();
    puts("USB diagnostic task: CDC gating, queued input, short writes, abort and bounded cleanup passed");
    return 0;
}
