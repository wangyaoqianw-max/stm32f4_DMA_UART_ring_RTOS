/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 * All Rights Reserved.
 * @file test_ui_sensor_monitor.c
 * @brief 用真实 LVGL 验证页面事件、格式、历史值、布局和内存。
 * @author YaoQian Wang
 * @date 2026-10-04
 * @version V1.0
 *****************************************************************************/
#include "ui_sensor_monitor.h"
#define UI_ADAPTER_TEST
#include "native_layout_probe.c"

#define TEST_ASSERT(condition) do { if (!(condition)) { printf("FAIL UI:%d\n", __LINE__); return 1; } } while (0)
static uint32_t g_requestCount;
static app_ctrl_event_t g_requestedEvent;
static platform_error_t g_submitResult;

static platform_error_t submit(void *context, app_ctrl_event_t event)
{
    (void)context;
    g_requestCount++;
    g_requestedEvent = event;
    return g_submitResult;
}

static uint32_t count_text(lv_obj_t *obj, const char *text)
{
    uint32_t index;
    uint32_t count = 0U;
    if (lv_obj_check_type(obj, &lv_label_class) && strcmp(lv_label_get_text(obj), text) == 0) {
        count++;
    }
    for (index = 0U; index < lv_obj_get_child_count(obj); index++) {
        count += count_text(lv_obj_get_child(obj, index), text);
    }
    return count;
}

static lv_obj_t *button_at(lv_obj_t *screen, int x)
{
    uint32_t index;
    for (index = 0U; index < lv_obj_get_child_count(screen); index++) {
        lv_obj_t *obj = lv_obj_get_child(screen, index);
        if (lv_obj_check_type(obj, &lv_button_class) && lv_obj_get_x(obj) == x) {
            return obj;
        }
    }
    return NULL;
}

int main(void)
{
    lv_display_t *display;
    lv_obj_t *screen;
    lv_obj_t *start;
    lv_obj_t *once;
    app_control_ui_status_t status = {0};
    app_acquisition_data_t data = {0};
    lv_mem_monitor_t memory;
    uint32_t index;

    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();
    display = lv_display_create(240, 280);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, g_drawBuffer, NULL, sizeof(g_drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    TEST_ASSERT(ui_sensor_monitor_create(submit, NULL) == PLATFORM_ERR_OK);
    screen = lv_screen_active();
    start = button_at(screen, 20);
    once = button_at(screen, 124);
    TEST_ASSERT(start && once);
    TEST_ASSERT(lv_obj_get_style_transition(start, LV_PART_MAIN) == NULL);
    TEST_ASSERT(lv_obj_get_style_transition(once, LV_PART_MAIN) == NULL);
    TEST_ASSERT(count_text(screen, "--") == 8);
    ui_sensor_monitor_show_sample_failure();
    TEST_ASSERT(count_text(screen, "--") == 8);
    TEST_ASSERT(count_text(screen, "Sample failed") == 1);
    lv_obj_send_event(start, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 1 && g_requestedEvent == APP_CTRL_START);
    TEST_ASSERT(count_text(screen, "START") == 1 && count_text(screen, "STOPPED") == 1);
    lv_obj_send_event(start, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    TEST_ASSERT(g_requestCount == 1);
    ui_sensor_monitor_update_control(&status, PLATFORM_TRUE);
    lv_obj_send_event(start, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 1);
    TEST_ASSERT(lv_obj_has_state(start, LV_STATE_DISABLED));
    status.state = APP_CONTROL_STATE_RUNNING;
    status.responseValid = PLATFORM_TRUE;
    status.response = APP_CONTROL_RESPONSE_OK_START;
    ui_sensor_monitor_update_control(&status, PLATFORM_FALSE);
    TEST_ASSERT(count_text(screen, "STOP") == 1 && count_text(screen, "RUNNING") == 1);
    TEST_ASSERT(lv_obj_has_state(once, LV_STATE_DISABLED));
    lv_obj_send_event(once, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 1);
    lv_obj_send_event(start, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 2 && g_requestedEvent == APP_CTRL_STOP);
    status.state = APP_CONTROL_STATE_STOPPED;
    status.onceActive = PLATFORM_TRUE;
    status.responseValid = PLATFORM_FALSE;
    ui_sensor_monitor_update_control(&status, PLATFORM_FALSE);
    TEST_ASSERT(lv_obj_has_state(start, LV_STATE_DISABLED) && lv_obj_has_state(once, LV_STATE_DISABLED));
    TEST_ASSERT(count_text(screen, "SAMPLING") == 1 && count_text(screen, "STOPPED") == 1);
    lv_obj_send_event(start, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 2);
    status.onceActive = PLATFORM_FALSE;
    status.responseValid = PLATFORM_TRUE;
    status.response = APP_CONTROL_RESPONSE_OK_ONCE;
    ui_sensor_monitor_update_control(&status, PLATFORM_FALSE);
    TEST_ASSERT(!lv_obj_has_state(start, LV_STATE_DISABLED) && !lv_obj_has_state(once, LV_STATE_DISABLED));
    TEST_ASSERT(count_text(screen, "Sample OK") == 1);
    lv_obj_send_event(once, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(g_requestCount == 3 && g_requestedEvent == APP_CTRL_SAMPLE_ONCE);
    g_submitResult = PLATFORM_ERR_FULL;
    lv_obj_send_event(once, LV_EVENT_CLICKED, NULL);
    TEST_ASSERT(count_text(screen, "Request failed") == 1);

    data.environment.temperatureC = -40.0F;
    data.environment.humidityPercent = 100.0F;
    data.motion.accelXG = -16.0F;
    data.motion.accelYG = 16.0F;
    data.motion.accelZG = 1.23F;
    data.motion.gyroXDps = -2000.0F;
    data.motion.gyroYDps = 2000.0F;
    data.motion.gyroZDps = -0.1F;
    ui_sensor_monitor_update_measurement(&data);
    TEST_ASSERT(count_text(screen, "-40.0") == 1 && count_text(screen, "100.0") == 1);
    TEST_ASSERT(count_text(screen, "-16.00") == 1 && count_text(screen, "+16.00") == 1);
    TEST_ASSERT(count_text(screen, "+1.23") == 1 && count_text(screen, "-2000.0") == 1);
    TEST_ASSERT(count_text(screen, "+2000.0") == 1 && count_text(screen, "-0.1") == 1);
    ui_sensor_monitor_show_sample_failure();
    TEST_ASSERT(count_text(screen, "Sample failed") == 1 && count_text(screen, "-40.0") == 1);
    TEST_ASSERT(count_text(screen, "-2000.0") == 1 && count_text(screen, "+1.23") == 1);
    lv_obj_update_layout(screen);
    TEST_ASSERT(labels_fit(screen) == 0);
    lv_refr_now(display);
    save_frame("sensor_monitor_maximum.ppm");
    data.environment.temperatureC = 125.0F;
    for (index = 0U; index < 100U; index++) {
        status.onceActive = index % 3U == 0U;
        status.state = status.onceActive != PLATFORM_TRUE && index % 2U ?
            APP_CONTROL_STATE_RUNNING : APP_CONTROL_STATE_STOPPED;
        status.responseValid = PLATFORM_FALSE;
        ui_sensor_monitor_update_control(&status, PLATFORM_FALSE);
        ui_sensor_monitor_update_measurement(&data);
        lv_obj_update_layout(screen);
        TEST_ASSERT(labels_fit(screen) == 0);
        lv_refr_now(display);
    }
    lv_mem_monitor(&memory);
    TEST_ASSERT(memory.free_size > 0 && g_flushCount > 0);
    printf("UI PASS: configured_pool=24576 usable=%lu free=%lu largest=%lu peak=%lu flushes=%u\n",
           (unsigned long)memory.total_size, (unsigned long)memory.free_size,
           (unsigned long)memory.free_biggest_size, (unsigned long)memory.max_used, g_flushCount);
    return 0;
}
