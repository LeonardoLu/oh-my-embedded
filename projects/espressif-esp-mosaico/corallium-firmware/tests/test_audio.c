// SPDX-License-Identifier: MIT
/* Real worker, fake RTOS/codec and a delayed DMA ring: write is not playback. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "audio_test_platform.h"
#include "mosaico_audio.h"

static jmp_buf stopped;
static void (*worker)(void *);
static int pending, opened, written, closed, volume;
static int attempts, tone_writes, tail_writes, submitted, played, dropped;
static int fail_write, mute_after_write;
static int close_after_write, enqueue_after_write, preview_after_write;
static int pcm_amplitude = -1;
static int max_pcm_amplitude = 4096;
static uint32_t stream_under_test;
static bool request_close, request_enqueue, request_preview, try_lock_busy, forbid_lock;
static uint8_t next_pcm[3200];
static bool muted, active, fail_open, fail_info, disabled_info, request_mute;
static uint32_t dma_bytes = 2880;
static bool dma_nonzero[4096];
static size_t dma_position;
static dev_audio_codec_handles_t device = {.codec_dev = &device};
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &device; }
int xSemaphoreTake(SemaphoreHandle_t h, uint32_t t) {
    (void)h; assert(!forbid_lock);
    return t == 0 && try_lock_busy ? 0 : pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t h) {
    (void)h;
    if (request_mute) { /* A local mute action after the worker releases its lock. */
        request_mute=false;
        assert(mosaico_audio_set_volume(0)==ESP_OK);
    }
    if (request_close) {
        request_close=false;
        forbid_lock=true;
        mosaico_audio_stream_close(stream_under_test);
        forbid_lock=false;
    }
    if (request_enqueue) {
        request_enqueue=false;
        assert(mosaico_audio_stream_write(stream_under_test, next_pcm, sizeof(next_pcm))==ESP_OK);
        assert(mosaico_audio_stream_write(stream_under_test, next_pcm, sizeof(next_pcm))==ESP_ERR_NOT_FINISHED);
    }
    if (request_preview) {
        request_preview=false;
        pcm_amplitude=-1;
        assert(mosaico_audio_preview()==ESP_OK);
    }
    return pdTRUE;
}
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
    assert(dma_bytes/2 <= sizeof(dma_nonzero)/sizeof(dma_nonzero[0]));
    attempts=tone_writes=tail_writes=submitted=played=dropped=0;
    dma_position=0;
    memset(dma_nonzero, 0, sizeof(dma_nonzero));
    active=true; opened++; return fail_open ? ESP_CODEC_DEV_DRV_ERR : ESP_CODEC_DEV_OK;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t h, int v) { assert(h && active); volume=v; return 0; }
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t h, bool m) { assert(h && active); muted=m; return 0; }
int esp_codec_dev_write(esp_codec_dev_handle_t h, void *buffer, int bytes) {
    assert(h && active && !muted && volume>0 && bytes==320 && dma_bytes>0);
    if (++attempts==fail_write) return ESP_CODEC_DEV_DRV_ERR;
    int16_t *pcm=buffer;
    bool tone=false;
    /* Delay every sample by the complete DMA capacity. Only subsequent writes
     * advance playback; closing/muting cannot make queued samples audible. */
    for (int i=0;i<bytes/2;i++) {
        assert(pcm[i]>=-max_pcm_amplitude && pcm[i]<=max_pcm_amplitude);
        if (pcm_amplitude>=0 && pcm[i]) assert(pcm[i]==pcm_amplitude || pcm[i]==-pcm_amplitude);
        if (dma_nonzero[dma_position]) played++;
        dma_nonzero[dma_position]=pcm[i]!=0;
        if (pcm[i]) { submitted++; tone=true; }
        dma_position=(dma_position+1)%(dma_bytes/2);
    }
    if (tone) tone_writes++; else tail_writes++;
    written++;
    if (attempts==mute_after_write) request_mute=true;
    if (attempts==close_after_write) request_close=true;
    if (attempts==enqueue_after_write) { request_enqueue=true; enqueue_after_write=0; }
    if (attempts==preview_after_write) { request_preview=true; preview_after_write=0; }
    return 0;
}
int esp_codec_dev_close(esp_codec_dev_handle_t h) {
    assert(h && active && (muted || fail_open));
    for (size_t i=0;i<dma_bytes/2;i++) if (dma_nonzero[i]) dropped++;
    active=false; closed++; return 0;
}
int esp_codec_set_disable_when_closed(esp_codec_dev_handle_t h, bool b) { assert(h && b); return 0; }
esp_err_t esp_board_manager_init_device_by_name(const char *n) { assert(!strcmp(n,"audio_dac")); return 0; }
esp_err_t esp_board_manager_get_device_handle(const char *n, void **out) { assert(!strcmp(n,"audio_dac")); *out=&device; return 0; }
esp_err_t esp_board_manager_get_periph_handle(const char *n, void **out) { assert(!strcmp(n,"i2s_audio_out")); *out=&dma_position; return 0; }
esp_err_t i2s_channel_get_info(i2s_chan_handle_t h, i2s_chan_info_t *info) {
    assert(h==&dma_position && active);
    *info=(i2s_chan_info_t){.is_enabled=!disabled_info, .total_dma_buf_size=dma_bytes};
    return fail_info ? ESP_FAIL : ESP_OK;
}
static void pump(void) {
    if (!setjmp(stopped)) worker(NULL);
    assert(!active && opened==closed && pending==0);
}
static void preview(void) {
    assert(mosaico_audio_preview()==ESP_OK);
    pump();
}
static void assert_complete_tone(void) {
    assert(submitted>0 && played==submitted && dropped==0);
    assert(tone_writes==10 && tail_writes==(int)((dma_bytes+319)/320));
}
static void run_audio_tests(void) {
    assert(!mosaico_audio_available());
    assert(mosaico_audio_set_volume(50)==ESP_ERR_INVALID_STATE);
    assert(mosaico_audio_init()==ESP_OK && mosaico_audio_available());
    assert(opened==0 && pending==0 && written==0); /* Boot never plays audio. */
    assert(mosaico_audio_set_volume(80)==ESP_OK && mosaico_audio_get_volume()==80);
    assert(opened==0 && pending==0); /* Restoring a preference is silent. */
    assert(mosaico_audio_set_volume(101)==ESP_ERR_INVALID_ARG);
    preview(); assert_complete_tone();
    assert(opened==1 && volume==80);
    assert(mosaico_audio_set_volume(0)==ESP_OK);
    int before=written;
    preview();
    assert(opened==1 && written==before && mosaico_audio_get_volume()==0);
    assert(mosaico_audio_set_volume(35)==ESP_OK);
    dma_bytes=4096; /* Capacity is queried, not a hard-coded delay/default ring. */
    preview(); assert_complete_tone();
    assert(opened==2 && volume==35);
    fail_open=true; /* Vendor can fail after enabling its I2S output. */
    preview(); assert(attempts==0);
    fail_open=false;
    fail_info=true;
    preview(); assert(attempts==0);
    fail_info=false;
    disabled_info=true;
    preview(); assert(attempts==0);
    disabled_info=false;
    dma_bytes=0;
    preview(); assert(attempts==0);
    dma_bytes=2880;
    fail_write=4; /* Tone write failure closes immediately. */
    preview(); assert(attempts==4 && tail_writes==0 && dropped>0);
    fail_write=12; /* Drain failure also closes, without an unbounded retry. */
    preview(); assert(attempts==12 && tail_writes==1 && dropped>0);
    fail_write=0;
    mute_after_write=3;
    preview(); assert(attempts==3 && tail_writes==0 && dropped>0 && volume==0);
    mute_after_write=0;
    assert(mosaico_audio_set_volume(50)==ESP_OK);
    preview(); assert_complete_tone();

    /* Actual bounded PCM queue: no caller codec I/O, owned copies, and exactly
     * one queued clip. Same worker/mutex also services volume feedback. */
    static uint8_t pcm[MOSAICO_AUDIO_PCM_MAX_BYTES];
    for (size_t i=0;i<sizeof(pcm);i+=2) { pcm[i]=0; pcm[i+1]=8; }
    memcpy(next_pcm, pcm, sizeof(next_pcm));
    int before_open=opened;
    uint32_t another=0;
    assert(mosaico_audio_stream_open(-1,&another)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_stream_open(50,NULL)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_stream_open(50,&stream_under_test)==ESP_OK && stream_under_test!=0);
    assert(mosaico_audio_stream_open(100,&another)==ESP_ERR_NOT_FINISHED && another==0);
    assert(opened==before_open && mosaico_audio_get_volume()==50);
    try_lock_busy=true;
    assert(mosaico_audio_stream_write(stream_under_test,pcm,3200)==ESP_ERR_NOT_FINISHED);
    try_lock_busy=false;
    assert(mosaico_audio_stream_write(stream_under_test,pcm,3)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_stream_write(stream_under_test,pcm,sizeof(pcm)+2)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_stream_write(stream_under_test,NULL,2)==ESP_ERR_INVALID_ARG);
    assert(mosaico_audio_stream_write(stream_under_test,NULL,0)==ESP_OK);
    assert(mosaico_audio_stream_write(stream_under_test,pcm,3200)==ESP_OK);
    assert(mosaico_audio_stream_write(stream_under_test,pcm,3200)==ESP_ERR_NOT_FINISHED);
    memset(pcm,0,3200); /* Caller memory is not borrowed by asynchronous DMA. */
    pcm_amplitude=1024;
    pump(); assert_complete_tone();
    assert(opened==before_open+1 && mosaico_audio_get_volume()==50);

    /* Queue another SFX while the first clip is active; request a preview in
     * that same playback. Codec open() assertions prohibit overlapping owners. */
    pcm_amplitude=1024;
    before_open=opened;
    enqueue_after_write=2; preview_after_write=3;
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    pump(); assert_complete_tone();
    assert(opened==before_open+3 && volume==50);

    /* Mute drops queued sound permanently, so a fast unmute cannot revive it. */
    before_open=opened;
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    assert(mosaico_audio_set_volume(0)==ESP_OK);
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    assert(mosaico_audio_set_volume(50)==ESP_OK);
    pump(); assert(opened==before_open);

    /* Quit/GC cancellation does not acquire the codec lock or drain old PCM. */
    close_after_write=3;
    pcm_amplitude=1024;
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    pump(); assert(attempts==3 && tail_writes==0 && dropped>0);
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_ERR_INVALID_STATE);
    close_after_write=0;
    const uint32_t old_stream=stream_under_test;
    assert(mosaico_audio_stream_open(100,&stream_under_test)==ESP_OK && stream_under_test!=old_stream);
    mosaico_audio_stream_close(old_stream); /* Stale finalizers cannot cancel a new job. */
    pcm_amplitude=2048;
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    pump(); assert_complete_tone();

    /* Closing a queued job prevents even the first DAC open. */
    before_open=opened;
    assert(mosaico_audio_stream_write(stream_under_test,next_pcm,sizeof(next_pcm))==ESP_OK);
    mosaico_audio_stream_close(stream_under_test);
    mosaico_audio_stream_close(stream_under_test);
    pump(); assert(opened==before_open);
    assert(mosaico_audio_stream_open(100,&stream_under_test)==ESP_OK);
    for (size_t i=0;i<sizeof(pcm);i+=2) { pcm[i]=0; pcm[i+1]=8; }
    assert(mosaico_audio_stream_write(stream_under_test,pcm,sizeof(pcm))==ESP_OK);
    pump();
    assert(tone_writes==52 && submitted==sizeof(pcm)/2 && played==submitted && dropped==0);
    assert(tail_writes==(int)((dma_bytes+319)/320));
    mosaico_audio_stream_close(stream_under_test);
    pump();
    pcm_amplitude=-1;
    puts("speaker worker: bounded owned PCM queue, single preview/Works owner, volume/mute, cancellation, dynamic DMA drain and failure cleanup passed");
}

#ifndef WORKS_AUDIO_LUA_TEST
int main(void) { run_audio_tests(); return 0; }
#endif
