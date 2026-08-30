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

typedef struct
{
    uint32_t initCallCount;
    uint32_t startCallCount;
    uint32_t stopCallCount;
    uint32_t serviceStartCallCount;
    platform_error_t initResult;
    platform_error_t startResult;
    platform_error_t stopResult;
    platform_error_t serviceStartResult;
} fake_runtime_t;

static fake_runtime_t g_fakeRuntime;

static void fake_runtime_reset(void)
{
    g_fakeRuntime = (fake_runtime_t){0};
    g_fakeRuntime.initResult = PLATFORM_ERR_OK;
    g_fakeRuntime.startResult = PLATFORM_ERR_OK;
    g_fakeRuntime.stopResult = PLATFORM_ERR_OK;
    g_fakeRuntime.serviceStartResult = PLATFORM_ERR_OK;
}

static platform_error_t fake_uart_lifecycle_init(void *self)
{
    (void)self;
    g_fakeRuntime.initCallCount++;
    return g_fakeRuntime.initResult;
}

static platform_error_t fake_uart_lifecycle_start(void *self)
{
    (void)self;
    g_fakeRuntime.startCallCount++;
    return g_fakeRuntime.startResult;
}

static platform_error_t fake_uart_lifecycle_stop(void *self)
{
    (void)self;
    g_fakeRuntime.stopCallCount++;
    return g_fakeRuntime.stopResult;
}

platform_error_t service_uart_start(service_uart_t *service)
{
    (void)service;
    g_fakeRuntime.serviceStartCallCount++;
    return g_fakeRuntime.serviceStartResult;
}

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

static int test_start_runs_uart_then_service(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_RUNNING == communication.context.state);

    return 0;
}

static int test_start_failure_stops_following_operations(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();
    g_fakeRuntime.initResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_IO == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(0U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_ERROR == communication.context.state);
    TEST_ASSERT(PLATFORM_ERR_IO == communication.context.lastError);
    TEST_ASSERT(1U == communication.statistics.fatalErrorCount);

    return 0;
}

static int test_service_start_failure_rolls_back_uart(void)
{
    app_communication_t communication = APP_COMMUNICATION_INITIALIZER;
    platform_uart_t uart = PLATFORM_UART_INITIALIZER;
    service_uart_t service = SERVICE_UART_INITIALIZER;
    app_communication_config_t config = {&uart, &service};
    platform_lifecycle_ops_t lifecycle = {0};

    lifecycle.init = fake_uart_lifecycle_init;
    lifecycle.start = fake_uart_lifecycle_start;
    lifecycle.stop = fake_uart_lifecycle_stop;
    uart.device.lifecycle = &lifecycle;
    fake_runtime_reset();
    g_fakeRuntime.serviceStartResult = PLATFORM_ERR_IO;

    TEST_ASSERT(PLATFORM_ERR_OK == app_communication_init(&communication, &config));
    TEST_ASSERT(PLATFORM_ERR_IO == app_communication_start(&communication));
    TEST_ASSERT(1U == g_fakeRuntime.initCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.startCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.serviceStartCallCount);
    TEST_ASSERT(1U == g_fakeRuntime.stopCallCount);
    TEST_ASSERT(APP_COMMUNICATION_STATE_ERROR == communication.context.state);

    return 0;
}

int main(void)
{
    int result = test_init_and_getters();

    if (0 != result) {
        return result;
    }

    result = test_start_runs_uart_then_service();
    if (0 != result) {
        return result;
    }

    result = test_start_failure_stops_following_operations();
    if (0 != result) {
        return result;
    }

    return test_service_start_failure_rolls_back_uart();
}
