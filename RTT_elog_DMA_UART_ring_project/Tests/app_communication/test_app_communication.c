/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_communication.c
 * @brief 验证通信 APP 对象初始化和观测接口。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include "app_communication.h"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

static int test_init_and_getters(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service};
    app_communication_status_t status = {0};
    app_communication_statistics_t statistics = {0};

    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(NULL, &config));
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, NULL));
    config.uart = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, &config));
    config.uart = &uart;
    config.service = NULL;
    TEST_ASSERT(PLATFORM_ERR_INVALID_PARAM == app_communication_init(&communication, &config));
    config.service = &service;
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED == app_communication_get_status(&communication, &status));
    TEST_ASSERT(PLATFORM_ERR_NOT_INITIALIZED ==
                app_communication_get_statistics(&communication, &statistics));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(APP_COMMUNICATION_STATE_INITIALIZED == communication.context.state);
    TEST_ASSERT(&uart == communication.config.uart);
    TEST_ASSERT(&service == communication.config.service);
    TEST_ASSERT(PLATFORM_ERR_OK == communication.context.lastError);
    TEST_ASSERT(0U == communication.statistics.processedChunkCount);
    TEST_ASSERT(PLATFORM_ERR_ALREADY_INITIALIZED == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_get_status(&communication, &status));
    TEST_ASSERT(APP_COMMUNICATION_STATE_INITIALIZED == status.state);
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_get_statistics(&communication, &statistics));
    TEST_ASSERT(0U == statistics.fatalErrorCount);

    return 0;
}

int main(void)
{
    return test_init_and_getters();
}
