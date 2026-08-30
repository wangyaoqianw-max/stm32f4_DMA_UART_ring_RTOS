/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_communication.c
 * @brief 实现通信 APP 的对象初始化与可观测性接口。
 * @author YaoQian Wang
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_communication.h"
#include "project_config.h"
#include "platform_log.h"
#include "platform_time.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LOG_TAG                                "app_comm"
//******************************** Defines **********************************//

//******************************** Functions *********************************//
static platform_error_t app_communication_set_error(
    app_communication_t *communication,
    platform_error_t error)
{
    communication->context.state = APP_COMMUNICATION_STATE_ERROR;
    communication->context.lastError = error;
    communication->statistics.fatalErrorCount++;

    return error;
}

static platform_error_t app_communication_drain_rx(app_communication_t *communication)
{
    uint8_t buffer[PROJECT_COMM_READ_BUFFER_SIZE] = {0};
    platform_error_t result = PLATFORM_ERR_OK;
    platform_size_t readLength = 0U;

    for (;;) {
        result = service_uart_read(communication->config.service,
                                   buffer,
                                   sizeof(buffer),
                                   &readLength);
        if (PLATFORM_ERR_EMPTY == result) {
            return PLATFORM_ERR_OK;
        }

        if (PLATFORM_ERR_OK != result) {
            return result;
        }

        communication->statistics.processedChunkCount++;
        communication->statistics.processedByteCount += readLength;
    }
}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config)
{
    /* 依赖对象必须在复制前有效，避免后续启动阶段发生空指针解引用。 */
    if ((NULL == communication) || (NULL == config) || (NULL == config->uart) ||
        (NULL == config->service)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED != communication->context.state) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    communication->config = *config;
    communication->context.state = APP_COMMUNICATION_STATE_INITIALIZED;
    communication->context.lastError = PLATFORM_ERR_OK;
    communication->statistics = (app_communication_statistics_t){0};

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_get_status(
    const app_communication_t *communication,
    app_communication_status_t *status)
{
    /* 未初始化对象不向调用者暴露零初始化存储的伪状态。 */
    if ((NULL == communication) || (NULL == status)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED == communication->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    status->state = communication->context.state;
    status->lastError = communication->context.lastError;

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_get_statistics(
    const app_communication_t *communication,
    app_communication_statistics_t *statistics)
{
    /* 统计仅通过快照返回，调用者不能修改 APP 内部累计值。 */
    if ((NULL == communication) || (NULL == statistics)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED == communication->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    *statistics = communication->statistics;

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_start(app_communication_t *communication)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (NULL == communication) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED == communication->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (APP_COMMUNICATION_STATE_INITIALIZED != communication->context.state) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((NULL == communication->config.uart) || (NULL == communication->config.service) ||
        (NULL == communication->config.uart->device.lifecycle) ||
        (NULL == communication->config.uart->device.lifecycle->init) ||
        (NULL == communication->config.uart->device.lifecycle->start) ||
        (NULL == communication->config.uart->device.lifecycle->stop)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = communication->config.uart->device.lifecycle->init(communication->config.uart);
    if (PLATFORM_ERR_OK != result) {
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    result = communication->config.uart->device.lifecycle->start(communication->config.uart);
    if (PLATFORM_ERR_OK != result) {
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    result = service_uart_start(communication->config.service);
    if (PLATFORM_ERR_OK != result) {
        (void)communication->config.uart->device.lifecycle->stop(communication->config.uart);
        communication->context.state = APP_COMMUNICATION_STATE_ERROR;
        communication->context.lastError = result;
        communication->statistics.fatalErrorCount++;
        return result;
    }

    communication->context.state = APP_COMMUNICATION_STATE_RUNNING;
    communication->context.lastError = PLATFORM_ERR_OK;
    platform_log_i("communication runtime started");

    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_process(app_communication_t *communication, uint32_t timeoutMs)
{
    platform_error_t result = PLATFORM_ERR_OK;
    uint32_t events = 0U;
    service_uart_status_t serviceStatus = {0};

    if (NULL == communication) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED == communication->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (APP_COMMUNICATION_STATE_RUNNING != communication->context.state) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = service_uart_wait_event(communication->config.service, timeoutMs, &events);
    if (PLATFORM_ERR_TIMEOUT == result) {
        return PLATFORM_ERR_OK;
    }

    if (PLATFORM_ERR_OK != result) {
        return app_communication_set_error(communication, result);
    }

    if (0U != (events & SERVICE_UART_EVENT_RX_AVAILABLE)) {
        result = app_communication_drain_rx(communication);
        if (PLATFORM_ERR_OK != result) {
            return app_communication_set_error(communication, result);
        }
    }

    if (0U != (events & SERVICE_UART_EVENT_ERROR)) {
        result = service_uart_get_status(communication->config.service, &serviceStatus);
        if (PLATFORM_ERR_OK != result) {
            return app_communication_set_error(communication, result);
        }

        result = service_uart_start(communication->config.service);
        if (PLATFORM_ERR_OK != result) {
            return app_communication_set_error(communication, result);
        }

        communication->statistics.uartErrorRecoveryCount++;
        return PLATFORM_ERR_OK;
    }

    if (0U != (events & SERVICE_UART_EVENT_DATA_LOSS)) {
        result = service_uart_stop(communication->config.service);
        if (PLATFORM_ERR_OK != result) {
            result = service_uart_get_status(communication->config.service, &serviceStatus);
            if ((PLATFORM_ERR_OK != result) || (SERVICE_UART_STATE_STOPPED != serviceStatus.state)) {
                return app_communication_set_error(communication, result);
            }
        }

        result = service_uart_start(communication->config.service);
        if (PLATFORM_ERR_OK != result) {
            return app_communication_set_error(communication, result);
        }

        communication->statistics.dataLossRecoveryCount++;
        return PLATFORM_ERR_OK;
    }

    if (0U != (events & SERVICE_UART_EVENT_STOPPED)) {
        return app_communication_set_error(communication, PLATFORM_ERR_CANCELED);
    }

    return PLATFORM_ERR_OK;
}

void app_communication_task_entry(void *argument)
{
    app_communication_t *communication = (app_communication_t *)argument;
    platform_error_t result = PLATFORM_ERR_OK;

    if (NULL == communication) {
        for (;;) {
            (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
        }
    }

    result = app_communication_start(communication);
    while (PLATFORM_ERR_OK == result) {
        result = app_communication_process(communication, PROJECT_COMM_WAIT_TIMEOUT_MS);
    }

    for (;;) {
        platform_log_e("communication fatal error: %d", (int)communication->context.lastError);
        (void)platform_time_delay_ms(PROJECT_COMM_ERROR_IDLE_DELAY_MS);
    }
}
//******************************** Functions *********************************//
