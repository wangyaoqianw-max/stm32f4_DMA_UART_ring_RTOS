/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_gui.c
 * @brief 实现 LVGL 9.4 的同步 ST7789、缓存触摸和 RTOS 时基端口
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "platform_gui.h"

#include "platform_time.h"
#include "project_config.h"

#include "lvgl.h"

#define PLATFORM_GUI_DRAW_ROWS (20U)
#define PLATFORM_GUI_PIXEL_COUNT (PROJECT_DISPLAY_WIDTH * PLATFORM_GUI_DRAW_ROWS)

static uint16_t g_drawBuffer[PLATFORM_GUI_PIXEL_COUNT];
static platform_st7789_t *g_display;
static const platform_cst816t_sample_t *g_touchSample;
static platform_error_t g_flushError;
static uint32_t g_lastTickMs;
static int32_t g_lastTouchX;
static int32_t g_lastTouchY;

static uint32_t platform_gui_get_tick_ms(void)
{
    uint32_t nowMs = 0U;

    if (platform_time_get_ms(&nowMs) == PLATFORM_ERR_OK) {
        g_lastTickMs = nowMs;
    }
    return g_lastTickMs;
}


/* Temporary diagnostic build only; sampled by SWD without halting the CPU. */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
volatile uint32_t g_uiDiagnostics[20];
static uint32_t g_diagFlushCount;
static uint32_t g_diagFlushMax;
static uint32_t g_diagFlushCycles;
static uint32_t g_diagPixels;
static uint32_t g_diagProcessCount;
static uint32_t g_diagProcessMax;
static uint32_t g_diagProcessCycles;
static uint32_t g_diagLastSample;
static uint32_t g_diagFlushErrors;
static void diag_sample(void)
{
    lv_mem_monitor_t memory;
    uint32_t now = platform_gui_get_tick_ms();
    if ((uint32_t)(now - g_diagLastSample) < 1000U) return;
    g_diagLastSample = now;
    lv_mem_monitor(&memory);
    g_uiDiagnostics[0]++;
    __DMB();
    g_uiDiagnostics[1] = now;
    g_uiDiagnostics[2] = SystemCoreClock;
    g_uiDiagnostics[3] = memory.total_size;
    g_uiDiagnostics[4] = memory.free_size;
    g_uiDiagnostics[5] = memory.free_biggest_size;
    g_uiDiagnostics[6] = memory.max_used;
    g_uiDiagnostics[7] = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    g_uiDiagnostics[8] = xPortGetFreeHeapSize();
    g_uiDiagnostics[9] = xPortGetMinimumEverFreeHeapSize();
    g_uiDiagnostics[10] = g_diagFlushCount;
    g_uiDiagnostics[11] = g_diagFlushMax;
    g_uiDiagnostics[12] = g_diagFlushCycles;
    g_uiDiagnostics[13] = g_diagPixels;
    g_uiDiagnostics[14] = g_diagProcessCount;
    g_uiDiagnostics[15] = g_diagProcessMax;
    g_uiDiagnostics[16] = g_diagProcessCycles;
    g_uiDiagnostics[17] = g_diagFlushErrors;
    __DMB();
    g_uiDiagnostics[0]++;
}

static void platform_gui_flush(lv_display_t *lvDisplay,
                               const lv_area_t *area,
                               uint8_t *pixelMap)
{
    uint32_t startCycles = DWT->CYCCNT;
    uint32_t elapsedCycles;
    uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);
    platform_error_t result = platform_st7789_write_rgb565(
        g_display,
        (uint16_t)area->x1,
        (uint16_t)area->y1,
        width,
        height,
        (const uint16_t *)pixelMap,
        (platform_size_t)width * height);

    elapsedCycles = DWT->CYCCNT - startCycles;
    g_diagFlushCount++;
    g_diagFlushCycles += elapsedCycles;
    g_diagPixels += (uint32_t)width * height;
    if (elapsedCycles > g_diagFlushMax) g_diagFlushMax = elapsedCycles;
    if (result != PLATFORM_ERR_OK) g_diagFlushErrors++;
    if ((result != PLATFORM_ERR_OK) && (g_flushError == PLATFORM_ERR_OK)) {
        g_flushError = result;
    }
    /* 同步 SPI 已结束；失败时也释放绘制缓冲，避免 LVGL 永久等待。 */
    lv_display_flush_ready(lvDisplay);
}

static void platform_gui_read_touch(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    if (g_touchSample->pressed == PLATFORM_TRUE) {
        g_lastTouchX = g_touchSample->x < PROJECT_DISPLAY_WIDTH ?
            (int32_t)g_touchSample->x : (int32_t)(PROJECT_DISPLAY_WIDTH - 1U);
        g_lastTouchY = g_touchSample->y < PROJECT_DISPLAY_HEIGHT ?
            (int32_t)g_touchSample->y : (int32_t)(PROJECT_DISPLAY_HEIGHT - 1U);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    /* 释放时沿用最后一次按下坐标，避免事件被误判为移到原点。 */
    data->point.x = g_lastTouchX;
    data->point.y = g_lastTouchY;
}

platform_error_t platform_gui_init(platform_st7789_t *display,
                                   const platform_cst816t_sample_t *touchSample)
{
    lv_display_t *lvDisplay;
    lv_indev_t *lvIndev;
    platform_error_t result;

    if ((display == NULL) || (touchSample == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    result = platform_time_get_ms(&g_lastTickMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_display = display;
    g_touchSample = touchSample;
    g_flushError = PLATFORM_ERR_OK;
    g_lastTouchX = 0;
    g_lastTouchY = 0;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    lv_init();
    lv_tick_set_cb(platform_gui_get_tick_ms);
    lvDisplay = lv_display_create(PROJECT_DISPLAY_WIDTH, PROJECT_DISPLAY_HEIGHT);
    if (lvDisplay == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(lvDisplay, g_drawBuffer, NULL, sizeof(g_drawBuffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay, platform_gui_flush);

    lvIndev = lv_indev_create();
    if (lvIndev == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_indev_set_type(lvIndev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvIndev, platform_gui_read_touch);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_gui_process(void)
{
    uint32_t startCycles = DWT->CYCCNT;
    uint32_t elapsedCycles;
    g_flushError = PLATFORM_ERR_OK;
    (void)lv_timer_handler();
    elapsedCycles = DWT->CYCCNT - startCycles;
    g_diagProcessCount++;
    g_diagProcessCycles += elapsedCycles;
    if (elapsedCycles > g_diagProcessMax) g_diagProcessMax = elapsedCycles;
    diag_sample();
    return g_flushError;
}
