/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 * All Rights Reserved.
 * @file native_layout_probe.c
 * @brief 验证原生生成页面，并提供 Host RGB565 渲染和布局检查。
 * @author YaoQian Wang
 * @date 2026-10-04
 * @version V1.0
 *****************************************************************************/
#include "gg_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void host_lvgl_assert(const char *file, int line)
{
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    fprintf(stderr, "LVGL ASSERT %s:%d free=%lu largest=%lu peak=%lu\n", file, line,
            (unsigned long)memory.free_size, (unsigned long)memory.free_biggest_size,
            (unsigned long)memory.max_used);
    abort();
}

static uint8_t g_drawBuffer[9600];
static uint16_t g_frame[240 * 280];
static uint32_t g_flushCount;

static void flush(lv_display_t *display, const lv_area_t *area, unsigned char *pixels)
{
    int32_t x;
    int32_t y;
    uint16_t *source = (uint16_t *)pixels;
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            g_frame[y * 240 + x] = *source++;
        }
    }
    g_flushCount++;
    lv_display_flush_ready(display);
}

static void save_frame(const char *path)
{
    uint32_t index;
    FILE *file = fopen(path, "wb");
    fprintf(file, "P6\n240 280\n255\n");
    for (index = 0U; index < 240U * 280U; index++) {
        uint16_t pixel = g_frame[index];
        uint8_t rgb[3] = {(pixel >> 11) * 255 / 31,
                               ((pixel >> 5) & 63) * 255 / 63, (pixel & 31) * 255 / 31};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

static int labels_fit(lv_obj_t *obj)
{
    uint32_t index;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
        const unsigned char *text = (const unsigned char *)lv_label_get_text(obj);
        while (*text != 0U) {
            lv_font_glyph_dsc_t glyph;
            if (!font->get_glyph_dsc(font, &glyph, *text, 0U)) {
                printf("MISSING GLYPH %u in %s\n", (unsigned int)*text, lv_label_get_text(obj));
                return 1;
            }
            text++;
        }
        lv_text_get_size(&size, lv_label_get_text(obj), lv_obj_get_style_text_font(obj, 0),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_content_width(obj) || size.y > lv_obj_get_content_height(obj)) {
            printf("CLIPPED %s: text=%dx%d content=%dx%d\n", lv_label_get_text(obj),
                   (int)size.x, (int)size.y, (int)lv_obj_get_content_width(obj),
                   (int)lv_obj_get_content_height(obj));
            return 1;
        }
    }
    for (index = 0U; index < lv_obj_get_child_count(obj); index++) {
        if (labels_fit(lv_obj_get_child(obj, index))) {
            return 1;
        }
    }
    return 0;
}

#ifndef UI_ADAPTER_TEST
int main(void)
{
    gg_ui_t ui = {0};
    lv_mem_monitor_t memory;
    lv_display_t *display;
    lv_init();
    display = lv_display_create(240, 280);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, g_drawBuffer, NULL, sizeof(g_drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    setup_ui(&ui);
    /* 对应已更新的原生设计；导出的旧布局由 APP 适配同一修正。 */
    lv_obj_set_y(ui.screen.container_temperature_label_temperature_value, 20);
    lv_obj_set_height(ui.screen.container_temperature_label_temperature_value, 29);
    lv_obj_set_y(ui.screen.container_humidity_label_humidity_value, 20);
    lv_obj_set_height(ui.screen.container_humidity_label_humidity_value, 29);
    lv_refr_now(display);
    save_frame("guider_native_initial.ppm");
    lv_label_set_text(ui.screen.container_temperature_label_temperature_value, "-40.0");
    lv_label_set_text(ui.screen.container_humidity_label_humidity_value, "100.0");
    lv_label_set_text(ui.screen.container_acceleration_label_acceleration_x, "-16.00");
    lv_label_set_text(ui.screen.container_acceleration_label_acceleration_y, "+16.00");
    lv_label_set_text(ui.screen.container_acceleration_label_acceleration_z, "-16.00");
    lv_label_set_text(ui.screen.container_gyroscope_label_gyroscope_x, "-2000.0");
    lv_label_set_text(ui.screen.container_gyroscope_label_gyroscope_y, "+2000.0");
    lv_label_set_text(ui.screen.container_gyroscope_label_gyroscope_z, "-2000.0");
    lv_obj_set_style_opa(ui.screen.button_start_stop, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_opa(ui.screen.button_once, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_add_state(ui.screen.button_start_stop, LV_STATE_DISABLED);
    lv_obj_add_state(ui.screen.button_once, LV_STATE_DISABLED);
    lv_label_set_text(ui.screen.button_once_button_once_label, "SAMPLING");
    lv_obj_update_layout(ui.screen.screen);
    if (labels_fit(ui.screen.screen)) {
        return 1;
    }
    lv_refr_now(display);
    save_frame("guider_native_maximum.ppm");
    lv_mem_monitor(&memory);
    printf("Native page PASS: pool=%lu free=%lu largest=%lu peak=%lu flushes=%u\n",
           (unsigned long)memory.total_size, (unsigned long)memory.free_size,
           (unsigned long)memory.free_biggest_size, (unsigned long)memory.max_used, g_flushCount);
    return memory.free_size > 0 && g_flushCount > 0 ? 0 : 1;
}
#endif
