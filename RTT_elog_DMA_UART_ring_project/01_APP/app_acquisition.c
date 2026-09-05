/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_acquisition.c
 * @brief 实现绝对周期采集、STOP 抑制和 ONCE 结果发布。
 * @author YaoQian Wang
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_acquisition.h"

#include "platform_time.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
/** @brief 使用有符号差值判断可回绕的毫秒 deadline。 */
static platform_bool_t app_acquisition_deadline_reached(
    uint32_t nowMs,
    uint32_t deadlineMs)
{
    return ((int32_t)(nowMs - deadlineMs) >= 0) ? PLATFORM_TRUE : PLATFORM_FALSE;
}

/** @brief 将 Service 层双传感器快照复制到 APP IPC 数据。 */
static void app_acquisition_copy_data(
    app_acquisition_data_t *destination,
    const service_acquisition_data_t *source)
{
    destination->environment = source->environment;
    destination->motion = source->motion;
}

/** @brief 以非阻塞方式投递 Queue，并统一累计投递失败统计。 */
static platform_error_t app_acquisition_send_queue(
    app_acquisition_t *acquisition,
    platform_queue_t *queue,
    const void *message)
{
    platform_error_t result = platform_queue_send(queue, message, PLATFORM_OS_NO_WAIT);

    if (result != PLATFORM_ERR_OK) {
        acquisition->statistics.queueSubmitFailureCount++;
    }
    return result;
}

/** @brief 向 Control FSM 回传 ONCE 失败或发送完成结果。 */
static platform_error_t app_acquisition_send_once_failure(
    app_acquisition_t *acquisition,
    app_control_message_type_t type,
    platform_error_t error)
{
    app_control_message_t message = {
        .type = type,
        .payload.result = error
    };

    return app_acquisition_send_queue(
        acquisition, acquisition->config.controlQueue, &message);
}

/** @brief 将完整采集结果按指定类型发布到通信 Queue。 */
static platform_error_t app_acquisition_publish(
    app_acquisition_t *acquisition,
    app_communication_outbound_type_t type,
    const service_acquisition_data_t *data)
{
    app_communication_outbound_message_t message = {
        .type = type
    };

    app_acquisition_copy_data(&message.payload.acquisition, data);
    return app_acquisition_send_queue(
        acquisition, acquisition->config.communicationQueue, &message);
}

/** @brief 采样完成后排空待处理命令并优先识别 STOP。 */
static platform_error_t app_acquisition_check_pending_stop(
    app_acquisition_t *acquisition,
    platform_bool_t *stopObserved)
{
    app_acquisition_command_t command = APP_ACQUISITION_COMMAND_MAX;
    platform_error_t result;

    *stopObserved = PLATFORM_FALSE;
    for (;;) {
        result = platform_queue_receive(
            acquisition->config.commandQueue, &command, PLATFORM_OS_NO_WAIT);
        if ((result == PLATFORM_ERR_EMPTY) || (result == PLATFORM_ERR_TIMEOUT)) {
            return PLATFORM_ERR_OK;
        }
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
        if (command == APP_ACQUISITION_COMMAND_STOP_PERIODIC) {
            acquisition->context.periodicEnabled = PLATFORM_FALSE;
            *stopObserved = PLATFORM_TRUE;
            return PLATFORM_ERR_OK;
        }
        /* Control FSM 在 RUNNING 下不会产生 SAMPLE_ONCE；重复 START 无需动作。 */
    }
}

/** @brief 执行一次周期采集，并抑制 STOP 后的过期结果。 */
static platform_error_t app_acquisition_execute_periodic(
    app_acquisition_t *acquisition)
{
    service_acquisition_data_t data = {0};
    platform_bool_t stopObserved = PLATFORM_FALSE;
    platform_error_t sampleResult;
    platform_error_t result;

    acquisition->statistics.periodicSampleCount++;
    sampleResult = service_acquisition_sample(acquisition->config.service, &data);
    if (sampleResult != PLATFORM_ERR_OK) {
        acquisition->statistics.sampleFailureCount++;
    }

    result = app_acquisition_check_pending_stop(acquisition, &stopObserved);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (stopObserved == PLATFORM_TRUE) {
        if (sampleResult == PLATFORM_ERR_OK) {
            acquisition->statistics.stalePeriodicDiscardCount++;
        }
        return PLATFORM_ERR_OK;
    }
    if (sampleResult != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_OK;
    }

    result = app_acquisition_publish(
        acquisition, APP_COMM_OUTBOUND_PERIODIC_REPORT, &data);
    if (result == PLATFORM_ERR_OK) {
        acquisition->statistics.periodicPublishCount++;
    }
    return PLATFORM_ERR_OK;
}

/** @brief 执行一次 ONCE 采集并保证向 Control 回传完成路径。 */
static platform_error_t app_acquisition_execute_once(app_acquisition_t *acquisition)
{
    service_acquisition_data_t data = {0};
    platform_error_t result;

    acquisition->statistics.onceSampleCount++;
    result = service_acquisition_sample(acquisition->config.service, &data);
    if (result != PLATFORM_ERR_OK) {
        acquisition->statistics.sampleFailureCount++;
        (void)app_acquisition_send_once_failure(
            acquisition,
            APP_CONTROL_MESSAGE_ONCE_ACQUISITION_FAILED,
            result);
        return PLATFORM_ERR_OK;
    }

    result = app_acquisition_publish(acquisition, APP_COMM_OUTBOUND_ONCE_REPORT, &data);
    if (result != PLATFORM_ERR_OK) {
        (void)app_acquisition_send_once_failure(
            acquisition,
            APP_CONTROL_MESSAGE_ONCE_TX_RESULT,
            result);
        return PLATFORM_ERR_OK;
    }
    acquisition->statistics.oncePublishCount++;

    return PLATFORM_ERR_OK;
}

/** @brief 处理一条 Acquisition 命令并维护周期运行状态。 */
static platform_error_t app_acquisition_handle_command(
    app_acquisition_t *acquisition,
    app_acquisition_command_t command,
    uint32_t nowMs)
{
    switch (command) {
        case APP_ACQUISITION_COMMAND_START_PERIODIC:
            if (acquisition->context.periodicEnabled == PLATFORM_TRUE) {
                return PLATFORM_ERR_OK;
            }
            acquisition->context.periodicEnabled = PLATFORM_TRUE;
            acquisition->context.nextSampleDeadlineMs =
                nowMs + PROJECT_ACQUISITION_PERIOD_MS;
            return app_acquisition_execute_periodic(acquisition);

        case APP_ACQUISITION_COMMAND_STOP_PERIODIC:
            acquisition->context.periodicEnabled = PLATFORM_FALSE;
            return PLATFORM_ERR_OK;

        case APP_ACQUISITION_COMMAND_SAMPLE_ONCE:
            if (acquisition->context.periodicEnabled == PLATFORM_TRUE) {
                return PLATFORM_ERR_INVALID_STATE;
            }
            return app_acquisition_execute_once(acquisition);

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

/** @brief 超期时推进绝对 deadline，并统计跳过的历史周期。 */
static platform_error_t app_acquisition_advance_deadline(
    app_acquisition_t *acquisition)
{
    uint32_t nowMs = 0U;
    uint32_t skippedPeriods;
    platform_error_t result = platform_time_get_ms(&nowMs);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (acquisition->context.periodicEnabled != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }
    if (app_acquisition_deadline_reached(
            nowMs, acquisition->context.nextSampleDeadlineMs) != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    skippedPeriods = ((uint32_t)(nowMs - acquisition->context.nextSampleDeadlineMs) /
                      PROJECT_ACQUISITION_PERIOD_MS) + 1U;
    acquisition->context.nextSampleDeadlineMs +=
        skippedPeriods * PROJECT_ACQUISITION_PERIOD_MS;
    acquisition->statistics.skippedPeriodCount += skippedPeriods;

    return PLATFORM_ERR_OK;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_acquisition_init(
    app_acquisition_t *acquisition,
    const app_acquisition_config_t *config)
{
    if ((acquisition == NULL) || (config == NULL) ||
        (config->service == NULL) || (config->commandQueue == NULL) ||
        (config->communicationQueue == NULL) || (config->controlQueue == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (acquisition->context.initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }
    if ((config->service->initialized != PLATFORM_TRUE) ||
        (config->commandQueue->native == NULL) ||
        (config->communicationQueue->native == NULL) ||
        (config->controlQueue->native == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    acquisition->config = *config;
    acquisition->context.periodicEnabled = PLATFORM_FALSE;
    acquisition->context.initialized = PLATFORM_TRUE;

    return PLATFORM_ERR_OK;
}

platform_error_t app_acquisition_run_once(app_acquisition_t *acquisition)
{
    app_acquisition_command_t command = APP_ACQUISITION_COMMAND_MAX;
    uint32_t nowMs = 0U;
    uint32_t timeoutMs = PLATFORM_OS_WAIT_FOREVER;
    platform_error_t result;

    if (acquisition == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }
    if (acquisition->context.initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (acquisition->context.periodicEnabled == PLATFORM_TRUE) {
        timeoutMs = app_acquisition_deadline_reached(
            nowMs, acquisition->context.nextSampleDeadlineMs) == PLATFORM_TRUE ?
            PLATFORM_OS_NO_WAIT :
            (uint32_t)(acquisition->context.nextSampleDeadlineMs - nowMs);
    }

    result = platform_queue_receive(acquisition->config.commandQueue, &command, timeoutMs);
    if (result == PLATFORM_ERR_OK) {
        return app_acquisition_handle_command(acquisition, command, nowMs);
    }
    if ((result != PLATFORM_ERR_TIMEOUT) && (result != PLATFORM_ERR_EMPTY)) {
        return result;
    }
    if (acquisition->context.periodicEnabled != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if (app_acquisition_deadline_reached(
            nowMs, acquisition->context.nextSampleDeadlineMs) != PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    acquisition->context.nextSampleDeadlineMs += PROJECT_ACQUISITION_PERIOD_MS;
    result = app_acquisition_execute_periodic(acquisition);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    return app_acquisition_advance_deadline(acquisition);
}

void app_acquisition_task_entry(void *argument)
{
    app_acquisition_t *acquisition = (app_acquisition_t *)argument;

    if (acquisition == NULL) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    for (;;) {
        if (app_acquisition_run_once(acquisition) != PLATFORM_ERR_OK) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }
}
//******************************** Functions *********************************//
