/* SPDX-License-Identifier: Apache-2.0 */
/* Real loader/gesture/capture code. Fakes replace only SDK/task boundaries. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"
#include "mosaic_loader.c"

struct test_queue { size_t capacity, item_size, count; uint8_t *items; };
struct test_semaphore { bool held; TaskHandle_t holder; };
struct esp_display_presenter { unsigned marker; };
static struct esp_display_presenter presenter;
static int64_t clock_us;
static uint32_t generation = 1;
static bool platform_paused, mutex_timeout;
static unsigned activity, touches, pause_calls, resume_calls, back_calls, pop_calls;
static int16_t touch_x[128], touch_y[128];
static bool touch_down[128];
static uint16_t page;
static unsigned pause_fail_on, resume_fail_on;
static unsigned capture_refresh_calls, capture_refresh_fail_on;
static bool pause_error_token, missing_tile, partial_plan, cancel_frame, input_during_flush;
static bool late_update_pending;
static unsigned settle_extra_us;
static void render_fixture_update(void);
static esp_err_t begin_error, flush_error, submit_error, commit_error, quiesce_error;
static unsigned real_begins, real_submits, real_commits, real_cancels, allocations;
static unsigned fail_allocation;
static char capture_warning[256];
void mosaic_test_log_warning(const char *format, ...)
{
    va_list arguments;
    va_start(arguments,format);
    const int length=vsnprintf(capture_warning,sizeof(capture_warning),format,arguments);
    va_end(arguments);
    assert(length>=0 && (size_t)length<sizeof(capture_warning));
}
static atomic_bool block_copy, copy_started, allow_copy, detach_returned;
static const mosaic_app_descriptor_t root_app = {.id=0,.name="mosaic-hub",.root_stack_key=1};
static const mosaic_app_descriptor_t child_app = {.id=4,.name="settings",.root_stack_key=7};
const mosaic_app_descriptor_t *const mosaic_app_registry[] = {&root_app,&child_app};
const size_t mosaic_app_registry_count = sizeof(mosaic_app_registry)/sizeof(mosaic_app_registry[0]);
static const mosaic_app_descriptor_t *active_app = &root_app;

/* Screen worker scheduling and device-control getters are test boundaries.
 * The UI functions below are extracted verbatim, not reimplemented. */
static const char *TAG="fixture-ui";
static atomic_bool s_started=true, s_screen_asleep, s_screen_dimmed;
static atomic_bool s_hub_presenter_active=true, s_ignore_wake_pointer;
static SemaphoreHandle_t s_screen_control_lock;
static bool fixture_panel_enabled=true, fixture_exclusive;
#define SCREEN_CMD_WAKE 2U
#define SCREEN_PAUSE_TIMEOUT_MS 1500U
static void screen_post(uint32_t command,bool from_isr)
{ assert(command==SCREEN_CMD_WAKE && !from_isr);s_screen_asleep=false;fixture_panel_enabled=true; }
static bool screen_set_dimmed(bool dimmed) { s_screen_dimmed=dimmed;return true; }
bool display_service_panel_enabled(void) { return fixture_panel_enabled; }
esp_err_t display_service_set_panel_enabled(bool enabled) { fixture_panel_enabled=enabled;return ESP_OK; }
bool display_service_is_started(void) { return true; }
bool display_service_has_exclusive_session(void) { return fixture_exclusive; }
uint32_t display_service_width(void) { return 4; }
uint32_t display_service_height(void) { return 4; }
esp_err_t display_service_get_rotation(uint16_t *out) { *out=90;return ESP_OK; }
esp_err_t display_service_get_brightness(uint8_t *out) { *out=18;return ESP_OK; }
esp_err_t display_service_request_exit(void) { return ESP_ERR_NOT_FOUND; }

esp_err_t __wrap_esp_display_presenter_begin_next_frame(esp_display_presenter_t *,
    const esp_display_present_surface_request_t *, esp_display_present_area_t *,
    size_t, size_t *, bool *);
esp_err_t __wrap_esp_display_presenter_submit_buffer(esp_display_presenter_t *,
    const esp_display_presenter_buffer_t *, const esp_display_present_area_t *, size_t);
esp_err_t __wrap_esp_display_presenter_commit_frame(esp_display_presenter_t *,
    const esp_display_presenter_submit_t *);
esp_err_t __wrap_esp_display_presenter_quiesce(esp_display_presenter_t *,uint32_t);
void __wrap_esp_display_presenter_cancel_frame(esp_display_presenter_t *);

static void brief_wait(void)
{
    const struct timespec delay = {.tv_nsec=1000000};
    nanosleep(&delay, NULL);
}

/* Only capture.c is compiled with this replacement. Pause a short CPU copy to
 * prove detach cannot free the request while a render-side reader owns it. */
void *mosaic_test_memcpy(void *dst, const void *src, size_t length)
{
    if (atomic_load(&block_copy)) {
        atomic_store(&copy_started, true);
        while (!atomic_load(&allow_copy)) brief_wait();
    }
    return memcpy(dst, src, length);
}

const char *esp_err_to_name(esp_err_t error) { return error == ESP_OK ? "OK" : "fixture-error"; }
int64_t esp_timer_get_time(void) { return clock_us; }
void *heap_caps_malloc(size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ++allocations;
    return fail_allocation == allocations ? NULL : malloc(size);
}
void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ++allocations;
    return fail_allocation == allocations ? NULL : calloc(count,size);
}
void vTaskDelay(TickType_t ticks)
{
    if(ticks==pdMS_TO_TICKS(100)) {
        assert(capture_refresh_calls>0 && !platform_paused);
        clock_us+=100000+settle_extra_us;
        if(late_update_pending && !platform_paused) {
            late_update_pending=false;
            render_fixture_update();
        }
    }
    brief_wait();
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
BaseType_t xTaskCreate(void (*task)(void *), const char *name, uint32_t stack,
    void *arg, UBaseType_t priority, TaskHandle_t *out) { *out=(void *)2; return pdPASS; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return calloc(1,sizeof(struct test_semaphore)); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t lock, TickType_t timeout)
{
    if (mutex_timeout || lock->held) return pdFALSE;
    lock->held=true; lock->holder=xTaskGetCurrentTaskHandle(); return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t lock) { lock->held=false; lock->holder=NULL; return pdTRUE; }
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t lock) { return lock->holder; }
void vSemaphoreDelete(SemaphoreHandle_t lock) { free(lock); }
QueueHandle_t xQueueCreate(UBaseType_t count, size_t item_size)
{
    QueueHandle_t queue=calloc(1,sizeof(*queue));
    queue->capacity=count; queue->item_size=item_size; queue->items=calloc(count,item_size);
    return queue;
}
BaseType_t xQueueSend(QueueHandle_t queue,const void *item,TickType_t timeout)
{
    if(queue->count==queue->capacity) return pdFALSE;
    memcpy(queue->items+queue->count++*queue->item_size,item,queue->item_size); return pdTRUE;
}
BaseType_t xQueueSendToFront(QueueHandle_t queue,const void *item,TickType_t timeout)
{
    if(queue->count==queue->capacity) return pdFALSE;
    memmove(queue->items+queue->item_size,queue->items,queue->count++*queue->item_size);
    memcpy(queue->items,item,queue->item_size); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t timeout)
{
    if(!queue->count) return pdFALSE;
    memcpy(item,queue->items,queue->item_size);
    memmove(queue->items,queue->items+queue->item_size,--queue->count*queue->item_size); return pdTRUE;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) { return (UBaseType_t)queue->count; }
void vQueueDelete(QueueHandle_t queue) { free(queue->items);free(queue); }

static void begin_frame(void)
{
    size_t count=0; bool full=false;
    assert(__wrap_esp_display_presenter_begin_next_frame(&presenter,NULL,NULL,0,&count,&full)==begin_error);
}
static void submit_band(unsigned y, unsigned rows)
{
    uint16_t pixels[16];
    for(unsigned i=0;i<4*rows;++i) pixels[i]=(uint16_t)(0x2100+4*y+i);
    const esp_display_presenter_buffer_t buffer={
        .surface={.pixels=pixels,.pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565},
        .capacity_bytes=8*rows,.lease_id=1};
    const esp_display_present_area_t area={0,(int)y,3,(int)(y+rows-1)};
    assert(__wrap_esp_display_presenter_submit_buffer(&presenter,&buffer,&area,8)==submit_error);
}
static void render_fixture_frame(void)
{
    begin_frame();
    submit_band(0,2);
    if(!missing_tile) submit_band(2,2);
    if(cancel_frame) __wrap_esp_display_presenter_cancel_frame(&presenter);
    else {
        const esp_display_presenter_submit_t submit={.coverage=ESP_DISPLAY_PRESENT_COVERAGE_FULL};
        assert(__wrap_esp_display_presenter_commit_frame(&presenter,&submit)==commit_error);
    }
}
static void render_fixture_pixel(int x,int y,uint16_t color)
{
    partial_plan=true;
    begin_frame();
    partial_plan=false;
    if(begin_error!=ESP_OK) return;
    uint16_t pixel=color;
    const esp_display_presenter_buffer_t buffer={
        .surface={.pixels=&pixel,.pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565},
        .capacity_bytes=sizeof(pixel),.lease_id=1};
    const esp_display_present_area_t area={x,y,x,y};
    assert(__wrap_esp_display_presenter_submit_buffer(&presenter,&buffer,&area,2)==submit_error);
    if(cancel_frame) __wrap_esp_display_presenter_cancel_frame(&presenter);
    else {
        const esp_display_presenter_submit_t submit={.coverage=ESP_DISPLAY_PRESENT_COVERAGE_FULL};
        assert(__wrap_esp_display_presenter_commit_frame(&presenter,&submit)==commit_error);
    }
}
static void render_fixture_update(void) { render_fixture_pixel(1,1,0xf7df); }
esp_err_t __real_esp_display_presenter_begin_next_frame(esp_display_presenter_t *p,
    const esp_display_present_surface_request_t *request,esp_display_present_area_t *areas,
    size_t capacity,size_t *count,bool *full)
{ assert(p==&presenter);++real_begins;*count=0;*full=!partial_plan;return begin_error; }
esp_err_t __real_esp_display_presenter_submit_buffer(esp_display_presenter_t *p,
    const esp_display_presenter_buffer_t *buffer,const esp_display_present_area_t *area,size_t stride)
{
    assert(p==&presenter);++real_submits;
    /* Real TE can swap/reuse this tile after submit: mirror must precede it. */
    memset(buffer->surface.pixels,0xcc,buffer->capacity_bytes);
    return submit_error;
}
esp_err_t __real_esp_display_presenter_commit_frame(esp_display_presenter_t *p,
    const esp_display_presenter_submit_t *submit) { ++real_commits;return commit_error; }
void __real_esp_display_presenter_cancel_frame(esp_display_presenter_t *p) { ++real_cancels; }
esp_err_t __real_esp_display_presenter_quiesce(esp_display_presenter_t *p,uint32_t ms)
{ assert(p==&presenter && ms>0);clock_us+=3000;return quiesce_error; }
esp_err_t esp_display_presenter_get_caps(const esp_display_presenter_t *p,
    esp_display_presenter_caps_t *caps)
{ *caps=(esp_display_presenter_caps_t){.contract=ESP_DISPLAY_PRESENT_CONTRACT_PARTITION,
    .pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,.width=4,.height=4};return ESP_OK; }

esp_err_t mosaic_esp_platform_create(const mosaic_esp_platform_config_t *config,
    mosaic_esp_platform_handle_t *out) { *out=(void *)3;return ESP_OK; }
void mosaic_esp_platform_delete(mosaic_esp_platform_handle_t platform) {}
const mosaic_platform_ops_t *mosaic_esp_platform_ops(void) { static mosaic_platform_ops_t ops;return &ops; }
esp_gsp_handle_t mosaic_esp_platform_ui(mosaic_esp_platform_handle_t platform)
{ return platform_paused ? NULL : (esp_gsp_handle_t)4; }
esp_gsp_handle_t mosaic_esp_platform_retained_ui(mosaic_esp_platform_handle_t platform)
{ return (esp_gsp_handle_t)4; }
uint32_t mosaic_esp_platform_generation(mosaic_esp_platform_handle_t platform) { return generation; }
bool mosaic_esp_platform_deliver_event(mosaic_esp_platform_handle_t platform,
    uint32_t gen,const esp_gsp_event_t *event) { return gen==generation; }
bool mosaic_esp_platform_deliver_pointer(mosaic_esp_platform_handle_t platform,
    mosaic_runtime_handle_t runtime,uint32_t gen,int32_t x,int32_t y,bool pressed) { return gen==generation; }
esp_err_t mosaic_esp_platform_quiesce(mosaic_esp_platform_handle_t p,uint32_t ms) { return ESP_OK; }
esp_err_t mosaic_esp_platform_activate(mosaic_esp_platform_handle_t p,esp_display_presenter_t *target)
{ platform_paused=false;return ESP_OK; }
esp_err_t mosaic_esp_platform_pause_screen(mosaic_esp_platform_handle_t p,uint32_t ms)
{
    assert(ms>0); ++pause_calls;
    if(pause_fail_on==pause_calls) { platform_paused=pause_error_token;return ESP_ERR_TIMEOUT; }
    /* Real SDK reaches the presenter only after command48 pause ack. */
    const esp_err_t error=__wrap_esp_display_presenter_quiesce(&presenter,ms);
    if(error!=ESP_OK) { platform_paused=false;return error; }
    platform_paused=true;return ESP_OK;
}
esp_err_t mosaic_esp_platform_resume_screen(mosaic_esp_platform_handle_t p)
{
    ++resume_calls;
    if(resume_fail_on==resume_calls) return ESP_FAIL;
    if(platform_paused) { platform_paused=false;render_fixture_frame(); }
    return ESP_OK;
}
esp_err_t mosaic_esp_platform_prepare_capture(mosaic_esp_platform_handle_t p)
{
    assert(!platform_paused && s_runtime_lock->held);
    ++capture_refresh_calls;
    return capture_refresh_calls==capture_refresh_fail_on
        ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_gsp_err_t esp_gsp_flush(esp_gsp_handle_t ui,uint32_t ms)
{
    assert(!platform_paused && ms>0);
    if(input_during_flush) assert(post_pointer(NULL,generation,20,20,true));
    return flush_error;
}
esp_gsp_err_t esp_gsp_inject_touch(esp_gsp_handle_t ui,int16_t x,int16_t y,bool pressed)
{ assert(ui==(esp_gsp_handle_t)4);assert(!platform_paused);assert(touches<128);
    touch_x[touches]=x;touch_y[touches]=y;touch_down[touches++]=pressed;return ESP_GSP_OK; }
void esp_gsp_render_stats(esp_gsp_handle_t ui,uint32_t *frames,uint64_t *busy)
{ *frames=23;*busy=98765; }
void esp_gsp_render_error_stats(esp_gsp_handle_t ui,uint32_t *errors,gsp_err_t *last)
{ *errors=2;*last=GSP_ERR_INVALID_ARG; }
esp_gsp_err_t esp_gsp_drawer_is_open(esp_gsp_handle_t ui,gsp_component_key_t key,bool *open)
{ *open=true;return ESP_GSP_OK; }
esp_gsp_err_t esp_gsp_stack_view_get_top(esp_gsp_handle_t ui,gsp_component_key_t key,uint16_t *out)
{ *out=page;return ESP_GSP_OK; }
esp_gsp_err_t esp_gsp_stack_view_pop(esp_gsp_handle_t ui,gsp_component_key_t key,bool animated)
{ ++pop_calls;page=0;return ESP_GSP_OK; }
esp_gsp_err_t esp_gsp_page_flow_get_page(esp_gsp_handle_t ui,gsp_component_key_t key,uint16_t *out)
{ *out=page;return ESP_GSP_OK; }
esp_gsp_err_t esp_gsp_page_flow_set_page(esp_gsp_handle_t ui,gsp_component_key_t key,uint16_t next,bool animated)
{ page=next;return ESP_GSP_OK; }
esp_gsp_err_t mosaic_app_shell_show_system_notice(esp_gsp_handle_t ui,mosaic_system_notice_t n,uint32_t ms)
{ return ESP_GSP_OK; }
void mosaic_hub_show_lock_screen(bool charging) {}
void mosaic_hub_lock_screen(void) {}
void mosaic_ui_note_screen_activity(void) { ++activity; }
void mosaic_ui_set_hub_foreground(bool foreground) { ++activity; }
bool mosaic_ui_absorb_wake_pointer(bool pressed) { return false; }
const mosaic_app_descriptor_t *mosaic_app_root(void) { return &root_app; }
bool mosaic_app_catalog_validate(void) { return true; }
esp_err_t mosaic_runtime_create(const mosaic_runtime_config_t *config,mosaic_runtime_handle_t *out)
{ *out=(void *)5;return ESP_OK; }
void mosaic_runtime_delete(mosaic_runtime_handle_t runtime) {}
esp_err_t mosaic_runtime_start(mosaic_runtime_handle_t runtime,const char *initial) { return ESP_OK; }
esp_err_t mosaic_runtime_step(mosaic_runtime_handle_t runtime,int64_t now) { return ESP_OK; }
const mosaic_app_descriptor_t *mosaic_runtime_active_app(mosaic_runtime_handle_t runtime) { return active_app; }
esp_err_t mosaic_runtime_request_app(mosaic_runtime_handle_t runtime,const char *name)
{ active_app=&child_app;++generation;return ESP_OK; }
esp_err_t mosaic_runtime_back(mosaic_runtime_handle_t runtime)
{ active_app=&root_app;++generation;++back_calls;return ESP_OK; }
esp_err_t mosaic_runtime_notify_model_changed(mosaic_runtime_handle_t runtime,uint16_t app_id,uint32_t revision)
{ return ESP_OK; }

#include "ui_diagnostics_under_test.inc"

static void process_next(void)
{
    mosaic_loader_command_t command;
    assert(xQueueReceive(s_command_queue,&command,0)==pdTRUE);
    assert(xSemaphoreTake(s_runtime_lock,0)==pdTRUE);
    process_command(&command);
    xSemaphoreGive(s_runtime_lock);
}
static void reset_faults(void)
{
    assert(!s_gesture.active);
    s_command_queue->count=0; s_pointer_pressed=false;
    atomic_store(&s_input_pending,false);atomic_store(&s_capture_in_progress,false);
    s_quiesced=false;s_screen_paused=false;platform_paused=false;mutex_timeout=false;
    pause_calls=resume_calls=pause_fail_on=resume_fail_on=0;
    capture_refresh_calls=capture_refresh_fail_on=0;
    pause_error_token=missing_tile=partial_plan=cancel_frame=input_during_flush=false;
    late_update_pending=false;
    settle_extra_us=0;
    begin_error=flush_error=submit_error=commit_error=quiesce_error=ESP_OK;fail_allocation=0;
    capture_warning[0]='\0';
}
static void assert_frame_pixels(mosaic_ui_frame_handle_t frame)
{
    mosaic_ui_frame_info_t info;
    assert(mosaic_frame_capture_info(frame,&info)==ESP_OK);
    assert(info.width==4 && info.height==4 && info.stride_bytes==8 && info.size_bytes==32);
    uint16_t pixels[16];
    assert(mosaic_frame_capture_read(frame,0,pixels,sizeof(pixels))==ESP_OK);
    for(unsigned i=0;i<16;++i) assert(pixels[i]==0x2100+i);
    assert(mosaic_frame_capture_read(frame,31,pixels,2)==ESP_ERR_INVALID_SIZE);
    assert(mosaic_frame_capture_read(frame,0,pixels,4097)==ESP_ERR_INVALID_ARG);
}

static void test_input(void)
{
    assert(mosaic_loader_simulate_tap(-1,0)==ESP_ERR_INVALID_ARG);
    assert(mosaic_loader_simulate_tap(0,480)==ESP_ERR_INVALID_ARG);
    assert(mosaic_loader_simulate_drag(10,10,300,400,31)==ESP_ERR_INVALID_ARG);
    assert(mosaic_loader_simulate_drag(10,10,300,400,2001)==ESP_ERR_INVALID_ARG);
    assert(mosaic_loader_simulate_drag(10,400,300,100,100)==ESP_OK);
    assert(touches==0); /* Admission alone must not touch the renderer. */
    assert(mosaic_loader_simulate_tap(20,20)==ESP_ERR_INVALID_STATE);
    process_next();assert(touches==1 && touch_down[0]);
    clock_us=32000;advance_simulated_input(clock_us);
    assert(touch_x[1]==102 && touch_y[1]==304 && touch_down[1]);
    clock_us=150000;advance_simulated_input(clock_us);
    assert(touch_x[2]==300 && touch_y[2]==100 && touch_down[2]);
    clock_us=182000;advance_simulated_input(clock_us);
    assert(touches==4 && !touch_down[3] && !atomic_load(&s_input_pending));
    assert(mosaic_loader_simulate_tap(40,50)==ESP_OK);process_next();
    clock_us+=32000;advance_simulated_input(clock_us);
    assert(touches==6 && !touch_down[5]);

    assert(mosaic_loader_simulate_drag(10,20,400,450,2000)==ESP_OK);process_next();
    const unsigned before=touches;
    ++generation;clock_us+=32000;advance_simulated_input(clock_us);
    assert(touches==before && !s_gesture.active && s_last_input_error==ESP_ERR_INVALID_STATE);
    assert(mosaic_loader_simulate_tap(40,40)==ESP_OK);process_next();
    assert(mosaic_loader_request_back()==ESP_OK);process_next();
    assert(!s_gesture.active && !touch_down[touches-1]);

    s_command_queue->count=s_command_queue->capacity;
    assert(mosaic_loader_simulate_tap(20,20)==ESP_ERR_TIMEOUT);
    assert(!atomic_load(&s_input_pending));s_command_queue->count=0;
    atomic_store(&s_capture_in_progress,true);
    assert(mosaic_loader_simulate_tap(20,20)==ESP_ERR_INVALID_STATE);
    atomic_store(&s_capture_in_progress,false);

    active_app=&child_app;page=1;
    assert(mosaic_loader_request_back()==ESP_OK);process_next();
    assert(pop_calls==1 && active_app==&child_app);
    assert(mosaic_loader_request_back()==ESP_OK);process_next();
    assert(back_calls==1 && active_app==&root_app);
    puts("UI input: queued tap/drag, timed endpoint/release, generation isolation, busy/full admission and Back navigation passed");
}

static void test_status(void)
{
    const unsigned before=activity;
    mosaic_ui_diagnostics_t out={0};
    s_screen_paused=true;platform_paused=true;s_runtime_errors=3;
    assert(mosaic_loader_get_diagnostics(&out,50)==ESP_OK);
    assert(!strcmp(out.app_name,"mosaic-hub") && out.app_id==0);
    assert(out.render_frames==23 && out.render_busy_us==98765 && out.render_errors==2);
    assert(out.last_render_error==GSP_ERR_INVALID_ARG && out.runtime_errors==3);
    assert(out.screen_paused && out.drawer_open && activity==before);
    mutex_timeout=true;
    assert(mosaic_loader_get_diagnostics(&out,1)==ESP_ERR_TIMEOUT);
    mutex_timeout=false;
    assert(xSemaphoreTake(s_runtime_lock,0)==pdTRUE);
    assert(mosaic_loader_get_diagnostics(&out,1)==ESP_ERR_INVALID_STATE);
    xSemaphoreGive(s_runtime_lock);reset_faults();
    puts("UI status: retained asleep renderer counters, catalog identity, no activity and bounded/reentrant lock rejection passed");
}

static void test_capture_control(void)
{
    const unsigned before=activity;
    mosaic_ui_frame_handle_t frame=NULL;
    reset_faults();
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_OK);
    assert(pause_calls==2 && resume_calls==2 && !platform_paused);
    assert(capture_refresh_calls==2);
    assert_frame_pixels(frame);
    assert(!s_runtime_lock->held && activity==before);
    mosaic_ui_frame_handle_t another=NULL;
    assert(mosaic_loader_capture_begin(&another,1000)==ESP_ERR_INVALID_STATE);
    assert(another==NULL);assert_frame_pixels(frame);mosaic_frame_capture_delete(frame);

    for(unsigned mode=0;mode<10;++mode) {
        reset_faults();frame=NULL;
        switch(mode) {
        case 0:missing_tile=true;break;
        case 1:partial_plan=true;break;
        case 2:cancel_frame=true;break;
        case 3:submit_error=ESP_FAIL;break;
        case 4:commit_error=ESP_FAIL;break;
        case 5:flush_error=ESP_ERR_TIMEOUT;break;
        case 6:pause_fail_on=1;break;
        case 7:pause_fail_on=2;break;
        case 8:pause_fail_on=1;pause_error_token=true;break;
        case 9:input_during_flush=true;break;
        }
        assert(mosaic_loader_capture_begin(&frame,1000)!=ESP_OK);
        assert(frame==NULL && !platform_paused && !s_runtime_lock->held);
        assert(!atomic_load(&s_capture_in_progress));
        static const char *const stages[]={
            "complete","complete","complete","complete","complete",
            "flush","pause_before","pause_after","pause_before","interrupted"};
        char expected[64];
        snprintf(expected,sizeof(expected),"failed stage=%s error=",stages[mode]);
        assert(strncmp(capture_warning,expected,strlen(expected))==0);
        assert(strstr(capture_warning," seen=") && strstr(capture_warning," full="));
        assert(strstr(capture_warning," tiles=") && strstr(capture_warning," coverage="));
        assert(strstr(capture_warning," commit="));
        if(mode==0) assert(strstr(capture_warning,"seen=1 full=1 tiles=1 coverage=8 commit=1"));
        if(mode==1) assert(strstr(capture_warning,"seen=1 full=0 tiles=0 coverage=0 commit=0"));
        if(mode==6) assert(strstr(capture_warning,"seen=0 full=0 tiles=0 coverage=0 commit=0"));
        if(mode==6) assert(strstr(capture_warning,"quiesce_calls=0 quiesce_error=0x0 quiesce_ms=0"));
        if(mode==7) assert(strstr(capture_warning,"quiesce_calls=1 quiesce_error=0x0 quiesce_ms=3"));
    }
    reset_faults();quiesce_error=ESP_ERR_TIMEOUT;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_TIMEOUT);
    assert(frame==NULL && !platform_paused && !s_runtime_lock->held);
    assert(strstr(capture_warning,"failed stage=pause_before error=0x107"));
    assert(strstr(capture_warning,"quiesce_calls=1 quiesce_error=0x107 quiesce_ms=3"));
    reset_faults();resume_fail_on=2;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_FAIL);
    assert(strstr(capture_warning,"failed stage=resume_restore error="));
    assert(frame==NULL && platform_paused && s_screen_paused);
    assert(capture_refresh_calls==1);
    /* This is the actual retry called by the UI awake/prepare-input path. */
    assert(mosaic_loader_resume_screen()==ESP_OK);
    assert(!platform_paused && !s_screen_paused);
    assert(mosaic_loader_simulate_tap(30,40)==ESP_OK);process_next();
    clock_us+=32000;advance_simulated_input(clock_us);
    assert(!s_gesture.active && !touch_down[touches-1]);

    for(unsigned failed=1;failed<=2;++failed) {
        reset_faults();capture_refresh_fail_on=failed;
        assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_INVALID_STATE);
        assert(frame==NULL && !platform_paused && !s_screen_paused);
        assert(!s_runtime_lock->held && !atomic_load(&s_capture_in_progress));
        assert(capture_refresh_calls==2);
        assert(strstr(capture_warning,failed==1
            ? "failed stage=refresh error=" : "failed stage=refresh_restore error="));
    }
    reset_faults();resume_fail_on=1;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_FAIL);
    assert(frame==NULL && !platform_paused && !s_screen_paused);
    assert(capture_refresh_calls==1 && !s_runtime_lock->held);
    assert(strstr(capture_warning,"failed stage=resume_frame error="));

    reset_faults();s_pointer_pressed=true;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_INVALID_STATE && frame==NULL);
    reset_faults();s_quiesced=true;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_INVALID_STATE);
    reset_faults();s_screen_paused=true;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_INVALID_STATE);
    reset_faults();fail_allocation=allocations+2;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_ERR_NO_MEM && frame==NULL);
    reset_faults();assert(mosaic_loader_capture_begin(&frame,1000)==ESP_OK);
    mosaic_frame_capture_delete(frame);
    assert(activity==before);
    puts("Device capture control: full coverage/commit/fence, tile reuse, no UI lock during reads; missing/partial/cancel/error/timeout/owner/input/PSRAM failures and failed-resume input recovery passed");
    puts("Capture failure diagnostics: exact stage and numeric seen/full/tiles/coverage/commit distinguish pause, flush, incomplete raster and resume recovery; no pixel/scene payloads");
    puts("Capture refresh: admitted after resume under owner lock, before sampling; restored after detach; initial/restore failure cleanup and failed-resume refresh exclusion passed");
    puts("Pause timeout diagnostics: no presenter call distinguishes command48 pause/ack failure from a real transfer/present-fence timeout; public quiesce arguments/results pass through unchanged");
}

static mosaic_ui_frame_handle_t create_frame(void)
{
    esp_display_presenter_caps_t caps;
    esp_display_presenter_get_caps(&presenter,&caps);
    mosaic_ui_frame_handle_t frame=NULL;
    assert(mosaic_frame_capture_create(&caps,&frame)==ESP_OK);
    assert(mosaic_frame_capture_arm(frame,&presenter)==ESP_OK);return frame;
}
static void *copy_thread(void *ctx) { submit_band(0,4);return NULL; }
static void *detach_thread(void *ctx)
{ mosaic_frame_capture_detach(ctx);atomic_store(&detach_returned,true);return NULL; }
static void test_capture_boundary(void)
{
    reset_faults();
    const unsigned initial_allocations=allocations;
    render_fixture_frame();assert(allocations==initial_allocations);
    mosaic_ui_frame_handle_t frame=create_frame();begin_frame();
    /* Duplicated rows cannot hide missing coverage. */
    submit_band(0,2);submit_band(0,2);
    const esp_display_presenter_submit_t submit={.coverage=ESP_DISPLAY_PRESENT_COVERAGE_FULL};
    assert(__wrap_esp_display_presenter_commit_frame(&presenter,&submit)==ESP_OK);
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_ERR_INVALID_STATE);
    mosaic_frame_capture_delete(frame);

    frame=create_frame();render_fixture_frame();mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,false)==ESP_ERR_TIMEOUT);
    uint8_t bytes[2];assert(mosaic_frame_capture_read(frame,0,bytes,2)==ESP_ERR_INVALID_STATE);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_OK);
    assert_frame_pixels(frame);mosaic_frame_capture_delete(frame);

    frame=create_frame();begin_frame();
    uint8_t small[8]={0};
    const esp_display_presenter_buffer_t invalid={.surface={.pixels=small,
        .pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565},.capacity_bytes=sizeof(small)};
    const esp_display_present_area_t area={0,0,3,3};
    assert(__wrap_esp_display_presenter_submit_buffer(&presenter,&invalid,&area,8)==ESP_OK);
    assert(__wrap_esp_display_presenter_commit_frame(&presenter,&submit)==ESP_OK);
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_ERR_INVALID_SIZE);
    mosaic_frame_capture_delete(frame);

    frame=create_frame();begin_frame();
    atomic_store(&block_copy,true);atomic_store(&copy_started,false);
    atomic_store(&allow_copy,false);atomic_store(&detach_returned,false);
    pthread_t copy,detach;
    assert(pthread_create(&copy,NULL,copy_thread,NULL)==0);
    while(!atomic_load(&copy_started)) brief_wait();
    assert(pthread_create(&detach,NULL,detach_thread,frame)==0);
    for(unsigned i=0;i<10;++i) brief_wait();
    assert(!atomic_load(&detach_returned));
    atomic_store(&allow_copy,true);
    pthread_join(copy,NULL);pthread_join(detach,NULL);
    atomic_store(&block_copy,false);
    assert(atomic_load(&detach_returned));mosaic_frame_capture_delete(frame);
    puts("Capture wrapper: unarmed zero-copy passthrough, duplicate coverage/invalid lease bounds rejection, mandatory present fence and concurrent detach lifetime passed");
}

static void assert_updated_pixels(mosaic_ui_frame_handle_t frame)
{
    uint16_t pixels[16];
    assert(mosaic_frame_capture_read(frame,0,pixels,sizeof(pixels))==ESP_OK);
    for(unsigned i=0;i<16;++i) assert(pixels[i]==(i==5 ? 0xf7df : 0x2100+i));
    mosaic_frame_capture_progress_t progress;
    mosaic_frame_capture_progress(frame,&progress);
    assert(progress.commits==2 && progress.covered_pixels==16 && progress.tiles==3);
}

static void test_capture_later_frames(void)
{
    reset_faults();
    mosaic_ui_frame_handle_t frame=create_frame();
    render_fixture_frame();
    render_fixture_update();
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_OK);
    assert_updated_pixels(frame);
    mosaic_frame_capture_delete(frame);

    frame=create_frame();render_fixture_frame();
    render_fixture_pixel(1,1,0xf7df);
    render_fixture_pixel(2,2,0xe4e4);
    render_fixture_pixel(1,1,0xfd20);
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_OK);
    uint16_t pixels[16];
    assert(mosaic_frame_capture_read(frame,0,pixels,sizeof(pixels))==ESP_OK);
    for(unsigned i=0;i<16;++i) assert(pixels[i]==(i==5 ? 0xfd20 : i==10 ? 0xe4e4 : 0x2100+i));
    mosaic_frame_capture_progress_t progress;
    mosaic_frame_capture_progress(frame,&progress);
    assert(progress.commits==4 && progress.covered_pixels==16);
    mosaic_frame_capture_delete(frame);

    frame=create_frame();render_fixture_frame();render_fixture_update();
    render_fixture_frame();mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_OK);
    assert_frame_pixels(frame);mosaic_frame_capture_delete(frame);
    frame=create_frame();render_fixture_frame();
    missing_tile=true;render_fixture_frame();missing_tile=false;
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_ERR_INVALID_STATE);
    mosaic_frame_capture_delete(frame);
    frame=create_frame();render_fixture_frame();begin_frame();
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_ERR_INVALID_STATE);
    mosaic_frame_capture_delete(frame);

    /* A later bad/cancelled raster must never turn into an accepted mixture. */
    for(unsigned mode=0;mode<4;++mode) {
        frame=create_frame();render_fixture_frame();
        if(mode==0) cancel_frame=true;
        else if(mode==1) commit_error=ESP_FAIL;
        else if(mode==2) submit_error=ESP_FAIL;
        else begin_error=ESP_FAIL;
        render_fixture_update();
        mosaic_frame_capture_detach(frame);
        assert(mosaic_frame_capture_complete(frame,true)!=ESP_OK);
        mosaic_frame_capture_delete(frame);reset_faults();
    }
    late_update_pending=true;
    assert(mosaic_loader_capture_begin(&frame,1000)==ESP_OK);
    assert_updated_pixels(frame);mosaic_frame_capture_delete(frame);
    assert(mosaic_loader_capture_begin(&frame,100)==ESP_ERR_TIMEOUT && frame==NULL);
    assert(strstr(capture_warning,"failed stage=settle error="));
    settle_extra_us=100000;
    assert(mosaic_loader_capture_begin(&frame,150)==ESP_ERR_TIMEOUT && frame==NULL);
    assert(strstr(capture_warning,"failed stage=flush error="));
    puts("Capture sampling: complete base plus later committed dirty pixels, rejection after later error/cancel, budgeted normal-runner window passed");
}

static void test_full_device_geometry(void)
{
    esp_display_presenter_caps_t caps={.contract=ESP_DISPLAY_PRESENT_CONTRACT_PARTITION,
        .pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,.width=480,.height=480};
    mosaic_ui_frame_handle_t frame=NULL;
    assert(mosaic_frame_capture_create(&caps,&frame)==ESP_OK);
    assert(mosaic_frame_capture_arm(frame,&presenter)==ESP_OK);begin_frame();
    uint16_t tile[480*10];
    for(unsigned y=0;y<480;y+=10) {
        for(unsigned row=0;row<10;++row) for(unsigned x=0;x<480;++x) {
            tile[row*480+x]=(uint16_t)(((y+row)<<8)^x);
        }
        const esp_display_presenter_buffer_t buffer={.surface={.pixels=tile,
            .pixel_format=ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565},
            .capacity_bytes=sizeof(tile),.lease_id=1};
        const esp_display_present_area_t area={0,(int)y,479,(int)y+9};
        assert(__wrap_esp_display_presenter_submit_buffer(&presenter,&buffer,&area,960)==ESP_OK);
    }
    const esp_display_presenter_submit_t submit={.coverage=ESP_DISPLAY_PRESENT_COVERAGE_FULL};
    assert(__wrap_esp_display_presenter_commit_frame(&presenter,&submit)==ESP_OK);
    mosaic_frame_capture_detach(frame);
    assert(mosaic_frame_capture_complete(frame,true)==ESP_OK);
    mosaic_ui_frame_info_t info;
    assert(mosaic_frame_capture_info(frame,&info)==ESP_OK);
    assert(info.width==480 && info.height==480 && info.stride_bytes==960 && info.size_bytes==460800);
    uint8_t last[2];
    assert(mosaic_frame_capture_read(frame,460798,last,2)==ESP_OK);
    const uint16_t expected=(uint16_t)((479U<<8)^479U);
    assert(last[0]==(uint8_t)expected && last[1]==(uint8_t)(expected>>8));
    mosaic_frame_capture_delete(frame);
    caps.height=481;
    assert(mosaic_frame_capture_create(&caps,&frame)==ESP_ERR_NOT_SUPPORTED && frame==NULL);
    puts("Device frame geometry: all 48 reusable 480x10 bands compose exactly 480x480 RGB565LE (460800 bytes); oversize rejected");
}

static void test_public_ui(void)
{
    reset_faults();
    s_screen_asleep=true;s_screen_dimmed=true;s_screen_paused=true;platform_paused=true;
    fixture_panel_enabled=false;
    const unsigned before=activity;
    mosaic_ui_diagnostics_t status;
    for(unsigned i=0;i<10;++i) {
        assert(mosaic_ui_get_diagnostics(&status,50)==ESP_OK);
        assert(status.started && status.asleep && status.dimmed && !status.panel_enabled);
        assert(status.rotation==90 && status.brightness_percent==18 && status.render_frames==23);
    }
    mosaic_ui_frame_info_t info;
    mosaic_ui_frame_handle_t frame=NULL;
    assert(mosaic_ui_capture_begin(&info,&frame,1000)==ESP_ERR_INVALID_STATE);
    assert(frame==NULL && activity==before && !fixture_panel_enabled);
    s_screen_asleep=false;fixture_panel_enabled=true;reset_faults();
    fixture_exclusive=true;s_hub_presenter_active=false;
    assert(mosaic_ui_capture_begin(&info,&frame,1000)==ESP_ERR_INVALID_STATE);
    assert(mosaic_ui_simulate_tap(20,20)==ESP_ERR_INVALID_STATE);
    fixture_exclusive=false;s_hub_presenter_active=true;
    assert(mosaic_ui_capture_begin(&info,&frame,1000)==ESP_OK);
    assert(info.rotation==90 && info.size_bytes==32 && s_screen_dimmed);
    assert(!s_screen_control_lock->held && !s_runtime_lock->held);
    uint8_t pixels[32];
    assert(mosaic_ui_capture_read(frame,0,pixels,sizeof(pixels))==ESP_OK);
    assert(activity==before);mosaic_ui_capture_end(frame);

    reset_faults();resume_fail_on=2;
    assert(mosaic_ui_capture_begin(&info,&frame,1000)==ESP_FAIL);
    assert(platform_paused && s_screen_paused && !s_screen_asleep && fixture_panel_enabled);
    resume_fail_on=3;
    assert(mosaic_ui_simulate_tap(20,30)==ESP_FAIL);
    assert(uxQueueMessagesWaiting(s_command_queue)==0 && !s_screen_control_lock->held);
    resume_fail_on=0;
    assert(mosaic_ui_simulate_tap(20,30)==ESP_OK);
    assert(!platform_paused && !s_screen_paused);
    process_next();clock_us+=32000;advance_simulated_input(clock_us);
    assert(!s_gesture.active && !touch_down[touches-1]);

    reset_faults();resume_fail_on=2;
    assert(mosaic_ui_capture_begin(&info,&frame,1000)==ESP_FAIL);
    assert(mosaic_ui_back()==ESP_OK);
    /* Run the real wake routine at the screen worker scheduling boundary. */
    assert(xSemaphoreTake(s_screen_control_lock,0)==pdTRUE);
    assert(screen_apply_wake(true));
    xSemaphoreGive(s_screen_control_lock);
    assert(!platform_paused && !s_screen_paused);process_next();

    reset_faults();s_started=false;
    assert(mosaic_ui_simulate_drag(10,10,20,20,100)==ESP_ERR_INVALID_STATE);
    assert(mosaic_ui_get_diagnostics(&status,50)==ESP_OK && !status.started);
    s_started=true;
    assert(mosaic_ui_simulate_drag(10,400,10,200,100)==ESP_OK);process_next();
    clock_us+=100000;advance_simulated_input(clock_us);
    clock_us+=32000;advance_simulated_input(clock_us);
    assert(!s_gesture.active && touch_y[touches-1]==200 && !touch_down[touches-1]);
    puts("Public UI entries (verbatim): asleep status/capture isolation, dim preservation, foreign rejection, released transport locks and tap/Back recovery after failed renderer resume passed");
}

static void test_public_open_app(void)
{
    reset_faults();active_app=&root_app;
    const unsigned before=activity;
    assert(mosaic_ui_open_app(NULL)==ESP_ERR_INVALID_ARG);
    assert(mosaic_ui_open_app("")==ESP_ERR_INVALID_ARG);
    assert(mosaic_ui_open_app("unregistered")==ESP_ERR_NOT_FOUND);
    assert(activity==before && uxQueueMessagesWaiting(s_command_queue)==0);
    s_started=false;
    assert(mosaic_ui_open_app("settings")==ESP_ERR_INVALID_STATE);
    s_started=true;s_hub_presenter_active=false;
    assert(mosaic_ui_open_app("settings")==ESP_ERR_INVALID_STATE);
    assert(activity==before);s_hub_presenter_active=true;

    /* A stalled runtime/screen mutex cannot turn admission into a wait. */
    mutex_timeout=true;
    const unsigned resumes=resume_calls;
    assert(mosaic_ui_open_app("settings")==ESP_OK);
    assert(active_app==&root_app && generation>0);
    assert(activity==before+1 && resume_calls==resumes);
    assert(uxQueueMessagesWaiting(s_command_queue)==1);
    assert(!s_screen_control_lock->held && !s_runtime_lock->held);
    mutex_timeout=false;process_next();assert(active_app==&child_app);

    reset_faults();active_app=&root_app;
    s_screen_asleep=true;s_screen_paused=true;platform_paused=true;
    fixture_panel_enabled=false;
    assert(mosaic_ui_open_app("settings")==ESP_OK);
    assert(s_screen_asleep && platform_paused && !fixture_panel_enabled);
    assert(active_app==&root_app && uxQueueMessagesWaiting(s_command_queue)==1);
    /* Execute the real wake routine at the worker scheduling boundary. */
    assert(xSemaphoreTake(s_screen_control_lock,0)==pdTRUE);
    assert(screen_apply_wake(false));xSemaphoreGive(s_screen_control_lock);
    assert(!s_screen_asleep && !s_screen_paused && fixture_panel_enabled);
    process_next();assert(active_app==&child_app);

    reset_faults();s_command_queue->count=s_command_queue->capacity;
    assert(mosaic_ui_open_app("settings")==ESP_ERR_TIMEOUT);
    assert(s_command_queue->count==s_command_queue->capacity);
    s_command_queue->count=0;
    puts("Public App open (verbatim catalog/UI/loader): unknown/foreign rejection, zero-wait admission with stalled mutexes, deferred startup, asleep wake/recovery and queue-full failure passed");
}

int main(void)
{
    const mosaic_loader_config_t config={.presenter=&presenter,.producer_generation=1};
    s_screen_control_lock=xSemaphoreCreateMutex();
    assert(mosaic_loader_init(&config)==ESP_OK);
    assert(mosaic_loader_start_hub()==ESP_OK);
    test_input();reset_faults();test_status();test_capture_control();test_capture_boundary();test_capture_later_frames();test_full_device_geometry();test_public_ui();test_public_open_app();
    cleanup_partial_init();
    vSemaphoreDelete(s_screen_control_lock);
    assert(real_begins>0 && real_submits>0 && real_commits>0 && real_cancels>0);
    puts("UI diagnostics host regression passed (SDK/RTOS/panel fakes; no hardware acceptance)");
    return 0;
}
