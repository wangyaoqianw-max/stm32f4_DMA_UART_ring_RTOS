/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display.c
 * @brief 实现 Display Queue 缓存与 LVGL 临时页面驱动
 * @author YaoQian Wang
 * @date 2026-09-06
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_display.h"

#include "platform_gui.h"
#include "platform_time.h"
#include "service_log.h"
#include "ui_smoke.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_DISPLAY_TOUCH_NOTIFY (1UL << 0U)
#define LOG_TAG                         "app_display"
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
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
            return PLATFORM_ERR_OK;

        case APP_DISPLAY_MESSAGE_MEASUREMENT:
            appDisplay->context.latestMeasurement = message->payload.measurement;
            appDisplay->context.measurementValid = PLATFORM_TRUE;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

static platform_error_t app_display_drain_pending(app_display_t *appDisplay, uint32_t budget)
{
    app_display_message_t message;
    platform_error_t result;

    uint32_t index;

    for (index = 0U; index < budget; index++) {
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
    return PLATFORM_ERR_OK;
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
static void app_display_read_touch(app_display_t *appDisplay)
{
    platform_cst816t_sample_t sample = {PLATFORM_FALSE, 0U, 0U};
    platform_cst816t_sample_t previous = appDisplay->context.touchSample;
    platform_error_t result = platform_cst816t_read_sample(appDisplay->config.touch, &sample);
    uint32_t nowMs = 0U;

    if (result != PLATFORM_ERR_OK) {
        if (appDisplay->context.touchLastError != result) {
            SERVICE_LOG_W("touch read failed: %d", (int)result);
        }
        sample.pressed = PLATFORM_FALSE;
        sample.x = 0U;
        sample.y = 0U;
    }
    appDisplay->context.touchLastError = result;
    appDisplay->context.touchSample = sample;
    if (platform_time_get_ms(&nowMs) != PLATFORM_ERR_OK) {
        return;
    }
    if (sample.pressed != previous.pressed) {
        SERVICE_LOG_I("touch %s x=%u y=%u", sample.pressed ? "DOWN" : "UP",
            (unsigned)sample.x, (unsigned)sample.y);
        appDisplay->context.touchLastLogMs = nowMs;
    } else if ((sample.pressed == PLATFORM_TRUE) &&
        ((sample.x != previous.x) || (sample.y != previous.y)) &&
        ((uint32_t)(nowMs - appDisplay->context.touchLastLogMs) >= PROJECT_TOUCH_MOVE_LOG_PERIOD_MS)) {
        SERVICE_LOG_I("touch MOVE x=%u y=%u", (unsigned)sample.x, (unsigned)sample.y);
        appDisplay->context.touchLastLogMs = nowMs;
    }
}

static void app_display_start_touch(app_display_t *appDisplay)
{
    platform_error_t result = platform_i2c_init(appDisplay->config.touchI2c,
        "touch_soft_i2c", appDisplay->config.touchScl, appDisplay->config.touchSda);

    if (result == PLATFORM_ERR_OK) {
        result = platform_cst816t_init(appDisplay->config.touch,
            appDisplay->config.touchI2c, appDisplay->config.touchReset);
    }
    appDisplay->context.touchLastError = result;
    appDisplay->context.touchAvailable = (result == PLATFORM_ERR_OK) ? PLATFORM_TRUE : PLATFORM_FALSE;
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_W("touch startup failed: %d", (int)result);
        return;
    }
    SERVICE_LOG_I("touch ready id=0x%02X fw=0x%02X", (unsigned)appDisplay->config.touch->chipId,
        (unsigned)appDisplay->config.touch->firmwareVersion);
    /* EXTI在任务创建前已启用，主动采样补足初始化期间丢弃的通知。 */
    app_display_read_touch(appDisplay);
}

static void app_display_service_touch(app_display_t *appDisplay)
{
    uint32_t flags = 0U;
    platform_error_t result = platform_notify_wait(APP_DISPLAY_TOUCH_NOTIFY,
        PLATFORM_FALSE, PLATFORM_TRUE, PLATFORM_OS_NO_WAIT, &flags);

    if ((result == PLATFORM_ERR_EMPTY) || (result == PLATFORM_ERR_TIMEOUT)) {
        return;
    }
    if (result != PLATFORM_ERR_OK) {
        appDisplay->context.touchSample.pressed = PLATFORM_FALSE;
        appDisplay->context.touchSample.x = 0U;
        appDisplay->context.touchSample.y = 0U;
        if (result != appDisplay->context.touchLastError) {
            SERVICE_LOG_W("touch notify failed: %d", (int)result);
        }
        appDisplay->context.touchLastError = result;
        return;
    }
    if (((flags & APP_DISPLAY_TOUCH_NOTIFY) != 0U) &&
        (appDisplay->context.touchAvailable == PLATFORM_TRUE)) {
        app_display_read_touch(appDisplay);
    }
}

//******************************** Private Functions ************************//

//******************************** Functions ********************************//
platform_error_t app_display_init(
    app_display_t *appDisplay,
    const app_display_config_t *config)
{
    if ((appDisplay == NULL) || (config == NULL) ||
        (config->display == NULL) || (config->spiBus == NULL) ||
        (config->queue == NULL) || (config->touch == NULL) ||
        (config->touchI2c == NULL) || (config->touchScl == NULL) ||
        (config->touchSda == NULL) || (config->touchReset == NULL) || (config->thread == NULL)) {
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

    app_display_start_touch(appDisplay);
    result = platform_st7789_init(
        appDisplay->config.display, appDisplay->config.spiBus);
    if (result != PLATFORM_ERR_OK) {
        app_display_disable(appDisplay, result);
        return PLATFORM_ERR_OK;
    }
    result = platform_gui_init(appDisplay->config.display,
        &appDisplay->context.touchSample);
    if (result == PLATFORM_ERR_OK) {
        result = ui_smoke_create();
    }
    if (result == PLATFORM_ERR_OK) {
        result = platform_st7789_backlight_on(appDisplay->config.display);
    }
    if (result != PLATFORM_ERR_OK) {
        app_display_disable(appDisplay, result);
        return PLATFORM_ERR_OK;
    }

    appDisplay->context.available = PLATFORM_TRUE;
    return app_display_drain_pending(appDisplay, PROJECT_DISPLAY_MESSAGE_BUDGET);
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
        appDisplay->config.queue, &message, PROJECT_DISPLAY_WAIT_TIMEOUT_MS);
    if (result == PLATFORM_ERR_OK) {
        result = app_display_update_cache(appDisplay, &message);
        if (result == PLATFORM_ERR_OK) {
            appDisplay->statistics.processedMessageCount++;
            result = app_display_drain_pending(appDisplay, PROJECT_DISPLAY_MESSAGE_BUDGET - 1U);
        }
    } else if ((result == PLATFORM_ERR_TIMEOUT) || (result == PLATFORM_ERR_EMPTY)) {
        result = PLATFORM_ERR_OK;
    }
    app_display_service_touch(appDisplay);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (appDisplay->context.available == PLATFORM_TRUE) {
        platform_error_t guiResult = platform_gui_process();

        if (guiResult != PLATFORM_ERR_OK) {
            appDisplay->statistics.renderFailureCount++;
            if (guiResult != appDisplay->context.guiLastError) {
                SERVICE_LOG_W("gui flush failed: %d", (int)guiResult);
            }
        }
        appDisplay->context.guiLastError = guiResult;
    }
    return PLATFORM_ERR_OK;
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

void app_display_touch_irq_from_isr(app_display_t *appDisplay)
{
    platform_error_t result;

    if ((appDisplay == NULL) || (appDisplay->context.initialized != PLATFORM_TRUE) ||
        (appDisplay->config.thread == NULL) || (appDisplay->config.thread->native == NULL)) {
        return;
    }
    result = platform_notify_set_from_isr(appDisplay->config.thread, APP_DISPLAY_TOUCH_NOTIFY);
    if (result != PLATFORM_ERR_OK) {
        /* ISR只写诊断计数，避免在中断内记录日志或访问I²C。 */
        appDisplay->statistics.touchNotifyFailureCount++;
    }
}
