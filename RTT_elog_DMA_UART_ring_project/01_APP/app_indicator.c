/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_indicator.c
 * @brief 实现 Indicator Queue 到 Indicator Service 的语义映射。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_indicator.h"

#include "platform_time.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
/** @brief 将 APP 指示命令映射为 Indicator Service 事件。 */
static platform_error_t app_indicator_map_event(
    app_indicator_command_t command,
    service_indicator_event_t *event)
{
    switch (command) {
        case APP_INDICATOR_STOPPED:
            *event = SERVICE_INDICATOR_EVENT_STOPPED;
            return PLATFORM_ERR_OK;

        case APP_INDICATOR_RUNNING:
            *event = SERVICE_INDICATOR_EVENT_RUNNING;
            return PLATFORM_ERR_OK;

        case APP_INDICATOR_ONCE_SUCCESS:
            *event = SERVICE_INDICATOR_EVENT_ONCE_SUCCESS;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_indicator_init(
    app_indicator_t *indicator,
    const app_indicator_config_t *config)
{
    if ((indicator == NULL) || (config == NULL) ||
        (config->service == NULL) || (config->queue == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (indicator->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if ((config->service->initialized != PLATFORM_TRUE) ||
        (config->queue->native == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    indicator->config = *config;
    indicator->initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t app_indicator_run_once(app_indicator_t *indicator)
{
    app_indicator_command_t command = APP_INDICATOR_MAX;
    service_indicator_event_t event = SERVICE_INDICATOR_EVENT_STOPPED;
    platform_error_t result;

    if (indicator == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (indicator->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_queue_receive(
        indicator->config.queue, &command, PLATFORM_OS_WAIT_FOREVER);
    if (result != PLATFORM_ERR_OK) {
        indicator->failureCount++;
        return result;
    }
    result = app_indicator_map_event(command, &event);
    if (result != PLATFORM_ERR_OK) {
        indicator->failureCount++;
        return result;
    }
    result = service_indicator_handle_event(indicator->config.service, event);
    if (result != PLATFORM_ERR_OK) {
        indicator->failureCount++;
        return result;
    }

    indicator->handledEventCount++;
    return PLATFORM_ERR_OK;
}

void app_indicator_task_entry(void *argument)
{
    app_indicator_t *indicator = (app_indicator_t *)argument;

    if (indicator == NULL) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    for (;;) {
        if (app_indicator_run_once(indicator) != PLATFORM_ERR_OK) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }
}
//******************************** Functions *********************************//
