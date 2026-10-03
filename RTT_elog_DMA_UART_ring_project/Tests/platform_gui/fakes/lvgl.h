/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file lvgl.h
 * @brief 提供 GUI 端口 Host 测试使用的 LVGL 替身声明
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#ifndef TEST_LVGL_H
#define TEST_LVGL_H

#include <stdint.h>

typedef struct {
    uint8_t unused;
} lv_display_t;
typedef struct {
    uint8_t unused;
} lv_indev_t;
typedef struct {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
} lv_area_t;
typedef struct {
    int32_t x;
    int32_t y;
} lv_point_t;
typedef struct {
    uint8_t state;
    lv_point_t point;
} lv_indev_data_t;
typedef void (*lv_display_flush_cb_t)(lv_display_t *, const lv_area_t *, uint8_t *);
typedef void (*lv_indev_read_cb_t)(lv_indev_t *, lv_indev_data_t *);

#define LV_DISPLAY_RENDER_MODE_PARTIAL 0
#define LV_COLOR_FORMAT_RGB565 1
#define LV_INDEV_TYPE_POINTER 1
#define LV_INDEV_STATE_RELEASED 0
#define LV_INDEV_STATE_PRESSED 1

void lv_init(void);
void lv_tick_set_cb(uint32_t (*callback)(void));
lv_display_t *lv_display_create(int32_t width, int32_t height);
void lv_display_set_color_format(lv_display_t *display, int colorFormat);
void lv_display_set_buffers(lv_display_t *display, void *first, void *second,
                            uint32_t bufferSize, int renderMode);
void lv_display_set_flush_cb(lv_display_t *display, lv_display_flush_cb_t callback);
void lv_display_flush_ready(lv_display_t *display);
lv_indev_t *lv_indev_create(void);
void lv_indev_set_type(lv_indev_t *indev, int type);
void lv_indev_set_read_cb(lv_indev_t *indev, lv_indev_read_cb_t callback);
uint32_t lv_timer_handler(void);

#endif
