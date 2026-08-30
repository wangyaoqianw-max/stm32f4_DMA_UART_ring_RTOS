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
//******************************** Includes *********************************//

//******************************** Functions *********************************//
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

    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
