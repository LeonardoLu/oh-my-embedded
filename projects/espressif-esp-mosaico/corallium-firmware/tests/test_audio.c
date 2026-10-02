// SPDX-License-Identifier: MIT
/* Exercise the real output task with an explicit non-hardware codec/RTOS fake. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "audio_test_platform.h"
#include "mosaico_audio.h"

static jmp_buf stopped;
static void (*worker)(void *);
static int pending, opened, written, closed, volume, samples;
static bool muted, active, fail_open;
static dev_audio_codec_handles_t device = {.codec_dev = &device};
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &device; }
int xSemaphoreTake(SemaphoreHandle_t h, uint32_t t) { (void)h; (void)t; return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t h) { (void)h; return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t h) { (void)h; }
int xTaskCreate(void (*fn)(void *), const char *n, unsigned s, void *a, unsigned p, TaskHandle_t *out) {
    (void)n; (void)s; (void)a; (void)p; worker=fn; *out=&device; return pdPASS;
}
uint32_t ulTaskNotifyTake(int clear, uint32_t wait) {
    (void)clear; (void)wait;
    if (!pending) longjmp(stopped, 1);
    pending=0; return 1;
}
void xTaskNotifyGive(TaskHandle_t h) { assert(h); pending=1; }
int esp_codec_dev_open(esp_codec_dev_handle_t h, esp_codec_dev_sample_info_t *f) {
    assert(h && !active && f->sample_rate==16000 && f->channel==1 && f->bits_per_sample==16);
    active=true; opened++; return fail_open ? ESP_CODEC_DEV_DRV_ERR : ESP_CODEC_DEV_OK;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t h, int v) { assert(h && active); volume=v; return 0; }
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t h, bool m) { assert(h && active); muted=m; return 0; }
int esp_codec_dev_write(esp_codec_dev_handle_t h, void *buffer, int bytes) {
    assert(h && active && !muted && volume>0 && bytes==320);
    int16_t *pcm=buffer;
    for (int i=0;i<bytes/2;i++) { assert(pcm[i]>=-4096 && pcm[i]<=4096); if(pcm[i]) samples++; }
    written++; return 0;
}
int esp_codec_dev_close(esp_codec_dev_handle_t h) { assert(h && active && (muted || fail_open)); active=false; closed++; return 0; }
int esp_codec_set_disable_when_closed(esp_codec_dev_handle_t h, bool b) { assert(h && b); return 0; }
esp_err_t esp_board_manager_init_device_by_name(const char *n) { assert(!strcmp(n,"audio_dac")); return 0; }
esp_err_t esp_board_manager_get_device_handle(const char *n, void **out) { assert(!strcmp(n,"audio_dac")); *out=&device; return 0; }
static void drain(void) { if (!setjmp(stopped)) worker(NULL); }
int main(void) {
    assert(!mosaico_audio_available());
    assert(mosaico_audio_set_volume(50)==ESP_ERR_INVALID_STATE);
    assert(mosaico_audio_init()==ESP_OK && mosaico_audio_available());
    assert(opened==0 && pending==0 && written==0); /* Boot never plays audio. */
    assert(mosaico_audio_set_volume(80)==ESP_OK && mosaico_audio_get_volume()==80);
    assert(opened==0 && pending==0); /* Restoring a preference is silent. */
    assert(mosaico_audio_set_volume(101)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_preview()==ESP_OK); drain();
    assert(opened==1 && closed==1 && written==10 && samples>0 && !active && volume==80);
    assert(mosaico_audio_set_volume(0)==ESP_OK);
    assert(mosaico_audio_preview()==ESP_OK); drain();
    assert(opened==1 && written==10 && mosaico_audio_get_volume()==0); /* Muted: no output. */
    assert(mosaico_audio_set_volume(35)==ESP_OK);
    assert(mosaico_audio_preview()==ESP_OK); drain();
    assert(opened==2 && closed==2 && written==20 && volume==35 && !active);
    fail_open=true; /* Vendor can fail after enabling its I2S output. */
    assert(mosaico_audio_preview()==ESP_OK); drain();
    assert(opened==3 && closed==3 && written==20 && !active);
    fail_open=false;
    assert(mosaico_audio_preview()==ESP_OK); drain();
    assert(opened==4 && closed==4 && written==30 && !active);
    puts("actual speaker worker: silent startup/restore, bounded local PCM, mute, idle close and failed-open cleanup passed");
}
