/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_control.c
 * @brief 实现唯一 APP Control FSM 与 deadline 驱动的 Control Task。
 * @author Codex
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_control.h"

#include "platform_time.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_bool_t app_control_is_source_valid(app_ctrl_source_t source)
{
    return ((source == APP_CTRL_SOURCE_BUTTON) ||
            (source == APP_CTRL_SOURCE_UART)) ? PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t app_control_send_queue(
    app_control_t *control,
    platform_queue_t *queue,
    const void *message)
{
    platform_error_t result = platform_queue_send(queue, message, PLATFORM_OS_NO_WAIT);

    if (result != PLATFORM_ERR_OK) {
        control->statistics.queueSubmitFailureCount++;
    }
    return result;
}

static platform_error_t app_control_send_acquisition(
    app_control_t *control,
    app_acquisition_command_t command)
{
    return app_control_send_queue(control, control->config.acquisitionQueue, &command);
}

static platform_error_t app_control_send_indicator(
    app_control_t *control,
    app_indicator_command_t command)
{
    return app_control_send_queue(control, control->config.indicatorQueue, &command);
}

static platform_error_t app_control_send_response(
    app_control_t *control,
    app_ctrl_source_t source,
    app_control_response_t response)
{
    app_communication_outbound_message_t message = {
        .type = APP_COMM_OUTBOUND_CONTROL_RESPONSE,
        .payload.controlResponse = response
    };

    if (source != APP_CTRL_SOURCE_UART) {
        return PLATFORM_ERR_OK;
    }
    return app_control_send_queue(control, control->config.communicationQueue, &message);
}

static platform_error_t app_control_keep_first_error(
    platform_error_t currentResult,
    platform_error_t newResult)
{
    return (currentResult == PLATFORM_ERR_OK) ? newResult : currentResult;
}

static platform_error_t app_control_handle_busy(
    app_control_t *control,
    app_ctrl_source_t source)
{
    if (source == APP_CTRL_SOURCE_BUTTON) {
        control->statistics.ignoredButtonEventCount++;
        return PLATFORM_ERR_OK;
    }
    return app_control_send_response(control, source, APP_CONTROL_RESPONSE_BUSY);
}

static platform_error_t app_control_handle_start(
    app_control_t *control,
    app_ctrl_source_t source)
{
    platform_error_t result;

    if (control->context.onceActive == PLATFORM_TRUE) {
        return app_control_handle_busy(control, source);
    }
    if (control->context.state == APP_CONTROL_STATE_RUNNING) {
        return app_control_send_response(
            control, source, APP_CONTROL_RESPONSE_ALREADY_RUNNING);
    }

    result = app_control_send_acquisition(
        control, APP_ACQUISITION_COMMAND_START_PERIODIC);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    control->context.state = APP_CONTROL_STATE_RUNNING;
    result = app_control_send_indicator(control, APP_INDICATOR_RUNNING);
    result = app_control_keep_first_error(
        result,
        app_control_send_response(control, source, APP_CONTROL_RESPONSE_OK_START));

    return result;
}

static platform_error_t app_control_handle_stop(
    app_control_t *control,
    app_ctrl_source_t source)
{
    platform_error_t result;

    if (control->context.onceActive == PLATFORM_TRUE) {
        return app_control_handle_busy(control, source);
    }
    if (control->context.state == APP_CONTROL_STATE_STOPPED) {
        return app_control_send_response(
            control, source, APP_CONTROL_RESPONSE_ALREADY_STOPPED);
    }

    result = app_control_send_acquisition(
        control, APP_ACQUISITION_COMMAND_STOP_PERIODIC);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    control->context.state = APP_CONTROL_STATE_STOPPED;
    result = app_control_send_indicator(control, APP_INDICATOR_STOPPED);
    result = app_control_keep_first_error(
        result,
        app_control_send_response(control, source, APP_CONTROL_RESPONSE_OK_STOP));

    return result;
}

static platform_error_t app_control_handle_sample_once(
    app_control_t *control,
    app_ctrl_source_t source)
{
    platform_error_t result;

    if (control->context.onceActive == PLATFORM_TRUE) {
        return app_control_handle_busy(control, source);
    }
    if (control->context.state == APP_CONTROL_STATE_RUNNING) {
        return app_control_send_response(
            control, source, APP_CONTROL_RESPONSE_ALREADY_RUNNING);
    }

    control->context.onceActive = PLATFORM_TRUE;
    control->context.onceSource = source;
    result = app_control_send_acquisition(control, APP_ACQUISITION_COMMAND_SAMPLE_ONCE);
    if (result != PLATFORM_ERR_OK) {
        control->context.onceActive = PLATFORM_FALSE;
    }

    return result;
}

static platform_error_t app_control_handle_status(
    app_control_t *control,
    app_ctrl_source_t source)
{
    app_control_response_t response = APP_CONTROL_RESPONSE_STATUS_STOPPED;

    if (control->context.state == APP_CONTROL_STATE_RUNNING) {
        response = APP_CONTROL_RESPONSE_STATUS_RUNNING;
    }
    return app_control_send_response(control, source, response);
}

static platform_error_t app_control_handle_acquisition_failure(
    app_control_t *control)
{
    app_ctrl_source_t source;

    if (control->context.onceActive != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    source = control->context.onceSource;
    control->context.onceActive = PLATFORM_FALSE;
    return app_control_send_response(
        control, source, APP_CONTROL_RESPONSE_ACQUISITION_FAILED);
}

static platform_error_t app_control_handle_tx_result(
    app_control_t *control,
    platform_error_t txResult)
{
    if (control->context.onceActive != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    control->context.onceActive = PLATFORM_FALSE;
    if (txResult != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_OK;
    }
    return app_control_send_indicator(control, APP_INDICATOR_ONCE_SUCCESS);
}

static platform_bool_t app_control_deadline_reached(uint32_t nowMs, uint32_t deadlineMs)
{
    return ((int32_t)(nowMs - deadlineMs) >= 0) ? PLATFORM_TRUE : PLATFORM_FALSE;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_control_init(
    app_control_t *control,
    const app_control_config_t *config)
{
    uint32_t nowMs = 0U;
    platform_error_t result;

    if ((control == NULL) || (config == NULL) ||
        (config->button == NULL) || (config->buttonService == NULL) ||
        (config->controlQueue == NULL) || (config->acquisitionQueue == NULL) ||
        (config->communicationQueue == NULL) || (config->indicatorQueue == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (control->context.initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if ((config->button->initialized != PLATFORM_TRUE) ||
        (config->buttonService->initialized != PLATFORM_TRUE) ||
        (config->controlQueue->native == NULL) ||
        (config->acquisitionQueue->native == NULL) ||
        (config->communicationQueue->native == NULL) ||
        (config->indicatorQueue->native == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    control->config = *config;
    control->context.state = APP_CONTROL_STATE_STOPPED;
    control->context.onceActive = PLATFORM_FALSE;
    control->context.onceSource = APP_CTRL_SOURCE_BUTTON;
    control->context.nextButtonSampleDeadlineMs =
        nowMs + PROJECT_BUTTON_SAMPLE_PERIOD_MS;
    control->context.initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t app_control_process_event(
    app_control_t *control,
    app_ctrl_event_t event,
    app_ctrl_source_t source)
{
    if (control == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (control->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    if ((event >= APP_CTRL_EVENT_MAX) ||
        (app_control_is_source_valid(source) != PLATFORM_TRUE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    control->statistics.processedEventCount++;
    switch (event) {
        case APP_CTRL_START:
            return app_control_handle_start(control, source);

        case APP_CTRL_STOP:
            return app_control_handle_stop(control, source);

        case APP_CTRL_SAMPLE_ONCE:
            return app_control_handle_sample_once(control, source);

        case APP_CTRL_GET_STATUS:
            return app_control_handle_status(control, source);

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

platform_error_t app_control_process_message(
    app_control_t *control,
    const app_control_message_t *message)
{
    if ((control == NULL) || (message == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (control->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    if (message->type >= APP_CONTROL_MESSAGE_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    control->statistics.processedMessageCount++;
    switch (message->type) {
        case APP_CONTROL_MESSAGE_CONTROL_REQUEST:
            return app_control_process_event(
                control,
                message->payload.request.event,
                message->payload.request.source);

        case APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED:
            return app_control_handle_acquisition_failure(control);

        case APP_CONTROL_MESSAGE_ONCE_TX_RESULT:
            return app_control_handle_tx_result(control, message->payload.result);

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

platform_error_t app_control_sample_button(app_control_t *control, uint32_t nowMs)
{
    platform_button_state_t buttonState = PLATFORM_BUTTON_STATE_RELEASED;
    service_button_event_t buttonEvent = SERVICE_BUTTON_EVENT_NONE;
    platform_error_t result;

    if (control == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (control->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_button_read(control->config.button, &buttonState);
    if (result != PLATFORM_ERR_OK) {
        control->statistics.buttonReadFailureCount++;
        return result;
    }
    result = service_button_process(
        control->config.buttonService, buttonState, nowMs, &buttonEvent);
    if (result != PLATFORM_ERR_OK) {
        control->statistics.buttonProcessFailureCount++;
        return result;
    }

    switch (buttonEvent) {
        case SERVICE_BUTTON_EVENT_NONE:
            return PLATFORM_ERR_OK;

        case SERVICE_BUTTON_EVENT_SINGLE:
            return app_control_process_event(
                control, APP_CTRL_START, APP_CTRL_SOURCE_BUTTON);

        case SERVICE_BUTTON_EVENT_DOUBLE:
            return app_control_process_event(
                control, APP_CTRL_SAMPLE_ONCE, APP_CTRL_SOURCE_BUTTON);

        case SERVICE_BUTTON_EVENT_LONG:
            return app_control_process_event(
                control, APP_CTRL_STOP, APP_CTRL_SOURCE_BUTTON);

        default:
            control->statistics.buttonProcessFailureCount++;
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

platform_error_t app_control_run_once(app_control_t *control)
{
    app_control_message_t message = {0};
    uint32_t nowMs = 0U;
    uint32_t timeoutMs;
    uint32_t elapsedPeriods;
    platform_error_t result;

    if (control == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (control->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    timeoutMs = app_control_deadline_reached(
        nowMs, control->context.nextButtonSampleDeadlineMs) == PLATFORM_TRUE ?
        PLATFORM_OS_NO_WAIT :
        (uint32_t)(control->context.nextButtonSampleDeadlineMs - nowMs);

    result = platform_queue_receive(control->config.controlQueue, &message, timeoutMs);
    if (result == PLATFORM_ERR_OK) {
        result = app_control_process_message(control, &message);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    } else if ((result != PLATFORM_ERR_TIMEOUT) && (result != PLATFORM_ERR_EMPTY)) {
        return result;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (app_control_deadline_reached(
            nowMs, control->context.nextButtonSampleDeadlineMs) != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = app_control_sample_button(control, nowMs);
    elapsedPeriods = ((uint32_t)(nowMs - control->context.nextButtonSampleDeadlineMs) /
                      PROJECT_BUTTON_SAMPLE_PERIOD_MS) + 1U;
    control->context.nextButtonSampleDeadlineMs +=
        elapsedPeriods * PROJECT_BUTTON_SAMPLE_PERIOD_MS;

    return result;
}

void app_control_task_entry(void *argument)
{
    app_control_t *control = (app_control_t *)argument;

    if (control == NULL) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    for (;;) {
        if (app_control_run_once(control) != PLATFORM_ERR_OK) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }
}
//******************************** Functions *********************************//
