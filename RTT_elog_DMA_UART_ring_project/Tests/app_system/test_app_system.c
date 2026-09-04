/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file test_app_system.c
 * @brief 验证 APP System Composition Root 装配顺序。
 * @author Codex
 * @date 2026-08-30
 * @version V1.0
 *
 *****************************************************************************/

#include "app_communication.h"
#include "app_system.h"
#include "platform_bsp_uart.h"
#include "platform_thread.h"
#include "service_log.h"
#include "service_uart.h"

#include <string.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            return __LINE__; \
        } \
    } while (0)

typedef struct
{
    uint32_t step;
    uint32_t bspStep;
    uint32_t appStep;
    uint32_t threadStep;
    uint32_t serviceStep;
    const service_uart_config_t *serviceConfig;
    uint32_t logCallCount;
    platform_log_level_t logLevel;
    const char *logTag;
    const char *logFormat;
} fake_system_t;

static fake_system_t g_fakeSystem;

static void fake_log_output(
    uint8_t level,
    const char *tag,
    const char *file,
    const char *func,
    long line,
    const char *format,
    ...)
{
    g_fakeSystem.logCallCount++;
    g_fakeSystem.logLevel = (platform_log_level_t)level;
    g_fakeSystem.logTag = tag;
    g_fakeSystem.logFormat = format;
    (void)file;
    (void)func;
    (void)line;
}

platform_log_output_fn_t platform_log_get_output_fn(void)
{
    return fake_log_output;
}

void app_communication_task_entry(void *argument)
{
    (void)argument;
}

platform_error_t platform_bsp_uart_construct_communication(
    platform_uart_t *uart,
    const platform_uart_config_t *config)
{
    (void)uart;
    TEST_ASSERT(115200U == config->baudRate);
    g_fakeSystem.bspStep = ++g_fakeSystem.step;
    return PLATFORM_ERR_OK;
}

platform_error_t app_communication_init(
    app_communication_t *communication,
    const app_communication_config_t *config)
{
    (void)communication;
    TEST_ASSERT(NULL != config->uart);
    TEST_ASSERT(NULL != config->service);
    g_fakeSystem.appStep = ++g_fakeSystem.step;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_thread_create(platform_thread_t *thread, const platform_thread_config_t *config)
{
    (void)thread;
    TEST_ASSERT(1024U == config->stackSizeBytes);
    TEST_ASSERT(PLATFORM_THREAD_PRIORITY_NORMAL == config->priority);
    g_fakeSystem.threadStep = ++g_fakeSystem.step;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_thread_terminate(platform_thread_t *thread)
{
    (void)thread;
    return PLATFORM_ERR_OK;
}

platform_error_t service_uart_init(service_uart_t *service, const service_uart_config_t *config)
{
    (void)service;
    g_fakeSystem.serviceStep = ++g_fakeSystem.step;
    g_fakeSystem.serviceConfig = config;
    return PLATFORM_ERR_OK;
}

int main(void)
{
    TEST_ASSERT(PLATFORM_ERR_OK == app_system_init());
    TEST_ASSERT(1U == g_fakeSystem.bspStep);
    TEST_ASSERT(2U == g_fakeSystem.appStep);
    TEST_ASSERT(3U == g_fakeSystem.threadStep);
    TEST_ASSERT(4U == g_fakeSystem.serviceStep);
    TEST_ASSERT(NULL != g_fakeSystem.serviceConfig->dmaRxBuffer);
    TEST_ASSERT(128U == g_fakeSystem.serviceConfig->dmaRxBufferSize);
    TEST_ASSERT(NULL != g_fakeSystem.serviceConfig->ringBufferStorage);
    TEST_ASSERT(512U == g_fakeSystem.serviceConfig->ringBufferStorageSize);
    TEST_ASSERT(NULL != g_fakeSystem.serviceConfig->ownerThread);
    TEST_ASSERT(1U == g_fakeSystem.logCallCount);
    TEST_ASSERT(PLATFORM_LOG_LEVEL_INFO == g_fakeSystem.logLevel);
    TEST_ASSERT(0 == strcmp("app_system", g_fakeSystem.logTag));
    TEST_ASSERT(0 == strcmp("system composition initialized", g_fakeSystem.logFormat));

    return 0;
}
