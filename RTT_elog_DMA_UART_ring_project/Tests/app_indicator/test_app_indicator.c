/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_indicator.c
 * @brief 验证 Indicator Task 的 Queue 与 Service 语义映射。
 * @author YaoQian Wang
 * @date 2026-09-04
 * @version V1.0
 *
 *****************************************************************************/

#include "app_indicator.h"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

static platform_queue_t g_indicatorQueue;
static app_indicator_command_t g_nextCommand;
static service_indicator_event_t g_lastEvent;
static uint32_t g_receiveTimeoutMs;
static uint32_t g_handleCallCount;

/** @brief 创建依赖已就绪的 Indicator Task 测试对象。 */
static app_indicator_t create_indicator(service_indicator_t *service)
{
    app_indicator_t indicator = APP_INDICATOR_INITIALIZER;
    app_indicator_config_t config = {
        .service = service,
        .queue = &g_indicatorQueue
    };

    service->initialized = PLATFORM_TRUE;
    g_indicatorQueue.native = &g_indicatorQueue;
    (void)app_indicator_init(&indicator, &config);
    return indicator;
}

/** @brief 验证 Indicator 初始化的依赖约束。 */
static int test_init_validates_dependencies(void)
{
    app_indicator_t indicator = APP_INDICATOR_INITIALIZER;
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    app_indicator_config_t config = {
        .service = &service,
        .queue = &g_indicatorQueue
    };

    g_indicatorQueue.native = &g_indicatorQueue;
    TEST_ASSERT(app_indicator_init(NULL, &config) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(app_indicator_init(&indicator, NULL) == PLATFORM_ERR_NULL_POINTER);
    TEST_ASSERT(app_indicator_init(&indicator, &config) == PLATFORM_ERR_NOT_INITIALIZED);
    service.initialized = PLATFORM_TRUE;
    TEST_ASSERT(app_indicator_init(&indicator, &config) == PLATFORM_ERR_OK);
    TEST_ASSERT(indicator.initialized == PLATFORM_TRUE);
    TEST_ASSERT(app_indicator_init(&indicator, &config) == PLATFORM_ERR_ALREADY_INITIALIZED);

    return 0;
}

/** @brief 验证全部 APP 指示命令映射到正确 Service 事件。 */
static int test_commands_map_to_indicator_service(void)
{
    service_indicator_t service = SERVICE_INDICATOR_INITIALIZER;
    app_indicator_t indicator = create_indicator(&service);

    g_handleCallCount = 0U;
    g_nextCommand = APP_INDICATOR_STOPPED;
    TEST_ASSERT(app_indicator_run_once(&indicator) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_receiveTimeoutMs == PLATFORM_OS_WAIT_FOREVER);
    TEST_ASSERT(g_lastEvent == SERVICE_INDICATOR_EVENT_STOPPED);

    g_nextCommand = APP_INDICATOR_RUNNING;
    TEST_ASSERT(app_indicator_run_once(&indicator) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_lastEvent == SERVICE_INDICATOR_EVENT_RUNNING);

    g_nextCommand = APP_INDICATOR_ONCE_SUCCESS;
    TEST_ASSERT(app_indicator_run_once(&indicator) == PLATFORM_ERR_OK);
    TEST_ASSERT(g_lastEvent == SERVICE_INDICATOR_EVENT_ONCE_SUCCESS);
    TEST_ASSERT(g_handleCallCount == 3U);

    return 0;
}

platform_error_t platform_queue_receive(
    platform_queue_t *queue,
    void *item,
    uint32_t timeoutMs)
{
    TEST_ASSERT(queue == &g_indicatorQueue);
    g_receiveTimeoutMs = timeoutMs;
    *(app_indicator_command_t *)item = g_nextCommand;
    return PLATFORM_ERR_OK;
}

platform_error_t service_indicator_handle_event(
    service_indicator_t *service,
    service_indicator_event_t event)
{
    (void)service;
    g_lastEvent = event;
    g_handleCallCount++;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_time_delay_ms(uint32_t delayMs)
{
    (void)delayMs;
    return PLATFORM_ERR_OK;
}

int main(void)
{
    int result = test_init_validates_dependencies();

    if (result != 0) {
        return result;
    }
    return test_commands_map_to_indicator_service();
}
