/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display.c
 * @brief 实现 ST7789 Boot/Main UI 与 Display Queue 合并刷新
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_display.h"

#include "platform_font_ascii_8x16.h"
#include "platform_graphics.h"
#include "platform_time.h"
#include "service_log.h"

#include <stdio.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                         "app_display"
#define APP_DISPLAY_LINE_HEIGHT         (16U)
#define APP_DISPLAY_TITLE_X             (64U)
#define APP_DISPLAY_ENVIRONMENT_TITLE_X (76U)
#define APP_DISPLAY_ACCEL_TITLE_X       (84U)
#define APP_DISPLAY_GYRO_TITLE_X        (80U)
#define APP_DISPLAY_LABEL_X             (56U)
#define APP_DISPLAY_VALUE_X             (120U)
#define APP_DISPLAY_VALUE_WIDTH         (80U)
#define APP_DISPLAY_VALUE_TEXT_SIZE     (16U)
#define APP_DISPLAY_STATE_Y             (32U)
#define APP_DISPLAY_TEMP_Y              (80U)
#define APP_DISPLAY_HUMIDITY_Y          (96U)
#define APP_DISPLAY_ACCEL_X_Y           (144U)
#define APP_DISPLAY_ACCEL_Y_Y           (160U)
#define APP_DISPLAY_ACCEL_Z_Y           (176U)
#define APP_DISPLAY_GYRO_X_Y            (224U)
#define APP_DISPLAY_GYRO_Y_Y            (240U)
#define APP_DISPLAY_GYRO_Z_Y            (256U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static platform_error_t app_display_draw_text(
    app_display_t *appDisplay,
    uint16_t x,
    uint16_t y,
    const char *text)
{
    return platform_graphics_draw_string(
        appDisplay->config.display,
        x,
        y,
        text,
        &g_platformFontAscii8x16,
        PLATFORM_ST7789_COLOR_WHITE,
        PLATFORM_ST7789_COLOR_BLACK);
}

static platform_error_t app_display_draw_dynamic_text(
    app_display_t *appDisplay,
    uint16_t y,
    const char *text)
{
    platform_error_t result = platform_st7789_fill_rect(
        appDisplay->config.display,
        APP_DISPLAY_VALUE_X,
        y,
        APP_DISPLAY_VALUE_WIDTH,
        APP_DISPLAY_LINE_HEIGHT,
        PLATFORM_ST7789_COLOR_BLACK);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    return app_display_draw_text(appDisplay, APP_DISPLAY_VALUE_X, y, text);
}

static platform_error_t app_display_draw_boot_page(app_display_t *appDisplay)
{
    platform_error_t result = platform_st7789_fill(
        appDisplay->config.display, PLATFORM_ST7789_COLOR_BLACK);

    if (result == PLATFORM_ERR_OK) {
        result = app_display_draw_text(appDisplay, 64U, 80U, "SENSOR MONITOR");
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_draw_text(appDisplay, 64U, 112U, "STM32F4 + RTOS");
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_draw_text(appDisplay, 76U, 144U, "STARTING...");
    }
    return result;
}

static platform_error_t app_display_draw_main_layout(app_display_t *appDisplay)
{
    static const struct
    {
        uint16_t x;
        uint16_t y;
        const char *text;
    } lines[] = {
        {APP_DISPLAY_TITLE_X, 0U, "SENSOR MONITOR"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_STATE_Y, "STATE :"},
        {APP_DISPLAY_ENVIRONMENT_TITLE_X, 64U, "ENVIRONMENT"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_TEMP_Y, "TEMP  :"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_HUMIDITY_Y, "HUM   :"},
        {APP_DISPLAY_ACCEL_TITLE_X, 128U, "ACCEL (g)"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_ACCEL_X_Y, "X     :"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_ACCEL_Y_Y, "Y     :"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_ACCEL_Z_Y, "Z     :"},
        {APP_DISPLAY_GYRO_TITLE_X, 208U, "GYRO (dps)"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_GYRO_X_Y, "X     :"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_GYRO_Y_Y, "Y     :"},
        {APP_DISPLAY_LABEL_X, APP_DISPLAY_GYRO_Z_Y, "Z     :"}
    };
    platform_error_t result = platform_st7789_fill(
        appDisplay->config.display, PLATFORM_ST7789_COLOR_BLACK);
    platform_size_t index;

    for (index = 0U;
         (index < (sizeof(lines) / sizeof(lines[0]))) &&
         (result == PLATFORM_ERR_OK);
         index++) {
        result = app_display_draw_text(
            appDisplay, lines[index].x, lines[index].y, lines[index].text);
    }
    return result;
}

static platform_error_t app_display_format_value(
    char *buffer,
    platform_size_t bufferSize,
    const char *format,
    double value)
{
    int writtenLength = snprintf(buffer, bufferSize, format, value);

    if ((writtenLength < 0) || ((platform_size_t)writtenLength >= bufferSize)) {
        return PLATFORM_ERR_OVERFLOW;
    }
    return PLATFORM_ERR_OK;
}

static platform_error_t app_display_render_state(app_display_t *appDisplay)
{
    const char *text = "--";

    if (appDisplay->context.systemStateValid == PLATFORM_TRUE) {
        text = (appDisplay->context.systemState == APP_CONTROL_STATE_RUNNING) ?
            "RUNNING" : "STOPPED";
    }
    return app_display_draw_dynamic_text(
        appDisplay, APP_DISPLAY_STATE_Y, text);
}

static platform_error_t app_display_render_measurement(app_display_t *appDisplay)
{
    static const uint16_t valueY[] = {
        APP_DISPLAY_TEMP_Y,
        APP_DISPLAY_HUMIDITY_Y,
        APP_DISPLAY_ACCEL_X_Y,
        APP_DISPLAY_ACCEL_Y_Y,
        APP_DISPLAY_ACCEL_Z_Y,
        APP_DISPLAY_GYRO_X_Y,
        APP_DISPLAY_GYRO_Y_Y,
        APP_DISPLAY_GYRO_Z_Y
    };
    char values[8][APP_DISPLAY_VALUE_TEXT_SIZE] = {0};
    platform_error_t result = PLATFORM_ERR_OK;
    platform_size_t index;

    if (appDisplay->context.measurementValid != PLATFORM_TRUE) {
        for (index = 0U; index < 8U; index++) {
            result = app_display_draw_dynamic_text(appDisplay, valueY[index], "--");
            if (result != PLATFORM_ERR_OK) {
                return result;
            }
        }
        return PLATFORM_ERR_OK;
    }

    result = app_display_format_value(values[0], sizeof(values[0]), "%+.1f C",
        (double)appDisplay->context.latestMeasurement.environment.temperatureC);
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[1], sizeof(values[1]), "%5.1f %%",
            (double)appDisplay->context.latestMeasurement.environment.humidityPercent);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[2], sizeof(values[2]), "%+.3f",
            (double)appDisplay->context.latestMeasurement.motion.accelXG);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[3], sizeof(values[3]), "%+.3f",
            (double)appDisplay->context.latestMeasurement.motion.accelYG);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[4], sizeof(values[4]), "%+.3f",
            (double)appDisplay->context.latestMeasurement.motion.accelZG);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[5], sizeof(values[5]), "%+.1f",
            (double)appDisplay->context.latestMeasurement.motion.gyroXDps);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[6], sizeof(values[6]), "%+.1f",
            (double)appDisplay->context.latestMeasurement.motion.gyroYDps);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_format_value(values[7], sizeof(values[7]), "%+.1f",
            (double)appDisplay->context.latestMeasurement.motion.gyroZDps);
    }
    for (index = 0U; (index < 8U) && (result == PLATFORM_ERR_OK); index++) {
        result = app_display_draw_dynamic_text(
            appDisplay, valueY[index], values[index]);
    }
    return result;
}

static platform_error_t app_display_render_dirty(app_display_t *appDisplay)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (appDisplay->context.available != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }
    if (appDisplay->context.stateDirty == PLATFORM_TRUE) {
        result = app_display_render_state(appDisplay);
        if (result == PLATFORM_ERR_OK) {
            appDisplay->context.stateDirty = PLATFORM_FALSE;
        }
    }
    if ((result == PLATFORM_ERR_OK) &&
        (appDisplay->context.measurementDirty == PLATFORM_TRUE)) {
        result = app_display_render_measurement(appDisplay);
        if (result == PLATFORM_ERR_OK) {
            appDisplay->context.measurementDirty = PLATFORM_FALSE;
        }
    }
    if (result != PLATFORM_ERR_OK) {
        appDisplay->statistics.renderFailureCount++;
        SERVICE_LOG_W("dynamic render failed: %d", (int)result);
    }
    return PLATFORM_ERR_OK;
}

static platform_error_t app_display_update_cache(
    app_display_t *appDisplay,
    const app_display_message_t *message)
{
    switch (message->type) {
        case APP_DISPLAY_MESSAGE_SYSTEM_STATE:
            if (message->payload.systemState >= APP_CONTROL_STATE_MAX) {
                return PLATFORM_ERR_INVALID_PARAM;
            }
            appDisplay->context.systemState = message->payload.systemState;
            appDisplay->context.systemStateValid = PLATFORM_TRUE;
            appDisplay->context.stateDirty = PLATFORM_TRUE;
            return PLATFORM_ERR_OK;

        case APP_DISPLAY_MESSAGE_MEASUREMENT:
            appDisplay->context.latestMeasurement = message->payload.measurement;
            appDisplay->context.measurementValid = PLATFORM_TRUE;
            appDisplay->context.measurementDirty = PLATFORM_TRUE;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t app_display_drain_pending(app_display_t *appDisplay)
{
    app_display_message_t message;
    platform_error_t result;

    for (;;) {
        result = platform_queue_receive(
            appDisplay->config.queue, &message, PLATFORM_OS_NO_WAIT);
        if ((result == PLATFORM_ERR_EMPTY) || (result == PLATFORM_ERR_TIMEOUT)) {
            return PLATFORM_ERR_OK;
        }
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
        result = app_display_update_cache(appDisplay, &message);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
        appDisplay->statistics.processedMessageCount++;
        appDisplay->statistics.coalescedMessageCount++;
    }
}

static void app_display_disable(app_display_t *appDisplay, platform_error_t error)
{
    (void)platform_st7789_backlight_off(appDisplay->config.display);
    if (appDisplay->config.display->initialized == PLATFORM_TRUE) {
        (void)platform_st7789_deinit(appDisplay->config.display);
    }
    appDisplay->context.available = PLATFORM_FALSE;
    appDisplay->statistics.startupFailureCount++;
    SERVICE_LOG_E("display startup failed: %d", (int)error);
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_display_init(
    app_display_t *appDisplay,
    const app_display_config_t *config)
{
    if ((appDisplay == NULL) || (config == NULL) ||
        (config->display == NULL) || (config->spiBus == NULL) ||
        (config->queue == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (appDisplay->context.initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if (config->queue->native == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    appDisplay->config = *config;
    appDisplay->context.systemState = APP_CONTROL_STATE_STOPPED;
    appDisplay->context.initialized = PLATFORM_TRUE;
    appDisplay->context.available = PLATFORM_FALSE;
    return PLATFORM_ERR_OK;
}

platform_error_t app_display_start(app_display_t *appDisplay)
{
    platform_error_t result;

    if (appDisplay == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (appDisplay->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_st7789_init(
        appDisplay->config.display, appDisplay->config.spiBus);
    if (result != PLATFORM_ERR_OK) {
        app_display_disable(appDisplay, result);
        return PLATFORM_ERR_OK;
    }
    result = app_display_draw_boot_page(appDisplay);
    if (result == PLATFORM_ERR_OK) {
        result = platform_st7789_backlight_on(appDisplay->config.display);
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_time_delay_ms(PROJECT_DISPLAY_BOOT_DURATION_MS);
    }
    if (result == PLATFORM_ERR_OK) {
        result = app_display_draw_main_layout(appDisplay);
    }
    if (result != PLATFORM_ERR_OK) {
        app_display_disable(appDisplay, result);
        return PLATFORM_ERR_OK;
    }

    appDisplay->context.available = PLATFORM_TRUE;
    appDisplay->context.stateDirty = PLATFORM_TRUE;
    appDisplay->context.measurementDirty = PLATFORM_TRUE;
    result = app_display_drain_pending(appDisplay);
    if (result == PLATFORM_ERR_OK) {
        (void)app_display_render_dirty(appDisplay);
    }
    return result;
}

platform_error_t app_display_run_once(app_display_t *appDisplay)
{
    app_display_message_t message;
    platform_error_t result;

    if (appDisplay == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (appDisplay->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_queue_receive(
        appDisplay->config.queue, &message, PLATFORM_OS_WAIT_FOREVER);
    if ((result == PLATFORM_ERR_TIMEOUT) || (result == PLATFORM_ERR_EMPTY)) {
        return PLATFORM_ERR_OK;
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    result = app_display_update_cache(appDisplay, &message);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    appDisplay->statistics.processedMessageCount++;

    result = app_display_drain_pending(appDisplay);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    return app_display_render_dirty(appDisplay);
}

void app_display_task_entry(void *argument)
{
    app_display_t *appDisplay = (app_display_t *)argument;

    if (appDisplay == NULL) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    (void)app_display_start(appDisplay);
    for (;;) {
        if (app_display_run_once(appDisplay) != PLATFORM_ERR_OK) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }
}
//******************************** Functions ********************************//
