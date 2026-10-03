/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_platform_gui.c
 * @brief 验证 LVGL 显示、触摸与时基端口的行为
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "lvgl.h"
#include "platform_st7789.h"
#include "platform_cst816t.h"
#include "platform_time.h"

#include <stdio.h>
#include <string.h>

platform_error_t platform_gui_init(platform_st7789_t *display,
                                   const platform_cst816t_sample_t *touchSample);
platform_error_t platform_gui_process(void);

#define TEST_ASSERT(condition) do { if (!(condition)) { return __LINE__; } } while (0)

static lv_display_t g_lvDisplay;
static lv_indev_t g_lvIndev;
static lv_display_flush_cb_t g_flushCallback;
static lv_indev_read_cb_t g_readCallback;
static uint32_t (*g_tickCallback)(void);
static platform_cst816t_sample_t g_sample;
static platform_error_t g_writeResult;
static platform_error_t g_timeResult;
static uint32_t g_timeMs;
static uint32_t g_initCount;
static uint32_t g_readyCount;
static uint32_t g_writeCount;
static uint32_t g_bufferSize;
static uint32_t g_pixelCount;
static uint16_t g_x;
static uint16_t g_y;
static uint16_t g_width;
static uint16_t g_height;
static const uint16_t *g_pixels;
static uint8_t g_flushOnTimer;
static lv_area_t g_flushArea;
static uint16_t g_drawBuffer[12];

static void reset_fakes(void)
{
    g_flushCallback = NULL;
    g_readCallback = NULL;
    g_tickCallback = NULL;
    (void)memset(&g_sample, 0, sizeof(g_sample));
    g_writeResult = PLATFORM_ERR_OK;
    g_timeResult = PLATFORM_ERR_OK;
    g_timeMs = 1234U;
    g_initCount = 0U;
    g_readyCount = 0U;
    g_writeCount = 0U;
    g_bufferSize = 0U;
    g_pixelCount = 0U;
    g_flushOnTimer = 0U;
}

void lv_init(void)
{
    g_initCount++;
}

void lv_tick_set_cb(uint32_t (*callback)(void))
{
    g_tickCallback = callback;
}
lv_display_t *lv_display_create(int32_t width, int32_t height)
{
    return (width == 240 && height == 280) ? &g_lvDisplay : NULL;
}
void lv_display_set_color_format(lv_display_t *display, int colorFormat)
{
    (void)display;
    (void)colorFormat;
}
void lv_display_set_buffers(lv_display_t *display, void *first, void *second,
                            uint32_t bufferSize, int renderMode)
{
    (void)display;
    (void)first;
    (void)second;
    (void)renderMode;
    g_bufferSize = bufferSize;
}
void lv_display_set_flush_cb(lv_display_t *display, lv_display_flush_cb_t callback)
{
    (void)display;
    g_flushCallback = callback;
}
void lv_display_flush_ready(lv_display_t *display)
{
    (void)display;
    g_readyCount++;
}
lv_indev_t *lv_indev_create(void)
{
    return &g_lvIndev;
}

void lv_indev_set_type(lv_indev_t *indev, int type)
{
    (void)indev;
    (void)type;
}
void lv_indev_set_read_cb(lv_indev_t *indev, lv_indev_read_cb_t callback)
{
    (void)indev;
    g_readCallback = callback;
}
uint32_t lv_timer_handler(void)
{
    if (g_flushOnTimer != 0U) {
        g_flushOnTimer = 0U;
        g_flushCallback(&g_lvDisplay, &g_flushArea, (uint8_t *)g_drawBuffer);
    }
    return 5U;
}

platform_error_t platform_time_get_ms(uint32_t *timeMs)
{
    if (g_timeResult == PLATFORM_ERR_OK) {
        *timeMs = g_timeMs;
    }
    return g_timeResult;
}

platform_error_t platform_st7789_write_rgb565(platform_st7789_t *display,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height,
    const uint16_t *pixels, platform_size_t pixelCount)
{
    (void)display;
    g_writeCount++;
    g_x = x;
    g_y = y;
    g_width = width;
    g_height = height;
    g_pixels = pixels;
    g_pixelCount = pixelCount;
    return g_writeResult;
}

static int test_init_registers_display_input_and_tick(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;

    reset_fakes();
    TEST_ASSERT(platform_gui_init(&display, &g_sample) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_initCount == 1U);
    TEST_ASSERT(g_bufferSize == 9600U);
    TEST_ASSERT(g_flushCallback != NULL);
    TEST_ASSERT(g_readCallback != NULL);
    TEST_ASSERT(g_tickCallback != NULL);
    TEST_ASSERT(g_tickCallback() == 1234U);
    return 0;
}

static int test_flush_uses_inclusive_area_and_completes(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;

    reset_fakes();
    TEST_ASSERT(platform_gui_init(&display, &g_sample) == PLATFORM_ERR_OK);
    g_flushArea = (lv_area_t){10, 20, 12, 21};
    g_flushOnTimer = 1U;
    TEST_ASSERT(platform_gui_process() == PLATFORM_ERR_OK);
    TEST_ASSERT(g_writeCount == 1U);
    TEST_ASSERT(g_x == 10U && g_y == 20U);
    TEST_ASSERT(g_width == 3U && g_height == 2U);
    TEST_ASSERT(g_pixelCount == 6U);
    TEST_ASSERT(g_pixels == g_drawBuffer);
    TEST_ASSERT(g_readyCount == 1U);
    return 0;
}

static int test_failed_flush_still_completes_and_reports_once(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;

    reset_fakes();
    TEST_ASSERT(platform_gui_init(&display, &g_sample) == PLATFORM_ERR_OK);
    g_flushArea = (lv_area_t){0, 0, 1, 1};
    g_writeResult = PLATFORM_ERR_IO;
    g_flushOnTimer = 1U;
    TEST_ASSERT(platform_gui_process() == PLATFORM_ERR_IO);
    TEST_ASSERT(g_readyCount == 1U);
    TEST_ASSERT(platform_gui_process() == PLATFORM_ERR_OK);
    return 0;
}

static int test_pointer_clamps_press_and_keeps_release_position(void)
{
    platform_st7789_t display = PLATFORM_ST7789_INITIALIZER;
    lv_indev_data_t data = {0};

    reset_fakes();
    TEST_ASSERT(platform_gui_init(&display, &g_sample) == PLATFORM_ERR_OK);
    g_sample.pressed = PLATFORM_TRUE;
    g_sample.x = 500U;
    g_sample.y = 600U;
    g_readCallback(&g_lvIndev, &data);
    TEST_ASSERT(data.state == LV_INDEV_STATE_PRESSED);
    TEST_ASSERT(data.point.x == 239 && data.point.y == 279);
    g_sample.pressed = PLATFORM_FALSE;
    g_sample.x = 0U;
    g_sample.y = 0U;
    g_readCallback(&g_lvIndev, &data);
    TEST_ASSERT(data.state == LV_INDEV_STATE_RELEASED);
    TEST_ASSERT(data.point.x == 239 && data.point.y == 279);
    return 0;
}

int main(void)
{
    int result;

    result = test_init_registers_display_input_and_tick();
    if (result == 0) {
        result = test_flush_uses_inclusive_area_and_completes();
    }
    if (result == 0) {
        result = test_failed_flush_still_completes_and_reports_once();
    }
    if (result == 0) {
        result = test_pointer_clamps_press_and_keeps_release_position();
    }
    if (result != 0) {
        (void)printf("test_platform_gui failed at line %d\n", result);
        return 1;
    }
    (void)puts("test_platform_gui PASS");
    return 0;
}
