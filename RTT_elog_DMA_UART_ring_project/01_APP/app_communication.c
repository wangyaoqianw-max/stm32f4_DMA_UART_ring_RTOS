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

#include "app_communication.h"

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config)
{
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
    if ((NULL == communication) || (NULL == statistics)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (APP_COMMUNICATION_STATE_UNINITIALIZED == communication->context.state) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    *statistics = communication->statistics;

    return PLATFORM_ERR_OK;
}
