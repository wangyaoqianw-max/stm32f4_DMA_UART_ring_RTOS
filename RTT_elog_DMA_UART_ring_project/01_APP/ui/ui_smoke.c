/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ui_smoke.c
 * @brief 创建颜色、边界和触摸点击的临时验收页面
 * @author YaoQian Wang
 * @date 2026-10-03
 * @version V1.0
 *****************************************************************************/

#include "ui_smoke.h"

#include "lvgl.h"
#include "service_log.h"

#define LOG_TAG "ui_smoke"

static lv_obj_t *g_feedbackLabel;
static lv_obj_t *g_buttonLabel;
static uint32_t g_clickCount;

static void ui_smoke_on_click(lv_event_t *event)
{
    lv_mem_monitor_t memory;

    (void)event;
    g_clickCount++;
    lv_label_set_text_fmt(g_feedbackLabel, "Clicks: %lu", (unsigned long)g_clickCount);
    lv_label_set_text_fmt(g_buttonLabel, "TAPPED %lu", (unsigned long)g_clickCount);
    lv_obj_center(g_buttonLabel);
    SERVICE_LOG_I("click count=%lu", (unsigned long)g_clickCount);
    lv_mem_monitor(&memory);
    SERVICE_LOG_I("lvgl pool free=%lu peak=%lu biggest=%lu",
                  (unsigned long)memory.free_size,
                  (unsigned long)memory.max_used,
                  (unsigned long)memory.free_biggest_size);
}

platform_error_t ui_smoke_create(void)
{
    static const struct
    {
        lv_align_t align;
        uint32_t color;
        int32_t xOffset;
        int32_t yOffset;
    } markers[] = {
        {LV_ALIGN_TOP_LEFT, 0xFF0000U, 8, 8},
        {LV_ALIGN_TOP_RIGHT, 0x00FF00U, -8, 8},
        {LV_ALIGN_BOTTOM_LEFT, 0x0000FFU, 8, -8},
        {LV_ALIGN_BOTTOM_RIGHT, 0xFFFFFFU, -8, -8}
    };
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *object;
    lv_obj_t *button;
    uint32_t index;

    if (screen == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202020U), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xFFFFFFU), 0);
    for (index = 0U; index < sizeof(markers) / sizeof(markers[0]); index++) {
        object = lv_obj_create(screen);
        if (object == NULL) {
            return PLATFORM_ERR_NO_MEMORY;
        }
        lv_obj_set_size(object, 22, 22);
        lv_obj_align(object, markers[index].align,
            markers[index].xOffset, markers[index].yOffset);
        lv_obj_set_style_bg_color(object, lv_color_hex(markers[index].color), 0);
        lv_obj_set_style_border_width(object, 0, 0);
        lv_obj_set_style_radius(object, 0, 0);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    }

    object = lv_label_create(screen);
    if (object == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_label_set_text(object, "LVGL 9.4  RGB565");
    lv_obj_set_style_text_color(object, lv_color_hex(0xFFFFFFU), 0);
    lv_obj_align(object, LV_ALIGN_CENTER, 0, -65);

    button = lv_button_create(screen);
    if (button == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_obj_set_size(button, 120, 55);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(button, ui_smoke_on_click, LV_EVENT_CLICKED, NULL);
    g_buttonLabel = lv_label_create(button);
    if (g_buttonLabel == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    lv_label_set_text(g_buttonLabel, "TAP HERE");
    lv_obj_set_style_text_color(g_buttonLabel, lv_color_hex(0xFFFFFFU), 0);
    lv_obj_center(g_buttonLabel);

    g_feedbackLabel = lv_label_create(screen);
    if (g_feedbackLabel == NULL) {
        return PLATFORM_ERR_NO_MEMORY;
    }
    g_clickCount = 0U;
    lv_label_set_text(g_feedbackLabel, "Clicks: 0");
    lv_obj_set_style_text_color(g_feedbackLabel, lv_color_hex(0xFFFFFFU), 0);
    lv_obj_align(g_feedbackLabel, LV_ALIGN_CENTER, 0, 60);
    return PLATFORM_ERR_OK;
}
